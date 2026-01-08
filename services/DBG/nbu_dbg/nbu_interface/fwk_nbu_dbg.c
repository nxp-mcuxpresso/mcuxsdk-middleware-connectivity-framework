/*
 * Copyright 2025-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fwk_debug_struct.h"
#include "fwk_nbu_dbg.h"
#include "fwk_platform_dbg.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

__attribute__((section(".debugbuf"))) nbu_debug_struct_t debug_buff = {
#if defined(NBUDBG_VERSION) && (NBUDBG_VERSION > 0U)
    .version = NBUDBG_VERSION,
    /* TODO: Set logging fields for NBU; in future, these may be configured by the host */
    .logging_buf_size   = NBUDBG_LOGGING_SIZE,
    .logging_buf_offset = NBUDBG_LOGGING_OFFSET,
#endif
};

nbu_debug_struct_t *debug_struct = &debug_buff;

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/**
 * \brief Inform the Host about a warning using its ID.
 *
 * \param  warning_id warning ID
 * \return 0 on success
 */
int NBUDBG_RaiseWarningToHost(nbudbg_warning_t event_type)
{
#if defined(NBUDBG_VERSION) && (NBUDBG_VERSION > 0U)
    /* Legacy structure (version 0) doesn't support this */
    static uint8_t warn_index = 0U;

    debug_struct->nbu_dbg_info.warning_index = warn_index;
    /* Circular buffer to log warning IDs */
    debug_struct->nbu_dbg_info.warnings[warn_index] = (uint8_t)event_type;
    warn_index                                      = (warn_index + 1U) % NBUDBG_MAX_NB_WARNINGS;
#endif
    return PLATFORM_Nbu2HostWarningIndication();
}
