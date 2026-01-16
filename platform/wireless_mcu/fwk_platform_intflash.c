/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_platform_intflash.h"
#include "fsl_adapter_flash.h"
#include "fwk_platform_definitions.h"
#include "cmsis_compiler.h"
#include <setjmp.h>

#if defined gInterceptEccBusFaults_d && (gInterceptEccBusFaults_d > 0)
/* required for Nv_GetPartitionAddressAndSize */
#include "NVM_Interface.h"
#endif
/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

#define INT_FLASH_SECTOR_SZ_LOG2 13U
#define INT_FLASH_PHRASE_SZ_LOG2 4U
#define INT_FLASH_SECTOR_SIZE    (1UL << INT_FLASH_SECTOR_SZ_LOG2)
#define INT_FLASH_PHRASE_SIZE    (1UL << INT_FLASH_PHRASE_SZ_LOG2)

#undef ROUND_FLOOR
#define ROUND_FLOOR(_X_, _SHIFT_) ((((uint32_t)_X_) >> (_SHIFT_)) << (_SHIFT_))

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

#if defined      gInterceptEccBusFaults_d && (gInterceptEccBusFaults_d > 0)
void             BusFault_Handler(void);
__NO_RETURN void FaultRecovery(void);
#endif

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

uint32_t PLATFORM_SeekForEccFaultsInRange(uint32_t start_addr, uint32_t size)
{
    uint32_t ecc_fault_at = 0UL;
    uint32_t flash_addr   = ROUND_FLOOR(start_addr, INT_FLASH_PHRASE_SZ_LOG2);
    uint32_t end_addr     = flash_addr + size;
    for (; flash_addr < end_addr; flash_addr += FSL_FEATURE_FLASH_PFLASH_PHRASE_SIZE)
    {
        uint32_t read_val;
        /* Dummy read just to probe ECC error */
        if (HAL_FlashReadCheckEccFaults(flash_addr, sizeof(uint32_t), (uint8_t *)&read_val) ==
            kStatus_HAL_Flash_EccError)
        {
            ecc_fault_at = flash_addr;
            break;
        }
    }
    return ecc_fault_at;
}

void PLATFORM_EnableBusFault(void)
{
    /* In the absence of a proper setting in SHCSR, all faults lead to a HardFault
     * Since we want a finer discrimination for Bus Faults in order to catch Faults when
     * accessing the Flash on pages in error, lets set BUSFAULTENA.
     */
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
}

/*
 *  Solution consisting in erasing only the sector containing the fault relies on a
 * proper handling of erased sector when mounting the NV storage firmware : NVM, NVS or
 * LittleFS
 */
WEAK void PLATFORM_TreatEccFault(uint32_t ecc_fault_addr)
{
/* alternatively may erase just the only sector containing the fault, or the whole NV storage */
#if 1
    uint32_t sector_addr = ROUND_FLOOR(ecc_fault_addr, INT_FLASH_SECTOR_SZ_LOG2);
    (void)HAL_FlashEraseSector(sector_addr, INT_FLASH_SECTOR_SIZE);
#else
    (void)HAL_FlashEraseSector((uint32_t)NV_STORAGE_START_ADDRESS, NVM_LENGTH);
#endif
}

#if defined gInterceptEccBusFaults_d && (gInterceptEccBusFaults_d > 0)

__attribute__((section(".after_vectors"))) void BusFault_Handler(void)
{
    /* If we want to catch the ECC fault without undergoing the reset and implement */
    FaultRecovery();
}

__NO_RETURN void FaultRecovery(void)
{
    /* Detect whether Bus Fault is caused by ECC fault */
    if (HAL_FlashEccStatusRaised())
    {
        uint32_t bfar = SCB->BFAR;
        uint32_t nv_storage_start;
        uint32_t nvm_length;
        Nv_GetPartitionAddressAndSize(&nv_storage_start, &nvm_length);
#if defined FSL_FEATURE_FLASH_PFLASH_START_ADDRESS && (FSL_FEATURE_FLASH_PFLASH_START_ADDRESS > 0)
        if (nv_storage_start >= FSL_FEATURE_FLASH_PFLASH_START_ADDRESS)
#endif
        {
            /* Check is the Bus Fault Address is within NV storage range */
            if (bfar >= nv_storage_start)
            {
                uint32_t end_addr;

                if ((nvm_length < FSL_FEATURE_FLASH_PFLASH_BLOCK_SIZE) &&
                    (nv_storage_start < (FSL_FEATURE_FLASH_PFLASH_BLOCK_SIZE - nvm_length)))
                {
                    end_addr = nv_storage_start + nvm_length;
                    if (bfar < end_addr)
                    {
                        /* We have now ascertained that the ECC fault raised within the NV storage area.
                         * The brutal solution, relies on the fact that when mounting the partition, NVS, or LittleFs
                         * will detect the missing sector and reformat if required */
                        PLATFORM_TreatEccFault(bfar);
                    }
                }
            }
        }
    }
#ifdef UNITY_DUMP_RESULT
    extern jmp_buf g_test_ctx;
    longjmp(g_test_ctx, 1); /* Jump back to the setjmp point */
#else
    /* Potentially add other handlers for other BusFault causes */
    /* Only a suggestion to reset. Alternate possibilities exist, such as log some information to notify
     * application of the loss  */
    NVIC_SystemReset();
#endif
}
#endif
