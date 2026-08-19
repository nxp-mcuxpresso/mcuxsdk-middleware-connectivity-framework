/*
 * Copyright 2022-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************************
 * Include
 ************************************************************************************/

#include "fsl_common.h"
#include "PWR_Interface.h"
#include "FreeRTOS.h"
#include "task.h"

/************************************************************************************
 * Private type definitions and macros
 ************************************************************************************/

#define TICK_TO_US(tick) (uint64_t)((uint64_t)(tick) * (uint64_t)portTICK_PERIOD_MS * (uint64_t)1000U)
#define US_TO_TICK(us)   (uint64_t)((uint64_t)(us) / ((uint64_t)portTICK_PERIOD_MS * (uint64_t)1000U))

/* Maximum value representable by TickType_t — derived from its actual width
 * to remain correct regardless of configUSE_16_BIT_TICKS. */
#define TICK_TYPE_MAX ((uint64_t)(TickType_t)(~(TickType_t)0U))

/************************************************************************************
 * Private functions prototypes
 ************************************************************************************/
#if defined(configUSE_TICKLESS_IDLE) && (configUSE_TICKLESS_IDLE > 0)
static TickType_t PWR_ConvertUsecToTicks(uint64_t usec);
#endif
/************************************************************************************
 * Private memory declarations
 ************************************************************************************/

#if defined(configUSE_TICKLESS_IDLE) && (configUSE_TICKLESS_IDLE > 0)
static uint64_t cumulatedErrorUs = 0U;
#endif

/************************************************************************************
 * Private functions definitions
 ************************************************************************************/
#if defined(configUSE_TICKLESS_IDLE) && (configUSE_TICKLESS_IDLE > 0)
static TickType_t PWR_ConvertUsecToTicks(uint64_t usec)
{
    uint64_t ticks;

    ticks = US_TO_TICK(usec);
    /* Saturate to TICK_TYPE_MAX before casting to TickType_t to prevent
     * truncation of the uint64_t intermediate value (CERT INT31-C, CID 35984798). */
    if (ticks > TICK_TYPE_MAX)
    {
        ticks = TICK_TYPE_MAX;
    }
    return (TickType_t)ticks;
}
#endif

/************************************************************************************
 * Public functions
 ************************************************************************************/

