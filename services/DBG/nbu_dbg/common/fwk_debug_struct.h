/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWK_DEBUG_STRUCT_H__
#define __FWK_DEBUG_STRUCT_H__

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*                                Public macros                               */
/* -------------------------------------------------------------------------- */
#define NBUDBG_COMMON_STRUCT_SIZE    sizeof(regs_status_t)
#define NBUDBG_BLE_STRUCT_SIZE       0x100
#define NBUDBG_15_4_STRUCT_SIZE      0x40
#define NBUDBG_SET_REG(reg, val)     ((debug_struct)->reg_dump.reg = (val))
#define NBUDBG_SET_EXCEPTION_ID(val) ((debug_struct)->reg_dump.exception_id = (val))
#define NBUDBG_SET_XFAR(reg, val)    ((debug_struct)->reg_dump.xfar.reg = (val))
#define NBUDBG_SET_SHA(val)          ((debug_struct)->reg_dump.nbu_sha1 = (val))
#define NBUDBG_BLE_STRUCT            debug_struct->dbg_ble
#define NBUDBG_15_4_STRUCT           debug_struct->dbg_15_4

/* -------------------------------------------------------------------------- */
/*                            Public type definitions                         */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t exception_id;
    uint32_t nbu_sha1;
    uint32_t cfsr;
    union
    {
        uint32_t mmfar;
        uint32_t bfar;
    } xfar;
    uint32_t pc;
    uint32_t lr;
    uint32_t psp;
    uint32_t psr;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t reserved[3]; /* For futur use */
} regs_status_t;

typedef struct
{
    regs_status_t reg_dump;
    uint8_t       dbg_ble[NBUDBG_BLE_STRUCT_SIZE];
    uint8_t       dbg_15_4[NBUDBG_15_4_STRUCT_SIZE];
} nbu_debug_struct_t;

extern nbu_debug_struct_t *debug_struct;
#endif /*  __FWK_DEBUG_STRUCT_H__ */
