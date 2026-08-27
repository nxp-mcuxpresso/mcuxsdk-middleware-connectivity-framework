/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include <stdbool.h>

#include "fwk_platform_devmgr.h"
#include "FunctionLib.h"
#include "fsl_rom_api.h"

/**********************************************************************************
 *************************************************************************************
 * Private variables
 *************************************************************************************
 ********************************************************************************** */
static flash_config_t s_flashInstance;
static bool           s_flash_init_done = false;

/**********************************************************************************
 *************************************************************************************
 * Private functions
 *************************************************************************************
 *
 * MCXW23 reads a factory-provisioned BD address through the ROM FFR path. The
 * three FFR locations map to the three explicit resolution steps sequenced by
 * the platform/CFPA backend wrapper DEVMGR_GetBdAddress:
 *
 *   customer store -> kFFR_BdAddrLocationCmpa  (PLATFORM_DEVMGR_GetBdAddressCust)
 *   NMPA / IFR      -> kFFR_BdAddrLocationNmpa  (PLATFORM_DEVMGR_GetBdAddrFromNMPA)
 *   UID-derived     -> kFFR_BdAddrLocationUuid  (PLATFORM_DEVMGR_GetBdAddrFromUID)
 *
 * MCXW23 carries no IEEE 802.15.4 MAC address, so all 802.15.4 reads report
 * DEVMGR_NotSupported_c.
 *
 ********************************************************************************** */

/*! *********************************************************************************
 * \brief  Read the BD address from a single FFR location.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 * \param[in]  location  one of the kFFR_BdAddrLocation* single-location values
 *
 * \return DEVMGR_Success_c when the location holds a non-blank address,
 *      DEVMGR_Blank_c when it is unprovisioned/absent, DEVMGR_Error_c on a NULL
 *      pointer or flash-init failure.
 ********************************************************************************** */
static int s_PLATFORM_DEVMGR_GetBdAddrFromLocation(uint8_t *pOutAddr, ffr_bd_addr_location_t location)
{
    int status = DEVMGR_Error_c;

    do
    {
        status_t flashStatus;
        uint32_t searchLocation = location;

        if (pOutAddr == NULL)
        {
            break;
        }

        FLib_MemSet(pOutAddr, 0xFFU, DEVMGR_BD_ADDR_SZ);
        if (!s_flash_init_done)
        {
            flashStatus = FLASH_Init(&s_flashInstance);
            if (flashStatus != kStatus_Success)
            {
                break;
            }
            s_flash_init_done = true;
        }

        flashStatus = FFR_GetBdAddress(&s_flashInstance, pOutAddr, &searchLocation);
        if (flashStatus != kStatus_Success)
        {
            /* No address available at this location. */
            status = DEVMGR_Blank_c;
            break;
        }

        /* An all-0xFF address means the field is unprovisioned (blank). */
        if (FLib_MemCmpToVal((const void *)pOutAddr, 0xFFU, DEVMGR_BD_ADDR_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }

        status = DEVMGR_Success_c;
    } while (false);

    return status;
}

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */

int PLATFORM_DEVMGR_GetBdAddressCust(uint8_t *pOutAddr)
{
    /* Customer store on MCXW23 is the CMPA FFR location. */
    return s_PLATFORM_DEVMGR_GetBdAddrFromLocation(pOutAddr, kFFR_BdAddrLocationCmpa);
}

int PLATFORM_DEVMGR_GetBdAddrFromNMPA(uint8_t *pOutAddr)
{
    return s_PLATFORM_DEVMGR_GetBdAddrFromLocation(pOutAddr, kFFR_BdAddrLocationNmpa);
}

int PLATFORM_DEVMGR_GetBdAddrFromUID(uint8_t *pOutAddr)
{
    return s_PLATFORM_DEVMGR_GetBdAddrFromLocation(pOutAddr, kFFR_BdAddrLocationUuid);
}

int PLATFORM_DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    (void)pAddr;
    /* The FFR BD address region is read-only from the DevMgr platform layer. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get154AddressCust(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Set154Address(const uint8_t *pAddr)
{
    (void)pAddr;
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get154AddrFromNMPA(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    /* MCXW23 carries no IEEE 802.15.4 MAC address in NMPA. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get154AddrFromUID(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    /* No 802.15.4 UID-derived address is provided on MCXW23. */
    return DEVMGR_NotSupported_c;
}
