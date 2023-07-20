/* -------------------------------------------------------------------------- */
/*                           Copyright 2022-2023 NXP                          */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_platform.h"
// #include "fsl_adapter_time_stamp.h"

/* -------------------------------------------------------------------------- */
/*                               Private memory                               */
/* -------------------------------------------------------------------------- */

static volatile int timer_manager_initialized = 0;

// static TIME_STAMP_HANDLE_DEFINE(timestampHandle);
// static bool timestampInitialized = false;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

timer_status_t PLATFORM_InitTimerManager(void)
{
    /* Initialize timer manager */
    timer_config_t timerConfig;
    timer_status_t status;

    if (timer_manager_initialized == 0)
    {
        timerConfig.instance    = PLATFORM_TM_INSTANCE;
        timerConfig.srcClock_Hz = CLOCK_GetFreq(kCLOCK_OscClk);

        status = TM_Init(&timerConfig);
        if (status == kStatus_TimerSuccess)
        {
            timer_manager_initialized = 1;
        }
    }
    return status;
}

void PLATFORM_DeinitTimerManager(void)
{
    if (timer_manager_initialized == 1)
    {
        TM_Deinit();
        timer_manager_initialized = 0;
    }
}

void PLATFORM_InitTimeStamp(void)
{
    // hal_time_stamp_config_t config;

    // if (timestampInitialized == false)
    // {
    //     CLOCK_AttachClk(kCLK32K_to_OSTIMER_CLK);

    //     config.instance       = 0;
    //     config.srcClock_Hz    = CLOCK_GetOSTimerClkFreq();
    //     config.clockSrcSelect = 1;

    //     HAL_TimeStampInit(timestampHandle, &config);

    //     timestampInitialized = true;
    // }
}

uint64_t PLATFORM_GetTimeStamp(void)
{
    // return HAL_GetTimeStamp(timestampHandle);
    return 0U;
}

uint64_t PLATFORM_GetMaxTimeStamp(void)
{
    // /* The timestamp module always converts the timer counter to microsec
    //  * as the OSTIMER is a 64bits timer, the conversion can overflow after a certain counter value
    //  * So the timestamp module discards the 20 MSB to avoid this, so the max value of the counter is
    //  * 0xFFFFFFFFFFFU */
    // return (uint64_t)COUNT_TO_USEC(0xFFFFFFFFFFFU, CLOCK_GetOSTimerClkFreq());
    return 0xFFFFFFFFU;
}