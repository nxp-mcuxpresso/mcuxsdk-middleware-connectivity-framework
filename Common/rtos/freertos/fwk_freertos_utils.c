/*
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fsl_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fwk_platform.h"
#include "fwk_freertos_utils.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* Convert a duration in microseconds to FreeRTOS ticks.
 * The division is performed in 64-bit to avoid intermediate overflow, then the
 * result is saturated to the maximum value representable by TickType_t before
 * the narrowing cast. This keeps the conversion compliant with CERT INT31-C:
 * the value assigned to a TickType_t is guaranteed to fit and cannot be
 * truncated or misinterpreted. */
#define FWK_TICK_MAX ((uint64_t)portMAX_DELAY)

/* -------------------------------------------------------------------------- */
/*                               Private functions                               */
/* -------------------------------------------------------------------------- */
static inline TickType_t FWK_UsToTick(uint64_t us)
{
    /* One microsecond is a 1000th of a millisecond */
    uint64_t ticks = us / ((uint64_t)portTICK_PERIOD_MS * (uint64_t)1000U);

    if (ticks > FWK_TICK_MAX)
    {
        /* dependent on TickType_t definition */
        ticks = FWK_TICK_MAX;
    }

    return (TickType_t)ticks;
}

/* -------------------------------------------------------------------------- */
/*                               Private memory                               */
/* -------------------------------------------------------------------------- */

static uint64_t   preIdleHookTimeStampUs;
static uint64_t   postIdleHookTimeStampUs;
static uint64_t   idleHookDurationUs;
static TickType_t idleHookDurationTick;
static TickType_t preIdleHookTickCount;
static TickType_t postIdleHookTickCount;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void FWK_PreIdleHookTickCompensation(void)
{
    uint32_t irqMask;

    /* The following implementation aims to mitigate the impact of saving to flash on the FreeRTOS time base
     * When saving to flash, the whole system is blocked and depending on the flash and the operation it can take more
     * than 1 tick. During this time, the FreeRTOS timebase is not incremented so the whole scheduled activity is
     * shifted in time. This can cause problems with the FreeRTOS timers. FreeRTOS provides the API xTaskCatchUpTicks
     * for this exact type of scenario.
     * Here, we measure the time elapsed during the idle hook execution using SOC timers and we convert this
     * time to ticks. The kernel is then notified with xTaskCatchUpTicks about those missing ticks.
     * */
    irqMask                = DisableGlobalIRQ();
    preIdleHookTickCount   = xTaskGetTickCount();
    preIdleHookTimeStampUs = PLATFORM_GetTimeStamp();
    EnableGlobalIRQ(irqMask);
}

void FWK_PostIdleHookTickCompensation(void)
{
    uint32_t irqMask;

    irqMask                 = DisableGlobalIRQ();
    postIdleHookTickCount   = xTaskGetTickCount();
    postIdleHookTimeStampUs = PLATFORM_GetTimeStamp();

    if (postIdleHookTimeStampUs < preIdleHookTimeStampUs)
    {
        /* Handle wrap */
        idleHookDurationUs = PLATFORM_GetMaxTimeStamp() - preIdleHookTimeStampUs + postIdleHookTimeStampUs;
    }
    else
    {
        idleHookDurationUs = postIdleHookTimeStampUs - preIdleHookTimeStampUs;
    }

    idleHookDurationTick = FWK_UsToTick(idleHookDurationUs);

    if (idleHookDurationTick > 0U)
    {
        TickType_t tmpTickCount;

        /* If the idle hook was called with interrupts unmasked, the systicks interrupts could execute and increase
         * the tick count. We have to consider the number of tick counts taken into account by
         * these interrupts and subtract this duration from the overall duration measured with the SOC timers.
         * If we don't do this, we may pass a higher tick count to xTaskCatchUpTicks than expected. */
        if (postIdleHookTickCount < preIdleHookTickCount)
        {
            /* Tick count wrapped */
            tmpTickCount = portMAX_DELAY - preIdleHookTickCount + postIdleHookTickCount;
        }
        else
        {
            tmpTickCount = postIdleHookTickCount - preIdleHookTickCount;
        }

        if (idleHookDurationTick > tmpTickCount)
        {
            idleHookDurationTick -= tmpTickCount;
            /* This function corrects the tick count value after the application code has held
             * interrupts disabled for an extended period resulting in tick interrupts having
             * been missed.
             *
             * This function is similar to vTaskStepTick(), however, unlike
             * vTaskStepTick(), xTaskCatchUpTicks() may move the tick count forward past a
             * time at which a task should be removed from the blocked state.  That means
             * tasks may have to be removed from the blocked state as the tick count is
             * moved. */
            (void)xTaskCatchUpTicks(idleHookDurationTick);
        }
    }

    EnableGlobalIRQ(irqMask);
}