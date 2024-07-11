/* -------------------------------------------------------------------------- */
/*                           Copyright 2020-2024 NXP                          */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "board.h"
#include "pin_mux.h"
#include "board_lp.h"
#include "fwk_platform_lowpower.h"
#include "fsl_power.h"
#include "fsl_pm_core.h"
#include "fwk_debug.h"
#include "fsl_usart.h"
#include "fsl_debug_console.h"
#include "clock_config.h"

#ifdef CONFIG_BT_SETTINGS
#include "mflash_drv.h"
#endif
#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
#include "fwk_platform_ble.h"
#include "timers.h"
#endif /* gBoardEnableIdleHold_d */

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
/* The maximum time(ms) for Host maintain active mode to wait PS_SLEEP event form controller */
#define BOARD_IDLE_HOLD_MS 5
#endif /* gBoardEnableIdleHold_d */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Basic lowpower entry call to flush serial transaction and disable peripherals pin to avoid leakage in lowpower
 *    typically called from wakeup from deep sleep
 */
static void BOARD_EnterLowPower(void);

/*!
 * @brief Basic lowpower exit callback to reinitialize clock and pin mux configuration,
 *    typically called from wakeup from deep sleep and other lowest power mode
 */
static void BOARD_ExitLowPower(void);

/*!
 * \brief This function called after exiting Power Down mode on main domain. It should be used to
 *        restore peripherals in main domain used by the application that need specific restore
 *        procedure such as LPUART1, etc..
 * \note  Peripherals in wakeup domain are not concerned. Wakeup domain always remains in Deep Sleep, Sleep,
 *        or Active mode so the HW registers are always retained.
 *
 */
static void BOARD_ExitPowerDown(void);

/*!
 * @brief Configures debug console for low power entry
 *
 */
static void BOARD_UninitDebugConsole(void);

/*!
 * \brief Reinitializes the debug console after low power exit
 *
 */
static void BOARD_ReinitDebugConsole(void);

/*!
 * \brief Callback registered to SDK Power Manager to get notified of entry/exit of low power modes
 *
 * \param[in] eventType event specifying if we entered or exited from low power mode
 * \param[in] powerState low power mode used during low power period
 * \param[in] data Optional data passed when the callback got registered (not used currently)
 * \return status_t
 */
static status_t BOARD_LowpowerCallback(pm_event_type_t eventType, uint8_t powerState, void *data);

#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
/*!
 * \brief Check the Controller state to decide if host is allowed to enter low power mode.
 *        If the Controller is active, start the idle hold timer and wait for some time (Defined by BOARD_IDLE_HOLD_MS)
 *        If the Controller is sleep, stop the idle hold timer.
 *        Should be called before enter low power.
 * \return kStatus_Success on allowed, kStatus_Fail on not allowed.
 *
 */
static status_t BOARD_IdleHoldCheck(void);

/*!
 * \brief Callback for IdleHold Timer
 *
 * \param[in] TimerHandle_t IdleHold Timer Handle
 *
 */
static void BOARD_IdleHoldTimerCallback(TimerHandle_t timer_h);
#endif /* gBoardEnableIdleHold_d */

/* -------------------------------------------------------------------------- */
/*                               Private memory                               */
/* -------------------------------------------------------------------------- */

static pm_notify_element_t boardLpNotifyGroup = {
    .notifyCallback = &BOARD_LowpowerCallback,
    .data           = NULL,
};

#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
static bool volatile idleHoldTO    = false;
static TimerHandle_t idleHoldTimer = NULL;
#endif /* gBoardEnableIdleHold_d */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void BOARD_LowPowerInit(void)
{
    status_t status;

    status = PM_RegisterNotify(kPM_NotifyGroup2, &boardLpNotifyGroup);
    assert(status == kStatus_Success);
    (void)status;

#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
    /* Create FreeRTOS timer which will be used to disable low power after a specific time */
    idleHoldTimer = xTimerCreate("idle hold timer", (TickType_t)(BOARD_IDLE_HOLD_MS / portTICK_PERIOD_MS), pdFALSE,
                                 NULL, BOARD_IdleHoldTimerCallback);
    assert(idleHoldTimer != NULL);
#endif /* gBoardEnableIdleHold_d */
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
static void BOARD_IdleHoldTimerCallback(TimerHandle_t timer_h)
{
    (void)timer_h;

    idleHoldTO = true;
}

static status_t BOARD_IdleHoldCheck(void)
{
    status_t ret = kStatus_Fail;

    if (PLATFORM_IsControllerActive() == true)
    {
        /* Start timer if Controller is active and not timeout before and timer not started */
        if ((idleHoldTO == false) && (xTimerIsTimerActive(idleHoldTimer) != pdTRUE))
        {
            (void)xTimerStart(idleHoldTimer, 0U);
        }
    }
    else
    {
        if (xTimerIsTimerActive(idleHoldTimer) == pdTRUE)
        {
            /* Stop the timer if Controller is in sleep */
            (void)xTimerStop(idleHoldTimer, 0U);
        }
    }

    do
    {
        if (xTimerIsTimerActive(idleHoldTimer) != pdTRUE)
        {
            idleHoldTO = false;
            ret        = kStatus_Success;
            break;
        }
    } while (false);

    return ret;
}
#endif /* gBoardEnableIdleHold_d */

static void BOARD_EnterLowPower(void)
{
    /* Configure debug console for low power entry */
    BOARD_UninitDebugConsole();
}

static void BOARD_ExitLowPower(void)
{
    /* Reinitialize debug console */
    BOARD_ReinitDebugConsole();
}

static void BOARD_ExitPowerDown(void)
{
    /* Clocks and pins need to be reinitialized after wake up from power down */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();

#ifdef CONFIG_BT_SETTINGS
    /* Reinit mflash driver after exit from power down mode */
    (void)mflash_drv_init();
#endif
}

static void BOARD_UninitDebugConsole(void)
{
    /* Wait for debug console output finished. */
    while (((uint32_t)kUSART_TxFifoEmptyFlag & USART_GetStatusFlags(BOARD_DEBUG_UART)) == 0U)
    {
    }
    (void)DbgConsole_EnterLowpower();
}

static void BOARD_ReinitDebugConsole(void)
{
    CLOCK_SetFRGClock(BOARD_DEBUG_UART_FRG_CLK);
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);
    (void)DbgConsole_ExitLowpower();
}

/* -------------------------------------------------------------------------- */
/*                              Low Power callbacks                           */
/* -------------------------------------------------------------------------- */

static status_t BOARD_LowpowerCallback(pm_event_type_t eventType, uint8_t powerState, void *data)
{
    status_t ret = kStatus_Success;

    if (powerState >= PLATFORM_DEEP_SLEEP_STATE)
    {
        if (eventType == kPM_EventEnteringSleep)
        {
            BOARD_EnterLowPower();
#if defined(gBoardEnableIdleHold_d) && (gBoardEnableIdleHold_d == 1)
            /* Check the Controller is asleep or not */
            ret = BOARD_IdleHoldCheck();
#endif /* gBoardEnableIdleHold_d */
        }
        else
        {
            /* Perform recovery context when enter PM3 successfully */
            if ((powerState >= PLATFORM_POWER_DOWN_STATE) && (POWER_GetWakenMode() >= PLATFORM_POWER_DOWN_STATE))
            {
                BOARD_ExitPowerDown();
            }

            BOARD_ExitLowPower();
        }
    }
    else
    {
        /* Nothing to do when entering WFI or Sleep low power state
         * NVIC fully functional to trigger upcoming interrupts */
    }

    return ret;
}
