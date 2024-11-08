/*
 * Copyright 2022-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_PLAT_DEFS_H_
#define _FWK_PLAT_DEFS_H_

#include "fsl_device_registers.h"

#define FWK_KW47_MCXW72_FAMILIES 1

#ifndef KB
#define KB(x) (((uint32_t)x) << 10u)
#endif
#ifndef MB
#define MB(x) (((uint32_t)x) << 20u)
#endif

#define gPlatformFlashStartAddress_c (FSL_FEATURE_FLASH_PFLASH_START_ADDRESS)
#define gPlatformFlashEndAddress_c   (FSL_FEATURE_FLASH_PFLASH_BLOCK_SIZE)

/*
 * External Flash geometry characteristics. SPI NOR Flash Part is AT25XE161D (16Mb)
 */
#define PLATFORM_EXTFLASH_SECTOR_SIZE KB(4U)
#define PLATFORM_EXTFLASH_PAGE_SIZE   256U
#define PLATFORM_EXTFLASH_TOTAL_SIZE  MB(2U)

#define PLATFORM_INTFLASH_SECTOR_SIZE FSL_FEATURE_FLASH_PFLASH_SECTOR_SIZE

/* IFR has 4 sectors */
#define IFR_SECT_ROMCFG  0u /* ROM Bootload configurations */
#define IFR_SECT_USER    1u /* Reserved for customer usage */
#define IFR_SECT_CMAC    2u /* Reserved CMAC */
#define IFR_SECT_OTA_CFG 3u /* OTACFG */
#ifndef FSL_FEATURE_IFR0_START_ADDRESS
#define FSL_FEATURE_IFR0_START_ADDRESS (0x02000000U)
#endif

#define IFR_SECTOR_SIZE FSL_FEATURE_FLASH_PFLASH_SECTOR_SIZE

#define IFR0_BASE FSL_FEATURE_IFR0_START_ADDRESS
//#define IFR1_BASE                     FSL_FEATURE_FLASH_BLOCK0_IFR1_START

#define IFR_SECTOR_OFFSET(n) ((n)*IFR_SECTOR_SIZE)

/* OTA CFG is in sector 3 of IFR */
#define IFR_OTA_CFG_ADDR (IFR0_BASE + IFR_SECTOR_OFFSET(IFR_SECT_OTA_CFG))
#define IFR_USER_ADDR    (IFR0_BASE + IFR_SECTOR_OFFSET(IFR_SECT_USER))
#define IFR_W_ONCE_DATA  (IFR0_BASE + IFR_SECTOR_OFFSET(IFR_SECT_ROMCFG) + 0x1000)

#define APP_FACTORY_DATA_MAX_LEN 0x800U
#define PROD_DATA_LEN            0x80U

#define PROD_DATA_OFFSET        0U
#define APP_FACTORY_DATA_OFFSET (PROD_DATA_OFFSET + PROD_DATA_LEN)

#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
extern uint32_t Image$$PROD_DATA_REGION$$Base;
#define PROD_DATA_BASE_ADDR &Image$$PROD_DATA_REGION$$Base
#else
extern uint32_t PROD_DATA_BASE_ADDR[];
#endif /* defined(__CC_ARM) */

#define MAIN_FLASH_PROD_DATA_ADDR ((uint32_t)PROD_DATA_BASE_ADDR)

/* Connected MCU uses NOR Flash driver not mflash. NV storage is by default placed in internal flash */
/* If one means to place NVS instance in external flash, need to define NV_STORAGE_EXTFLASH_START_OFFSET at the right
 * offset in
 * external flash. It could be done using NV_STORAGE_START_ADDRESS provided that it is defined accordingly in linker
 * script */
#ifdef NV_STORAGE_EXTFLASH_START_OFFSET
#define NV_EXTFLASH_OFFSET(offset) (NV_EXTFLASH_PHYS_ADDR(offset) - NV_STORAGE_EXTFLASH_START_OFFSET)
#define NV_STORAGE_START_OFFSET    NV_STORAGE_EXTFLASH_START_OFFSET
#else
/*
 *  NV_STORAGE_START_ADDRESS is defined by linker script. Dwells in external flash  for RW61x
 */
#define NV_STORAGE_INTFLASH_START_OFFSET ((uint32_t)NV_STORAGE_START_ADDRESS - gPlatformFlashStartAddress_c)
#define NV_STORAGE_START_OFFSET          NV_STORAGE_INTFLASH_START_OFFSET
#endif

/*********************************************************************
 *        RAM settings
 *********************************************************************/

/*! Enable/Disable shutdown of ECC RAM banks during low power period like Deep Sleep or Power Down
 *  Shutting down ECC RAM banks allows to save about 1uA
 *  The RAM banks can be selectively reinitialized by calling MEM_ReinitRamBank API
 *  The MemoryManagerLight will call this API when allocating a new block in the heap
 *  Defining this flag to 0 will make the system shutdown only the non-ecc banks */
