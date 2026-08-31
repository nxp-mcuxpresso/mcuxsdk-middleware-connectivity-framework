/* -------------------------------------------------------------------------- */
/*                           Copyright 2026 NXP                               */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_platform.h"
#include "fwk_platform_lowpower.h"

/* SDK components */
#include "fsl_pm_core.h"
#include "fsl_pm_device.h"
#include "fsl_power.h"
#include "fsl_usart.h"
#include "fsl_rtc.h"
#include <assert.h>
/* board.h is included only to supply the default BOARD_BT_UART_INSTANCE value.
 * Define PLATFORM_BT_UART_INSTANCE externally to skip this board-level header. */
#ifndef PLATFORM_BT_UART_INSTANCE
#include "board.h"
#endif /* PLATFORM_BT_UART_INSTANCE */
#if defined(CONFIG_BT_CONTROLLER_SLEEP)
#include "controller_low_power.h"
#endif /* CONFIG_BT_CONTROLLER_SLEEP */

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* Index into the USART_BASE_PTRS array for the HCI UART peripheral.
 * Defaults to the board-supplied value; override by defining this macro
 * at compile time to decouple from board.h. */
#ifndef PLATFORM_BT_UART_INSTANCE
#define PLATFORM_BT_UART_INSTANCE BOARD_BT_UART_INSTANCE
#endif /* PLATFORM_BT_UART_INSTANCE */

/* Number of NOP cycles inserted after re-enabling the HCI UART on wake-up.
 * This short busy-wait gives the USART internal state machine time to
 * re-initialise before the host attempts to transmit, preventing spurious
 * framing errors on the first byte.  The value was determined empirically
 * at 300 MHz core clock (~7 us).  Adjust if the core clock changes. */
#ifndef PLATFORM_BT_UART_REENABLE_NOP_COUNT
#define PLATFORM_BT_UART_REENABLE_NOP_COUNT 2000U
#endif

#define PLATFORM_WAKEUP_TIMER_STOP_MAX_RETRIES 1000U

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Initialize RTC to be used as a wake up source
 *
 */
static void PLATFORM_InitRtc(void);

/*!
 * \brief Initializes wake up sources
 *
 */
static void PLATFORM_InitWakeUpSources(void);

/*!
 * \brief Callback registered to SDK Power Manager to get notified of entry/exit of low power modes
 *
 * \param[in] eventType event specifying if we entered or exited from low power mode
 * \param[in] powerState low power mode used during low power period
 * \param[in] data Optional data passed when the callback got registered (not used currently)
 * \return status_t
 */
static status_t PLATFORM_LowPowerCallback(pm_event_type_t eventType, uint8_t powerState, void *data);

/* -------------------------------------------------------------------------- */
/*                             Private variables                              */
/* -------------------------------------------------------------------------- */

static USART_Type *const platformHciUsartBase[] = USART_BASE_PTRS;
static volatile bool     platformBoardSleep     = false;

static pm_notify_element_t platformLpNotifyGroup = {
    .notifyCallback = PLATFORM_LowPowerCallback,
    .data           = NULL,
};

static pm_wakeup_source_t C2HWakeupSource;
static pm_wakeup_source_t timerWakeupSource;

static uint16_t platformWakeUpTimerStartCount = 0U;
static uint16_t platformWakeUpTimerStopCount  = 0U;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void PLATFORM_LowPowerInit(void)
{
    status_t status;

    POWER_ClearEventFlags(POWER_GetEventFlags());

    /* Register the low power Notify callback as high priority. */
    status = PM_RegisterNotify(kPM_NotifyGroup2, &platformLpNotifyGroup);
    assert(status == kStatus_Success);
    (void)status;

    /* Init mandatory wake up sources such as BLE wake up. */
    PLATFORM_InitWakeUpSources();
}

void PLATFORM_InitWakeUpTimer(void)
{
    /* Init RTC to be used as wake up source. */
    PLATFORM_InitRtc();

    /* Init timestamp module to measure low power duration. */
    PLATFORM_InitTimeStamp();
}

