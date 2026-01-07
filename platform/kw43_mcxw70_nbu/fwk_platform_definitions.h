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

typedef enum _fwk_tstmr_clk_sel
{
    FwkTSTMR0_ClkSel_1MHz = 5U, /*!< TSTMR 1MHz configuration for gPlatformTstmr0HasClkControl_d */
} fwk_tstmr_clk_sel_t;

#endif /* _FWK_PLAT_DEFS_H_ */
