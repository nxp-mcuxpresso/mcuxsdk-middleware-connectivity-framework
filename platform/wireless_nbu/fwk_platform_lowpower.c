/* -------------------------------------------------------------------------- */
/*                        Copyright 2021-2023, 2025 NXP                       */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdbool.h>

#include "fwk_config.h"
#include "fsl_common.h"
#include "ll_types.h"
#include "fwk_platform_lowpower.h"
#include "fwk_platform.h"
#if defined(gUseSfcRf_d) && (gUseSfcRf_d == 1)
#include "fwk_rf_sfc.h"
#endif
#include "controller_api_ll.h"
#include "fwk_debug.h"
#include "fwk_workq.h"
#include "fwk_platform_ics.h"
#if defined(gPlatformEnableDcdcOnNbu_d) && (gPlatformEnableDcdcOnNbu_d == 1)
#include "fwk_platform_dcdc.h"
#endif

#if defined(PHY_15_4_LOW_POWER_ENABLED) && (PHY_15_4_LOW_POWER_ENABLED == 1)
extern bool PHY_XCVR_AllowLowPower(void);
#endif

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
/* Value form which CONVERT_HALF_SLOTS_2_32KTICKS overflows 32 bits using CONVERT_HALF_SLOTS_2_32KTICKS */
#define OVF_HALF_SLOTS_2_32KTICKS 419430399UL
/* Macro converting number of BLE half-slots to number of 32kHz ticks */
#define CONVERT_HALF_SLOTS_2_32KTICKS(hlf_slot) ((((((hlf_slot) << 2) / 5) << 2) / 5) << 4)
/* Macro converting number of microseconds  to number of BLE half-slots */
#define CONVERT_USEC_2_HALF_SLOTS(usec)   (((usec) << 1) / 625)
#define BLE_MINIMAL_SLEEP_TIME_ALLOWED_HS CONVERT_USEC_2_HALF_SLOTS(5000u) /* 5ms minimal sleep time */
#define BLE_SLP_WUP_MARGIN_HS             CONVERT_USEC_2_HALF_SLOTS(1875u) /* 1.875ms margin */

#define ST_INIT                                0x11U
#define HANDLE_INIT                            0x000EU
#define PLATFORM_SWITCH_SLEEP_CLOCK_TIMEOUT_US 200U

#define PLATFORM_BTRTU1_ISR_ADDR 0xA8009424U

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */
static int  PLATFORM_LowPowerModeAllowed(void);
static bool PLATFORM_IsRemoteAllowingLowPower(void);
static bool PLATFORM_IsBleAllowingLowPower(uint32_t *outSleepTime);
#if defined(PHY_15_4_LOW_POWER_ENABLED) && (PHY_15_4_LOW_POWER_ENABLED == 1)
static bool PLATFORM_Is_15_4_AllowingLowPower(void);
#endif
static void PLATFORM_RemoteLowPowerIndication(bool enter);
static void PLATFORM_ConfigureRamRetention(void);
static void PLATFORM_XtalWarningIndicationWorkHandler(fwk_work_t *work);

/* -------------------------------------------------------------------------- */
/*                               Private memory                               */
/* -------------------------------------------------------------------------- */
static bool    initialized   = false;
static uint8_t chipRevision  = 0xFFU;
static uint8_t delayLpoCycle = 0x3U;
/* bleMinimalSleepTimeAllowedHs number of half-slots */
static uint32_t   bleMinimalSleepTimeAllowedHs = BLE_MINIMAL_SLEEP_TIME_ALLOWED_HS;
static fwk_work_t xtal_warning_ind_work        = {
           .handler = PLATFORM_XtalWarningIndicationWorkHandler,
};

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

#if defined(gPlatformLowpowerEnableWakeUpInterrupt_d) && (gPlatformLowpowerEnableWakeUpInterrupt_d == 1)
void T1_INT_IRQHandler(void)
{
    /* Clear the BTRTU timer 1 interrupt*/
    *(uint32_t *)PLATFORM_BTRTU1_ISR_ADDR &= ~(BTRTU1_BTRTU1_ISR_T1_INT_MASK);
}
#endif

