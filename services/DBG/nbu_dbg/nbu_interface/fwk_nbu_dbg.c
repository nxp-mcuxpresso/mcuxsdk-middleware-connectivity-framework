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
#include "mcmgr.h"

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

#if defined(MCMGR_REMOTE_APP_EVENT_COUNT) && (MCMGR_REMOTE_APP_EVENT_COUNT > 1U)
/*!
 * \brief MCMGR event callback invoked when the host requests a forced fault.
 *
 * This callback runs in the MCMGR event (IMU/MU) interrupt context on the NBU.
 * It raises a fault so the fault handler can capture the debug context and
 * stream a coredump to the host. This call does not return.
 *
 * \param[in] coreNum source core number (unused)
 * \param[in] data    optional event data (unused)
 * \param[in] context user context (unused)
 */
static void NBUDBG_ForceFaultEventHandler(mcmgr_core_t coreNum, uint16_t data, void *context)
{
    (void)coreNum;
    (void)data;
    (void)context;

    PLATFORM_NbuRaiseFault();
}

/*!
 * \brief Initialize the force-fault feature on the NBU.
 *
 * Registers an MCMGR remote application event callback so that the host can
 * request the NBU to raise a fault on demand. When the event is received, the
 * NBU triggers a fault (via PLATFORM_NbuRaiseFault) so the fault handler can
 * capture the debug context and stream a coredump to the host.
 *
 * This is a debug-only feature that requires MCMGR_REMOTE_APP_EVENT_COUNT >= 2.
 *
 * \return 0 on success, negative value on error.
 */
static int NBUDBG_InitForceFault(void)
{
    int            ret = 0;
    mcmgr_status_t status;

    status = MCMGR_RegisterEvent(kMCMGR_RemoteApplicationEvent1, NBUDBG_ForceFaultEventHandler, NULL);
    if (status != kStatus_MCMGR_Success)
    {
        ret = -1;
    }

    return ret;
}
#else
static int NBUDBG_InitForceFault(void)
{
    /* Force-fault feature unavailable (MCMGR_REMOTE_APP_EVENT_COUNT < 2) */
    return 0;
}
#endif /* MCMGR_REMOTE_APP_EVENT_COUNT > 1U */

int NBUDBG_Init(void)
{
    int ret = 0;

    /* Register the force-fault feature. Additional NBU debug initialization
     * may be added here in the future. */
    ret = NBUDBG_InitForceFault();

    return ret;
}