uint8_t PLATFORM_ControllerLowPowerInit(void)
{
#if defined(CONFIG_BT_CONTROLLER_SLEEP)
    return controller_low_power_init();
#else
    return 0U;
#endif /* CONFIG_BT_CONTROLLER_SLEEP */
}

void PLATFORM_StartWakeUpTimer(uint64_t timeOutUs)
{
    /* RTC WAKE timer runs at 1 kHz. Convert microseconds to 1 kHz counts.
     * The 16-bit WAKE counter limits the maximum timeout to 65535 ms (~65 s). */
    uint32_t wakeCount = USEC_TO_COUNT(timeOutUs, 1000U);
    RTC_EnableTimer(RTC, false);
    platformWakeUpTimerStartCount = (uint16_t)wakeCount;
    RTC_SetWakeupCount(RTC, platformWakeUpTimerStartCount);
    RTC_EnableWakeupTimer(RTC, true);
    RTC_EnableTimer(RTC, true);
}

void PLATFORM_StopWakeUpTimer(void)
{
    /* Poll until the WAKE counter has counted down to or below the start value.
     * The loop is bounded to prevent an indefinite spin if the RTC stops. */
    uint32_t retries = 0U;
    do
    {
        platformWakeUpTimerStopCount = RTC_GetWakeupCount(RTC);
        retries++;
    } while ((platformWakeUpTimerStopCount > platformWakeUpTimerStartCount) &&
             (retries < PLATFORM_WAKEUP_TIMER_STOP_MAX_RETRIES));

    RTC_ClearStatusFlags(RTC, RTC_GetStatusFlags(RTC));
    /* Clear all FLAGS. */
    POWER_ClearEventFlags(POWER_GetEventFlags());
    NVIC_ClearPendingIRQ(RTC_IRQn);
    RTC_EnableWakeupTimer(RTC, false);
    RTC_EnableTimer(RTC, false);
}

uint64_t PLATFORM_GetLowPowerTimestampUs(void)
{
    return PLATFORM_GetTimeStamp();
}

uint64_t PLATFORM_GetLowPowerDurationUs(uint64_t enterLowPowerTimestamp, uint64_t exitLowPowerTimestamp)
{
    uint64_t durationUs;

    if (exitLowPowerTimestamp < enterLowPowerTimestamp)
    {
        durationUs = PLATFORM_GetMaxTimeStamp() - enterLowPowerTimestamp + exitLowPowerTimestamp;
    }
    else
    {
        durationUs = exitLowPowerTimestamp - enterLowPowerTimestamp;
    }

    return durationUs;
}

void PLATFORM_EnterLowPower(void)
{
    /* If do not disable BT UART, host RTS will stay low even when enters low power mode.
     * There might be a risk that controller will send data to host even when host is not totally waken up.
     * The CFG register is written directly (rather than through USART_Enable()) to keep the operation
     * as a single atomic read-modify-write with no function-call overhead.  This is intentional: the
     * low-power entry path is timing-sensitive and the direct register access is the most efficient
     * way to disable the peripheral. */
    platformHciUsartBase[PLATFORM_BT_UART_INSTANCE]->CFG &= ~((uint32_t)USART_CFG_ENABLE_MASK);
#if defined(CONFIG_BT_CONTROLLER_SLEEP)
    controller_enter_low_power();
#endif /* CONFIG_BT_CONTROLLER_SLEEP */
}

void PLATFORM_ExitLowPower(void)
{
#if defined(CONFIG_BT_CONTROLLER_SLEEP)
    controller_exit_low_power();
#endif /* CONFIG_BT_CONTROLLER_SLEEP */
    /* This busy-wait runs immediately after exiting low-power mode, before the system clocks and
     * RTOS tick timer are fully restored.  OS-level delay functions (e.g. vTaskDelay) rely on
     * SysTick or other timer peripherals that may not yet be running at their normal rate, so
     * they cannot be used here.  A NOP-based loop is the only reliable delay available at this
     * early stage of wake-up.  See PLATFORM_BT_UART_REENABLE_NOP_COUNT for the count rationale. */
    for (uint32_t i = 0U; i < PLATFORM_BT_UART_REENABLE_NOP_COUNT; i++)
    {
        __NOP();
    }
    /* Re-enable the USART peripheral via a direct register write for the same reason as in
     * PLATFORM_EnterLowPower: single atomic read-modify-write, no function-call overhead,
     * minimal instruction count in this timing-sensitive wake-up path. */
    platformHciUsartBase[PLATFORM_BT_UART_INSTANCE]->CFG |= ((uint32_t)USART_CFG_ENABLE_MASK);
}

