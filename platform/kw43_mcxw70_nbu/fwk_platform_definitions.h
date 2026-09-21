/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_PLAT_DEFS_H_
#define _FWK_PLAT_DEFS_H_

#define FWK_KW43_MCXW70_NBU_FAMILIES 1

#define gPlatformHasIntercoreCommonTimestamp_d 1
#define gPlatformTstmr0HasClkControl_d         1

#define FWK_MRCC_TSTMR0_REG       (volatile uint32_t *)(0x401A1288U)
#define FWK_MRCC_TSTMR0_CLKSEL_CC (0x3U)

#define FWK_TSRMR0_BASE   (0x40199000U)
#define FWK_TSRMR1_BASE   (0x4019B000U)
#define FWK_TSTMR_NB_INST 2U

/* On KW43/MCXW70, RAM retention settings are different (SRAM retention is controlled by CMC, not RF_CMC)
 * So, adapt the configuration to retain EBRAM and TX/RX packet RAM */
#define FWK_PLAT_DEFAULT_RF_CMC_RAM_PWR (BIT29 | BIT17 | BIT16)

#endif /* _FWK_PLAT_DEFS_H_ */
