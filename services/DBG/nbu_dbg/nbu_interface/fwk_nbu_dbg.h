/*
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWK_NBUDEBUG_NBU_IF_H__
#define __FWK_NBUDEBUG_NBU_IF_H__

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

/*!
 * \brief Initialize the NBU debug features.
 *
 * \return 0 on success, negative value on error.
 */
int NBUDBG_Init(void);

#endif /* __FWK_NBUDEBUG_NBU_IF_H__ */
