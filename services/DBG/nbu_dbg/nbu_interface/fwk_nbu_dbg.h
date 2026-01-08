/*
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWK_DEBUG_NBU_H__
#define __FWK_DEBUG_NBU_H__

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*                               Public macros                                */
/* -------------------------------------------------------------------------- */
typedef enum
{
    NBUDBG_GENERIC_WARN                  = 1U,
    NBUDBG_XTAL32MHZ_NOT_READY_AT_WAKEUP = 2U
    /* TODO: new warnings to be added */
} nbudbg_warning_t;

/* -------------------------------------------------------------------------- */
/*                           Public type definitions                          */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int NBUDBG_RaiseWarningToHost(nbudbg_warning_t event_type);

#endif /*  __FWK_DEBUG_NBU_H__ */
