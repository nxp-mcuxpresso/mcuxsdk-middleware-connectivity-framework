/* -------------------------------------------------------------------------- */
/*                           Copyright 2026 NXP                               */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdbool.h>

#include "fwk_platform.h"
#include "fsl_adapter_time_stamp.h"

/* -------------------------------------------------------------------------- */
/*                              Private variables                             */
/* -------------------------------------------------------------------------- */

static TIME_STAMP_HANDLE_DEFINE(timestampHandle);
static volatile bool timestampInitialized = false;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void PLATFORM_InitTimeStamp(void)
{
    if (timestampInitialized == false)
    {
        /* Set 32k_clk for ostimer_clk. */
        CLOCK_EnableOsc32K(true);
        CLOCK_EnableClock(kCLOCK_Rtc);
        CLOCK_AttachClk(PLATFORM_OSTIMER_CLK);

        /* All fields are explicitly named to satisfy MISRA §5.2 (fully initialised at declaration). */
        /* Refer to Clock Tree for srcClock_Hz and clockSrcSelect values. */
        hal_time_stamp_config_t config = {
            .instance       = 0U,
            .srcClock_Hz    = CLOCK_GetOsc32KFreq(),
            .clockSrcSelect = PLATFORM_OSTIMER_CLK_SRC_SELECT,
        };

        HAL_TimeStampInit(timestampHandle, &config);

        timestampInitialized = true;
    }
}

uint64_t PLATFORM_GetTimeStamp(void)
{
    return HAL_GetTimeStamp(timestampHandle);
}

uint64_t PLATFORM_GetMaxTimeStamp(void)
{
    return PLATFORM_MAX_TIMESTAMP_US;
}

void PLATFORM_TimeStampEnterPowerDown(void)
{
    if (timestampInitialized == true)
    {
        HAL_TimeStampEnterLowpower(timestampHandle);
        CLOCK_AttachClk(kNONE_to_OSTIMER_CLK);
    }
}

void PLATFORM_TimeStampExitPowerDown(void)
{
    if (timestampInitialized == true)
    {
        CLOCK_AttachClk(PLATFORM_OSTIMER_CLK);
        HAL_TimeStampExitLowpower(timestampHandle);
    }
}
