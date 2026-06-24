/* -------------------------------------------------------------------------- */
/*                           Copyright 2025-2026 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fsl_device_registers.h"
#include "fwk_platform_dbg.h"
#include <stddef.h>
#include "mcmgr.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
/* Bit definitions */
#define NBU2HOST_ERROR_INDICATION (1U << 0) /* Bit 0: Fault occurred */
#define NBU2HOST_WARNING_SHIFT    (2U)      /* Bits 2-4: Warning count field */
#define NBU2HOST_WARNING_MASK     (0x1CU)   /* 3-bit mask for warning count */
#define NBU2HOST_WARNING_MAX      (NBU2HOST_WARNING_MASK >> NBU2HOST_WARNING_SHIFT)
/* Macro to extract warning count from register value */
#define NBU2HOST_WARNING_GET(x) (((x)&NBU2HOST_WARNING_MASK) >> NBU2HOST_WARNING_SHIFT)

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

bool PLATFORM_IsNbuFaultSet(void)
{
    return (RFMC->RF2P4GHZ_RADIO2HOST & NBU2HOST_ERROR_INDICATION) != 0U;
}

/*!
 * IMPORTANT LIMITATION: The NBU uses 3 bits (1-7) that wrap around (7→1). If 7 warnings
 * occur between calls, the 3 bits value will remain the same. Call frequently for accurate counting.
 *
 * For accurate warning counting, ensure this function is called with
 * sufficient frequency relative to the NBU warning generation rate.
 */
bool PLATFORM_IsNbuWarningSet(uint8_t *pCount)
{
    static uint8_t last_register_count = 0U;
    static bool    first_read          = true;

    uint8_t current_warning_count;
    uint8_t new_warnings  = 0U;
    bool    hasNewWarning = false;

    /* Extract warning bits from the register */
    current_warning_count = (uint8_t)NBU2HOST_WARNING_GET(RFMC->RF2P4GHZ_RADIO2HOST);

    if (current_warning_count != 0U)
    {
        if (first_read)
        {
            /* First time reading, all current warnings are "new" */
            new_warnings  = current_warning_count;
            hasNewWarning = true;
            first_read    = false;
        }
        else if (current_warning_count != last_register_count)
        {
            last_register_count &= (uint8_t)NBU2HOST_WARNING_MAX;
            current_warning_count &= (uint8_t)NBU2HOST_WARNING_MAX;

            /* Calculate new warnings since last read */
            if (current_warning_count > last_register_count)
            {
                /* Normal increment */
                new_warnings = current_warning_count - last_register_count;
            }
            else
            {
                /* Wrap-around occurred: 7 -> 1, 2, 3, etc. */
                new_warnings = (NBU2HOST_WARNING_MAX - last_register_count) + current_warning_count;
            }
            hasNewWarning = true;
        }
        else
        {
            /* Warning count unchanged - no new warnings to report */
            /* new_warnings remains 0, hasNewWarning remains false */
        }

        last_register_count = current_warning_count;
    }

    /* Set output count if pointer is provided */
    if (pCount != NULL)
    {
        *pCount = new_warnings;
    }

    return hasNewWarning;
}

int PLATFORM_TryForceNbuFault(void)
{
#if defined(MCMGR_REMOTE_APP_EVENT_COUNT) && (MCMGR_REMOTE_APP_EVENT_COUNT > 1U)
    int ret = 0;

    /* Trigger an MCMGR remote application event on the NBU (Core1). The NBU
     * has registered a handler that raises a fault so the fault handler can
     * capture the debug context and stream a coredump back to the host.
     * Note: this relies on the NBU still being able to service the MCMGR
     * (IMU/MU) interrupt. A fully hard-locked NBU with interrupts disabled
     * cannot be broken into this way. */
    if (MCMGR_TriggerEventForce(kMCMGR_Core1, kMCMGR_RemoteApplicationEvent1, 0U) != kStatus_MCMGR_Success)
    {
        ret = -1;
    }

    return ret;
#else
    /* Force-fault feature unavailable (MCMGR_REMOTE_APP_EVENT_COUNT < 2) */
    return -1;
#endif
}