void PLATFORM_EnterDeepSleep(void)
{
    /* No platform-specific actions are required on MIMXRT685S before entering
     * deep sleep.  The SDK Power Manager handles the hardware sequencing. */
}

void PLATFORM_ExitDeepSleep(void)
{
    /* No platform-specific restore actions are required on MIMXRT685S after
     * exiting deep sleep.  The SDK Power Manager handles the hardware restore. */
}

void PLATFORM_EnterPowerDown(void)
{
    /* Note that this code is never reached,
     * because device re-boot according to power driver. */
}

void PLATFORM_ExitPowerDown(void)
{
    /* Note that this code is never reached,
     * because device re-boot according to power driver. */
}

void PLATFORM_EnterDeepPowerDown(void)
{
    /* Note that this code is never reached,
     * because device re-boot according to power driver. */
}

void PLATFORM_ExitDeepPowerDown(void)
{
    /* Note that this code is never reached,
     * because device re-boot according to power driver. */
}

uint8_t PLATFORM_AllowEnterLowPower(void)
{
#if defined(CONFIG_BT_CONTROLLER_SLEEP)
    return controller_power_state();
#else
    return 1U;
#endif /* CONFIG_BT_CONTROLLER_SLEEP */
}

void RTC_IRQHandler(void)
{
    RTC_ClearStatusFlags(RTC, RTC_GetStatusFlags(RTC));
    SDK_ISR_EXIT_BARRIER;
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

static void PLATFORM_InitWakeUpSources(void)
{
    /* Enable wake up source from BLE controller to wake up the device. */
    PM_InitWakeupSource(&C2HWakeupSource, (uint32_t)GPIO_INTA_IRQn, NULL, true);
}

static void PLATFORM_InitRtc(void)
{
    RTC_Init(RTC);
    RTC_EnableTimer(RTC, false);
    EnableIRQ(RTC_IRQn);
    RTC_EnableWakeUpTimerInterruptFromDPD(RTC, true);
    NVIC_ClearPendingIRQ(RTC_IRQn);

    /* Enable RTC as wakeup source. */
    PM_InitWakeupSource(&timerWakeupSource, (uint32_t)RTC_IRQn, NULL, true);
}

static status_t PLATFORM_LowPowerCallback(pm_event_type_t eventType, uint8_t powerState, void *data)
{
    status_t ret = kStatus_Success;
    (void)data;

    if (eventType == kPM_EventEnteringSleep)
    {
        /* Check if the board is allowed to enter low power mode. */
        /* The time between checking C2H state and pulling up RTS should be as less as possible.
         * Otherwise, when C2H is changed continuously, RTS cannot take action in time, which may cause data loss. */
        if (PLATFORM_AllowEnterLowPower())
        {
            if (powerState >= PLATFORM_DEEP_SLEEP_STATE)
            {
                PLATFORM_EnterLowPower();
            }
            else
            {
                /* Nothing to do when entering WFI or Sleep low power state. */
            }
            platformBoardSleep = true;
        }
        else
        {
            platformBoardSleep = false;
            ret                = kStatus_Fail;
        }
    }
    else
    {
        if (platformBoardSleep == true)
        {
            if (powerState >= PLATFORM_DEEP_SLEEP_STATE)
            {
                PLATFORM_ExitLowPower();
            }
            else
            {
                /* Nothing to do when exit WFI or Sleep low power state. */
            }
        }
    }

    return ret;
}