void PLATFORM_LowPowerInit(void)
{
    if (initialized == false)
    {
        /* Retrieve current chip revision (required to perform extra check on
         * A0). 0xFF means the chip revision hasn't been received yet, so it
         * blocks until it is received */
        do
        {
            chipRevision = PLATFORM_GetChipRevision();
        } while (chipRevision == 0xFFU);

        /* This is required for the bt_clk_req signal to be asserted when exiting
         * low power */
        BTU2_REG->BTU_RIF_BLE_CLK_CTRL |= BTU2_REG_BTU_RIF_BLE_CLK_CTRL_BLE_CLK_REQ_EARLY_ASSERT(0x1);

        BLE2_REG->BLE_REG_TMR_WAKEUP_DELAY_LPO_CYCLES = delayLpoCycle;

        /* Enable interrupt of the XTAL ready, necessary to read instant value of XTAL_RDY */
        RF_CMC1->IRQ_CTRL |= RF_CMC1_IRQ_CTRL_RDY_IE_MASK;

#if defined(gPlatformLowpowerEnableWakeUpInterrupt_d) && (gPlatformLowpowerEnableWakeUpInterrupt_d == 1)
        /* Initialize BTRTU timer 1 interrupt (classic bluetooth interrupt) that is not used on this platform. It will
         * be triggered by PLATFORM_RemoteActiveReq() on host core each time it needs to access to NBU power domain */
        BTRTU1->BTRTU1_TIMER1_LEN = 1U;
        BTRTU1->BTRTU1_IMR |= BTRTU1_BTRTU1_IMR_T1_INT_MASK;
        NVIC_EnableIRQ(T1_INT_IRQn);
#endif
        /* WorkQ used to schedule warning/error indications to Host */
        if (WORKQ_InitSysWorkQ() < 0)
        {
            assert(0);
        }
        initialized = true;
    }

    PWR_DBG_LOG("chipRevision: %x", chipRevision);
}

void PLATFORM_EnterLowPower(void)
{
    uint32_t irqMask      = DisableGlobalIRQ();
    int      stateAllowed = PLATFORM_LowPowerModeAllowed();
    if (stateAllowed == 0)
    {
        /* Indicate remote CPU the radio is going to low power */
        PLATFORM_RemoteLowPowerIndication(true);

        /* Check and update the XTAL32M CDAC if needed */
        PLATFORM_UpdateXtal32MTrim();

        /* Configure RAM retention */
        PLATFORM_ConfigureRamRetention();

        /* Execute low power entry procedure */
        PLATFORM_HandleLowPowerEntry();

        /* Indicate remote CPU the radio is active */
        PLATFORM_RemoteLowPowerIndication(false);
    }
    else if (stateAllowed == 1)
    {
        // PWR_DBG_LOG("WFI only is allowed");

        PLATFORM_HandleWFI();
    }
    else
    {
        /* Neither lowpower nor WFI is authorized, it will loop in Idle until one is authorized */
    }

    EnableGlobalIRQ(irqMask);
}