#ifndef gPlatformShutdownEccRamInLowPower
#define gPlatformShutdownEccRamInLowPower 1
#endif

#define gPlatformRamStartAddress_c (0x20000000U)
#define gPlatformRamEndAddress_c   (0x2003FFFFU)

#define PLATFORM_CTCM0_IDX 0U
#define PLATFORM_CTCM1_IDX 1U
#define PLATFORM_STCM0_IDX 2U
#define PLATFORM_STCM1_IDX 3U
#define PLATFORM_STCM2_IDX 4U
#define PLATFORM_STCM3_IDX 5U
#define PLATFORM_STCM4_IDX 6U
#define PLATFORM_STCM5_IDX 7U
#define PLATFORM_STCM6_IDX 8U
#define PLATFORM_STCM7_IDX 9U
#define PLATFORM_STCM8_IDX 10U

#define PLATFORM_CTCM0_START_ADDR (0x04000000U)
#define PLATFORM_CTCM0_END_ADDR   (0x04003FFFU)
#define PLATFORM_CTCM1_START_ADDR (0x04004000U)
#define PLATFORM_CTCM1_END_ADDR   (0x04007FFFU)
#define PLATFORM_STCM0_START_ADDR (0x20000000U)
#define PLATFORM_STCM0_END_ADDR   (0x20003FFFU)
#define PLATFORM_STCM1_START_ADDR (0x20004000U)
#define PLATFORM_STCM1_END_ADDR   (0x20007FFFU)
#define PLATFORM_STCM2_START_ADDR (0x20008000U)
#define PLATFORM_STCM2_END_ADDR   (0x2000FFFFU)
#define PLATFORM_STCM3_START_ADDR (0x20010000U)
#define PLATFORM_STCM3_END_ADDR   (0x20017FFFU)
#define PLATFORM_STCM4_START_ADDR (0x20018000U)
#define PLATFORM_STCM4_END_ADDR   (0x2001FFFFU)
#define PLATFORM_STCM5_START_ADDR (0x20020000U)
#define PLATFORM_STCM5_END_ADDR   (0x20027FFFU)
#define PLATFORM_STCM6_START_ADDR (0x20028000U)
#define PLATFORM_STCM6_END_ADDR   (0x2002FFFFU)
#define PLATFORM_STCM7_START_ADDR (0x20030000U)
#define PLATFORM_STCM7_END_ADDR   (0x20037FFFU)
#define PLATFORM_STCM8_START_ADDR (0x20038000U)
#define PLATFORM_STCM8_END_ADDR   (0x20039FFFU)

#define PLATFORM_BANK_START_ADDR                                                                                    \
    PLATFORM_CTCM0_START_ADDR, PLATFORM_CTCM1_START_ADDR, PLATFORM_STCM0_START_ADDR, PLATFORM_STCM1_START_ADDR,     \
        PLATFORM_STCM2_START_ADDR, PLATFORM_STCM3_START_ADDR, PLATFORM_STCM4_START_ADDR, PLATFORM_STCM5_START_ADDR, \
        PLATFORM_STCM6_START_ADDR, PLATFORM_STCM7_START_ADDR, PLATFORM_STCM8_START_ADDR

#define PLATFORM_BANK_END_ADDR                                                                              \
    PLATFORM_CTCM0_END_ADDR, PLATFORM_CTCM1_END_ADDR, PLATFORM_STCM0_END_ADDR, PLATFORM_STCM1_END_ADDR,     \
        PLATFORM_STCM2_END_ADDR, PLATFORM_STCM3_END_ADDR, PLATFORM_STCM4_END_ADDR, PLATFORM_STCM5_END_ADDR, \
        PLATFORM_STCM6_END_ADDR, PLATFORM_STCM7_END_ADDR, PLATFORM_STCM8_END_ADDR

#define PLATFORM_BANK_IS_ECC TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE

#define PLATFORM_VBAT_LDORAM_IDX PLATFORM_STCM8_IDX

#if defined(gPlatformShutdownEccRamInLowPower) && (gPlatformShutdownEccRamInLowPower > 0)
/* In this configuration, all RAM banks can be shutdown during low power if not used
 * The ECC RAM banks can be selectively reinitialized with MEM_ReinitRamBank API
 * This API is also used by the Memory Manager Light */
#define PLATFORM_SELECT_RAM_RET_START_IDX 0U
#define PLATFORM_SELECT_RAM_RET_END_IDX   10U
#else
/* STCM3, STCM4, STCM5, STCM6, STCM7 are non-ECC RAM banks*/
#define PLATFORM_SELECT_RAM_RET_START_IDX 5U
#define PLATFORM_SELECT_RAM_RET_END_IDX   9U
#endif /* gPlatformShutdownEccRamInLowPower */

#endif /* _FWK_PLAT_DEFS_H_ */