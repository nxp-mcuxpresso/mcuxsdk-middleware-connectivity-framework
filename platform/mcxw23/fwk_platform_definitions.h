/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_PLAT_DEFS_H_
#define _FWK_PLAT_DEFS_H_

#include "fsl_device_registers.h"

#define FWK_MCXW32_FAMILIES 1

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
#endif /* _FWK_PLAT_DEFS_H_ */
