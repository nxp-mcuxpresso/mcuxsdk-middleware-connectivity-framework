/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_PLAT_DEFS_H_
#define _FWK_PLAT_DEFS_H_

/* Placeholder for platform-specific definitions */

#define FWK_MRCC_TSTMR0_REG       (volatile uint32_t *)(0xA919C0C0U)
#define FWK_MRCC_TSTMR0_CLKSEL_CC (0x3U)
#define FWK_TSRMR0_BASE           0xA91B0000U
#define FWK_TSTMR_NB_INST         1U

/* Retain all banks by default */
#define FWK_PLAT_DEFAULT_RF_CMC_RAM_PWR (RF_CMC1_RAM_PWR_DS_EN_MASK)

#endif /* _FWK_PLAT_DEFS_H_ */