bool PWR_SysticksPreProcess(uint32_t xExpectedIdleTime, uint64_t *expectedIdleTimeUs)
{
#if defined(configUSE_TICKLESS_IDLE) && (configUSE_TICKLESS_IDLE > 0)
    bool             abortIdle = false;
    eSleepModeStatus eSleepStatus;
    TickType_t       cumulatedErrorTick;

    /* Convert cumulated error to ticks. */
    cumulatedErrorTick = PWR_ConvertUsecToTicks(cumulatedErrorUs);

    if (cumulatedErrorTick > 0U)
    {
        if (cumulatedErrorTick <= xExpectedIdleTime)
        {
            /* In order to recover most of the cumulated error, we increase the
             * FreeRTOS tick count with a number of ticks corresponding to the
             * overall cumulated error */
            vTaskStepTick(cumulatedErrorTick);

            /* Subtract the error compensated to the overall error and reduce the
             * expected idle time accordingly */
            /* cumulatedErrorTick was obtained via US_TO_TICK (integer division), so it can only
             * yield a value smaller than or equal to cumulatedErrorUs / portTICK_PERIOD_MS_US.
             * Converting back via TICK_TO_US necessarily gives a value <= cumulatedErrorUs. */
            /* No unsigned wrap: guard above ensures cumulatedErrorTick <= xExpectedIdleTime.
             * (CERT INT30-C, CID 35985953 - false positive) */
            /* coverity[cert_int30_c_violation: FALSE] */
            cumulatedErrorUs -= TICK_TO_US(cumulatedErrorTick);
            xExpectedIdleTime -= cumulatedErrorTick;
        }
        else
        {
            assert(cumulatedErrorTick > xExpectedIdleTime);

            /* As the expected idle time is less than the error, we cannot
             * recover the whole drift for now, we'll try to do it next time */
            vTaskStepTick(xExpectedIdleTime);
            /* No unsigned wrap: else-branch means cumulatedErrorTick > xExpectedIdleTime,
             * so TICK_TO_US(xExpectedIdleTime) < TICK_TO_US(cumulatedErrorTick) <= cumulatedErrorUs.
             * (CERT INT30-C, CID  35984878: - false positive) */
            /* coverity[cert_int30_c_violation: FALSE] */

            cumulatedErrorUs -= TICK_TO_US(xExpectedIdleTime);
        }
        /* Whenever the timebase is compensated, abort the current idle loop to let the kernel re-calculate the
         * expected idle time */
        abortIdle = true;
    }

    if (abortIdle == false)
    {
        /* Disable Systicks for tickless mode */
        SysTick->CTRL &= ~(SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);

        /* Make sure it's still ok to enter low power mode */
        eSleepStatus = eTaskConfirmSleepModeStatus();

        if (eSleepStatus != eAbortSleep)
        {
            if (eSleepStatus == eNoTasksWaitingTimeout)
            {
                /* if no tasks are waiting a timeout, then idle time can be infinite */
                *expectedIdleTimeUs = 0U;
            }
            else
            {
                assert(eSleepStatus == eStandardSleep);

                /* Convert the expected idle time in us for the PWR function, the
                 * sleep mode will not last any longer than this expected idle time */
                *expectedIdleTimeUs = TICK_TO_US(xExpectedIdleTime);
            }
        }
        else
        {
            abortIdle = true;
        }
    }

    /* Abort Idle -> re enable systicks */
    if (abortIdle == true)
    {
        /* Re-enable Systicks before releasing interrupts in case an interrupt fires
         * directly, otherwise we could loose some precision */
        SysTick->CTRL |= (SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
    }

    return abortIdle;
#else
    return true;
#endif
}

void PWR_SysticksPostProcess(uint64_t expectedIdleTimeUs, uint64_t actualIdleTimeUs)
{
#if defined(configUSE_TICKLESS_IDLE) && (configUSE_TICKLESS_IDLE > 0)
    TickType_t idleTimeTick;

    if ((actualIdleTimeUs > expectedIdleTimeUs) && (expectedIdleTimeUs > 0U))
    {
        /* Add the error to be compensated on next low power period */
        /* No unsigned wrap: if cumulatedErrorUs were to wrap, it would mean that the cumulated
         * error would have reached thousands of years.
         * (CERT INT30-C, CID  35986237: - false positive) */
        /* coverity[cert_int30_c_violation: FALSE] */
        cumulatedErrorUs += actualIdleTimeUs - expectedIdleTimeUs;

        /* Fake the idle time to avoid corrupting FreeRTOS timebase
         * On next idle loop, the tick count will be increased by the ticks
         * we missed there, and the next low power period will be reduced */
        actualIdleTimeUs = expectedIdleTimeUs;
        idleTimeTick     = PWR_ConvertUsecToTicks(actualIdleTimeUs);
    }
    else
    {
        /* Convert us to ticks */
        idleTimeTick = PWR_ConvertUsecToTicks(actualIdleTimeUs);

        /* Compute the error due to discarded LSB.
         * No unsigned wrap: integer division guarantees TICK_TO_US(idleTimeTick) <= actualIdleTimeUs
         * always (US_TO_TICK truncates, so round-trip can only be equal or smaller).
         * (CERT INT30-C, CID 35985785 - false positive) */
        /* coverity[cert_int30_c_violation: FALSE] */
        cumulatedErrorUs += actualIdleTimeUs - TICK_TO_US(idleTimeTick);
    }

    /* Update the OS time ticks. */
    vTaskStepTick(idleTimeTick);

    /* Re-enable Systicks before releasing interrupts in case an interrupt fires
     * directly, otherwise we could loose some precision */
    SysTick->CTRL |= (SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
#endif
}