void PLATFORM_SetWakeupDelay(uint8_t wakeupDelayReceivedFromHost)
{
    delayLpoCycle = wakeupDelayReceivedFromHost;
    /* Configure BLE timer wakeup delay of 3.2kHz period */
    BLE2_REG->BLE_REG_TMR_WAKEUP_DELAY_LPO_CYCLES = delayLpoCycle;

    /* Update the variable with the wake-up time of the 32 MHz crystal,
     * adding a margin of a few half-slots to ensure reliable sleep and wake-up timing */
    bleMinimalSleepTimeAllowedHs = wakeupDelayReceivedFromHost + BLE_SLP_WUP_MARGIN_HS;
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Checks from various sources if the radio can enter low power
 *
 * \return 0  : lowpower allowed
 * \return 1  : WFI allowed
 * \return -1 : Neither lowpower nor WFI is allowed
 */
static int PLATFORM_LowPowerModeAllowed(void)
{
    int      ret              = 0;
    uint32_t slp_nb_32k_ticks = 0U;
    do
    {
        uint32_t bleSleepTime = ~0U;
        bool     bleAllowLp   = false;
        if (initialized == false)
        {
            /* Do not allow low power if the module hasn't been initialized */
            ret = 1;
            break;
        }
        bleAllowLp = PLATFORM_IsBleAllowingLowPower(&bleSleepTime);
        if (bleAllowLp == false)
        {
            /* BLE is not allowing low power */
            slp_nb_32k_ticks = 0u;
            if (bleSleepTime > bleMinimalSleepTimeAllowedHs)
            {
                /* Disallow WFI when the LL sleep time is large enough but the LL is still not ready for deep sleep
                 * When the LL is done with an activity, the sleep time can be higher than bleMinimalSleepTimeAllowedHs
                 * meaning we can go to deep sleep, but the LL sched still needs to update its internal state machine so
                 * LL_API_SCHED_IsSleepAllowed can return true
                 * In such case, if we go to WFI, then we will stay in WFI until the next interrupt, even if we could
                 * already go to deep sleep
                 * This condition helps to avoid this case and will let the device loop in idle until the LL sched is
                 * correctly updated */
                ret = -1;
            }
            else
            {
                /* BLE sleep is not allowed or sleep time not large enough */
                ret = 1;
            }
            break;
        }
        if (bleSleepTime < OVF_HALF_SLOTS_2_32KTICKS) /* */
        {
            slp_nb_32k_ticks = CONVERT_HALF_SLOTS_2_32KTICKS(bleSleepTime);
        }
        else
        {
            slp_nb_32k_ticks = ~0UL;
        }
#if defined(PHY_15_4_LOW_POWER_ENABLED) && (PHY_15_4_LOW_POWER_ENABLED == 1)
        if (PLATFORM_Is_15_4_AllowingLowPower() == false)
        {
            /* 15.4 PHY may not be ready for low power */
            ret = 1;
            break;
        }
#endif
        if (PLATFORM_IsRemoteAllowingLowPower() == false)
        {
            /* Remote CPU can prevent low power */
            ret = 1;
            break;
        }
#if defined(gUseSfcRf_d) && (gUseSfcRf_d == 1)
#if (defined FPGA_TARGET && (FPGA_TARGET != 0))
#error "gUseSfcRf_d  not possible"
#endif
        /* SFC_Init must have been run to have SFA clock started */
        if (SFC_IsBusy() == true)
        {
            ret = 1;
            break;
        }
        if (SFC_IsMeasureAvailable() == true)
        {
            ret = -1;
            break;
        }
#endif
    } while (false);
    /* Has no effect on KW45 or KW47 : tell main core how much time there is to insert activities that would stall NBU
     */
    PLATFORM_SetNextNbuActivityDeadline(slp_nb_32k_ticks);
    return ret;
}

/*!
 * \brief Checks if the remote CPU allows radio low power
 *
 * \return true
 * \return false
 */
static bool PLATFORM_IsRemoteAllowingLowPower(void)
{
    bool ret = true;

    do
    {
        if ((RF_CMC1->RADIO_LP & RF_CMC1_RADIO_LP_BLE_WKUP_MASK) != 0U)
        {
            /* Remote CPU can disallow CM3 to go to low power by setting the following
             * bit */
            ret = false;
            break;
        }
    } while (false);

    return ret;
}

/*!
 * \brief Checks if BLE LL allows radio low power
 *
 * \return true
 * \return false
 */
static bool PLATFORM_IsBleAllowingLowPower(uint32_t *outSleepTime)
{
    bool     ret       = true;
    uint32_t sleepTime = 0U;
    do
    {
        uint8_t sleepAllowed = LL_API_SCHED_IsSleepAllowed();

        /* sleepTime is expressed in number of half-slots */
        sleepTime = LL_API_SCHED_GetSleepTime();

#if defined(gPlatformLowpowerCheckHciPendingCommand) && (gPlatformLowpowerCheckHciPendingCommand == 1)
        uint8_t pendingCmd = LL_API_BLE_HCI_GetNofPendingCommand();
#endif

        if (outSleepTime != NULL)
        {
            *outSleepTime = sleepTime;
        }

        if (sleepAllowed != 1u)
        {
            // PWR_DBG_LOG("sleepAllowed %x", sleepAllowed);
            ret = false;
            break;
        }
        PWR_DBG_LOG("Slp allowed, SleepTime=%d pendingCmd=%d", sleepTime, pendingCmd);
        if (sleepTime <= bleMinimalSleepTimeAllowedHs)
        {
            ret = false;
            break;
        }
#if defined(gPlatformLowpowerCheckHciPendingCommand) && (gPlatformLowpowerCheckHciPendingCommand == 1)
        if (pendingCmd > 0U)
        {
            /* Some HCI event are not sent directly when receiving an HCI command
             * The HCI task delays the HCI event and increments the number of pending
             * commands. This should be used to disallow low power so the Host doesnt'
             * wait for the HCI event. */
            PWR_DBG_LOG("pendingCmd %x", pendingCmd);
            ret = false;
            break;
        }
#endif
    } while (false);

    return ret;
}

#if defined(PHY_15_4_LOW_POWER_ENABLED) && (PHY_15_4_LOW_POWER_ENABLED == 1)
/*!
 * \brief Checks if 15.4 LL allows radio low power
 *
 * \return true
 * \return false
 */
static bool PLATFORM_Is_15_4_AllowingLowPower(void)
{
    return PHY_XCVR_AllowLowPower();
}
#endif

/*!
 * \brief Indicates to remote CPU if the radio is entering or exiting low power
 *
 * \param[in] enter true to indicate low power entry, false to indicate low
 *                  power exit
 */
static void PLATFORM_RemoteLowPowerIndication(bool enter)
{
    static uint32_t lowPowerEntryNb = 0U;

    if (enter == true)
    {
        /* CM3 indicates to CM33 it's going to low power so the CM33 will need
         * to request radio wake up if it needs to access radio domain.
         * WOR registers are not used, so we can use them for this purpose.
         * OR with 0x1U to avoid issue when counter wraps. */
        WOR_REGS->WKUP_TIME = ((++lowPowerEntryNb << 1U) | 0x1U) & WOR_WKUP_TIME_WKUP_TIME_MASK;
    }
    else
    {
        /* Notify LL of the NBU wake up */
        LL_API_NotifyWakeUp();
        /* Ensure API call completes before updating register and notifying application core*/
        __ISB();
        /* Indicate to CM33 the radio is awake, so the CM33 can avoid
         * waiting for the radio domain */
        WOR_REGS->WKUP_TIME = 0U;
    }
}

/*!
 * \brief Configures RAM retention for low power period
 *
 */
static void PLATFORM_ConfigureRamRetention(void)
{
    /* Keep all ram banks in retention,  Set SD_EN (Shutdown enable) bits to zero and DS_EN (Deep Sleep enable) bits to
     * one */
    uint32_t ram_pwr = RF_CMC1_RAM_PWR_DS_EN_MASK;
    ram_pwr &= ~RF_CMC1_RAM_PWR_SD_EN_MASK;
    RF_CMC1->RAM_PWR = ram_pwr;
}

int PLATFORM_SwitchSleepClockSource(bool switchTo32k)
{
    int      ret = 0;
    uint64_t start;
    uint64_t now;
    uint64_t duration;

    if (switchTo32k == true)
    {
        CIU2->CIU2_LBC_CTRL |= CIU2_CIU2_LBC_CTRL_MAN_SEL_NCO_MASK;

        start = PLATFORM_GetTimeStamp();

        while (CIU2->CIU2_LBC_CTRL & CIU2_CIU2_LBC_CTRL_LPO_CLK_SEL_FSM_MASK)
        {
            now = PLATFORM_GetTimeStamp();
            if (now > start)
            {
                duration = PLATFORM_GetMaxTimeStamp() - now + start;
            }
            else
            {
                duration = start - now;
            }

            if (duration > PLATFORM_SWITCH_SLEEP_CLOCK_TIMEOUT_US)
            {
                ret = -1;
                break;
            }
        }
    }
    else
    {
        CIU2->CIU2_LBC_CTRL &= ~CIU2_CIU2_LBC_CTRL_MAN_SEL_NCO_MASK;
    }

    return ret;
}

static void PLATFORM_XtalWarningIndicationWorkHandler(fwk_work_t *work)
{
    (void)work;
    NbuEvent_t event = {};

    event.eventType = gNbuWarningXtal32MhzNotReadyAtWakeUp;

    if (PLATFORM_NotifyNbuEvent(&event) < 0)
    {
        assert(0);
    }
}

void PLATFORM_WaitForXtalReady(void)
{
    if ((RF_CMC1->IRQ_CTRL & RF_CMC1_IRQ_CTRL_XTAL_RDY_MASK) == 0U)
    {
        /* This is a critical section. avoid calling RPMSG which may call OS APIs in here */
        /* Postpone the indication and use workqueue to notify the host that the 32MHz crystal is not ready after
         * the NBU wakeup */
        if (WORKQ_Submit(&xtal_warning_ind_work) < 0)
        {
            assert(0);
        }
        /* Wait for the XTAL to be ready before running anything else */
        while ((RF_CMC1->IRQ_CTRL & RF_CMC1_IRQ_CTRL_XTAL_RDY_MASK) == 0U)
        {
        }
    }
}
