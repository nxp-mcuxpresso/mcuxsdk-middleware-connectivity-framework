/* -------------------------------------------------------------------------- */
/*                           Copyright 2025-2026 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fsl_device_registers.h"
#include "fwk_platform_dbg.h"
#include "fwk_platform_lowpower_timer.h"
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

/* Handcheck (SW watchdog) bit definitions */
#define NBU2HOST_HANDCHECK_BIT_SHIFT      (1U)
#define NBU2HOST_HANDCHECK_BIT_INDICATION (1U << NBU2HOST_HANDCHECK_BIT_SHIFT) /* Bit 1: Handcheck NBU/HOST */

#define HOST2NBU_HANDCHECK_BIT_SHIFT      (0U)
#define HOST2NBU_HANDCHECK_BIT_INDICATION (1U << HOST2NBU_HANDCHECK_BIT_SHIFT) /* Bit 0: Handcheck HOST/NBU */

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief  Get the NBU handcheck value.
 *
 * \return uint32_t 0 or 1.
 */
static uint32_t PLATFORM_Nbu2HostGetNbuHandcheckValue(void);

/*!
 * \brief  Inform the NBU of the HOST/NBU handcheck value.
 *
 * \param[in] host2nbuHandcheckValue value read by the host to be echoed to the NBU.
 */
static void PLATFORM_Host2NbuSetHostHandcheckValue(uint32_t host2nbuHandcheckValue);

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

bool PLATFORM_IsNbuFaultSet(void)
{
    return (RFMC->RF2P4GHZ_RADIO2HOST & NBU2HOST_ERROR_INDICATION) != 0U;
}

bool PLATFORM_IsNbuStuck(uint32_t nbuWatchdogDurationInUs)
{
    bool isNbuStuck = false;

    /* Initialize the handcheck with an impossible value */
    static uint32_t previousNbuHandcheckVal = 0xFFFFFFFFU;
    uint32_t        nbuHandcheckVal         = 0U;

    static uint64_t previousTimestamp = 0ULL;
    uint64_t        timestampDiff, currentTimestamp;

    /* If the NBU is in low power it means it is not stuck, no need to execute more */
    if ((RFMC->RF2P4GHZ_MAN2 & RFMC_RF2P4GHZ_MAN2_WKUP_TIME_MASK) == 0U)
    {
        currentTimestamp = PLATFORM_GetLowPowerTimestampUs();

        timestampDiff = PLATFORM_GetLowPowerDurationUs(previousTimestamp, currentTimestamp);

        /* Check if the watchdog has expired */
        if (timestampDiff > nbuWatchdogDurationInUs)
        {
            /* Read the intercore register to check the value of the handcheck set by the NBU */
            nbuHandcheckVal = PLATFORM_Nbu2HostGetNbuHandcheckValue();

            /* Communicate to the NBU the value read by the host for it to update it in its next idle execution */
            PLATFORM_Host2NbuSetHostHandcheckValue(nbuHandcheckVal);

            /* If the handcheck value hasn't changed, the NBU did not update its value in time */
            if (nbuHandcheckVal == previousNbuHandcheckVal)
            {
                isNbuStuck = true;
            }

            previousNbuHandcheckVal = nbuHandcheckVal;

            /* Update timestamp for the next call in idle */
            previousTimestamp = currentTimestamp;
        }
    }

    return isNbuStuck;
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

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

static uint32_t PLATFORM_Nbu2HostGetNbuHandcheckValue(void)
{
    return ((RFMC->RF2P4GHZ_RADIO2HOST & NBU2HOST_HANDCHECK_BIT_INDICATION) >> NBU2HOST_HANDCHECK_BIT_SHIFT);
}

static void PLATFORM_Host2NbuSetHostHandcheckValue(uint32_t host2nbuHandcheckValue)
{
    uint32_t host2radio = RFMC->RF2P4GHZ_HOST2RADIO;
    host2radio &= (~HOST2NBU_HANDCHECK_BIT_INDICATION);
    host2radio |= ((host2nbuHandcheckValue << HOST2NBU_HANDCHECK_BIT_SHIFT) & HOST2NBU_HANDCHECK_BIT_INDICATION);
    RFMC->RF2P4GHZ_HOST2RADIO = host2radio;
}
