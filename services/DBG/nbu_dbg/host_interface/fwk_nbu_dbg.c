/*
 * Copyright 2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_nbu_dbg.h"
#include "fwk_platform.h"
#include "fwk_debug_struct.h"
#include "fwk_platform_dbg.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */
extern unsigned int m_sqram_debug_start[];
nbu_debug_struct_t *debug_struct = (nbu_debug_struct_t *)(m_sqram_debug_start);

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
static nbu_dbg_system_cb_t nbu_dbg_system_cb = (nbu_dbg_system_cb_t)NULL;

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void NBUDBG_StateCheck(void)
{
    nbu_dbg_context_t nbu_event = {0U};

    (void)PLATFORM_IsNbuWarningSet(&nbu_event.nbu_warning_count);
    nbu_event.nbu_error_count = (uint8_t)PLATFORM_IsNbuFaultSet();

    if ((nbu_event.nbu_error_count > 0U) || (nbu_event.nbu_warning_count > 0U))
    {
        nbu_dbg_system_cb(&nbu_event);
    }
}

void NBUDBG_RegisterNbuDebugNotificationCb(nbu_dbg_system_cb_t cb)
{
    nbu_dbg_system_cb = cb;
}

int NBUDBG_StructDump(nbu_debug_struct_t *debug_info)
{
    int status = 0;

    if (debug_info == NULL)
    {
        status = -1; /* Invalid parameter */
    }
    else
    {
        PLATFORM_RemoteActiveReq();
        /* Need to wake NBU domain to access the debug structure */
        memcpy(debug_info, debug_struct, sizeof(nbu_debug_struct_t));
        PLATFORM_RemoteActiveRel();
    }

    return status;
}
