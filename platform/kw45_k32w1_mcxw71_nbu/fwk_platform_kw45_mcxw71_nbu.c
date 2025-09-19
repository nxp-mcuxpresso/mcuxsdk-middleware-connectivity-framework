/* -------------------------------------------------------------------------- */
/*                           Copyright 2025 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fwk_platform.h"
#include "EmbeddedTypes.h"
#include "fsl_clock.h"
#include "ll_types.h"
#include "controller_api_ll.h"
#include "fwk_platform_lowpower.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t clock_ctrl;
    uint32_t div;
} Fro192Cfg_t;
/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
const Fro192Cfg_t slowest_fro_cfg = {.clock_ctrl = 0U, .div = FRO192M_FRODIV_FRODIV_MASK};

/* -------------------------------------------------------------------------- */
/*                              Private functions                              */
/* -------------------------------------------------------------------------- */
static void PLATFORM_SaveNbuClockCfg(Fro192Cfg_t *cfg)
{
    cfg->clock_ctrl = FRO192M0->FROCCSR;
    cfg->div        = FRO192M0->FRODIV;
}

static void PLATFORM_ApplyNbuClkCfg(const Fro192Cfg_t *cfg)
{
    /* The host allows the NBU to downgrade its frequency to 16MHz
     * So we can select the 16MHz range and use the highest divider */
    FRO192M0->FROCCSR = cfg->clock_ctrl;
    FRO192M0->FRODIV  = cfg->div;
}

static void PLATFORM_ApplySlowestNbuClkCfg(void)
{
    /* The host allows the NBU to downgrade its frequency to 16MHz
     * So we can select the 16MHz range and use the highest divider */
    PLATFORM_ApplyNbuClkCfg(&slowest_fro_cfg);
}

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */
/*!
 * \brief Get 32kHz timestamp from 32kHz counter
 *
 * \return uint64_t number ticks since start of counter.
 *
 */
uint64_t PLATFORM_Get32KTimeStamp(void)
{
    /* 32kHz TSTMR not instantiated on KW45/MCXW71 */
    return 0ULL;
}

/*!
 * \brief Compute number of microseconds between 2 timestamps expressed in number of TSTM 32kHz ticks
 *
 * \param [in] timestamp0 start timestamp from which duration is assessed.
 * \param [in] timestamp1 end timestamp till which duration is assessed.
 *
 * \return uint64_t number of microseconds
 *
 */
uint64_t PLATFORM_Get32KTimeStampDeltaUs(uint64_t timestamp0, uint64_t timestamp1)
{
    NOT_USED(timestamp0);
    NOT_USED(timestamp1);
    /* 32kHz TSTMR not instantiated on KW45/MCXW71 */
    return 0ULL;
}

void PLATFORM_SetNextNbuActivityDeadline(uint32_t duration_32kHz_tick)
{
    /* KW45 / MCXW71 stub - Not needed */
    NOT_USED(duration_32kHz_tick);
}

uint32_t PLATFORM_GetNbuFreq(void)
{
    return CLOCK_GetRfFro192MFreq();
}

/*!
 * \brief Handles Entry/Exit WFI to save minimal amount of power for short period of time
 *
 */
void PLATFORM_HandleWFI(void)
{
    /* Make sure core Deep Sleep is disabled */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    if (PLATFORM_GetFrequencyConstraintFromHost() == 0U)
    {
        Fro192Cfg_t nbuclk_cfg;

        PLATFORM_SaveNbuClockCfg(&nbuclk_cfg);
        PLATFORM_ApplySlowestNbuClkCfg();

        /* WFI will trigger low power entry procedure */
        __DSB();
        __WFI();
        __ISB();

        /* On wake up restore original clock */
        PLATFORM_ApplyNbuClkCfg(&nbuclk_cfg);
    }
    else
    {
        /* Enter simple WFI to save minimal power */
        __DSB();
        __WFI();
        __ISB();
    }
}

/*!
 * \brief Starts the low power entry procedure
 *
 */
void PLATFORM_HandleLowPowerEntry(void)
{
    /* Force sleep clock source to 32k */
    if (PLATFORM_SwitchSleepClockSource(true) == 0)
    {
        Fro192Cfg_t nbuclk_cfg;

        /* Disable and clear Systicks */
        SysTick->CTRL = 0U;
        SysTick->VAL  = SysTick->LOAD;
        /* Clear pending status of the systick interrupt to avoid having an unrequired wakeup if the systick was pending
         * before disabling it */
        SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

        SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

        /* update sleep entry timestamp: required to know if native clock is valid after wakeup (HW issue: LL-2953) */
        LL_API_UpdateLastNativeClkBeforeSleep();

        /* Request low power entry to RFMC */
        RF_CMC1->RADIO_LP |= RF_CMC1_RADIO_LP_SLEEP_EN_MASK;

        PLATFORM_SaveNbuClockCfg(&nbuclk_cfg);
        PLATFORM_ApplySlowestNbuClkCfg();

        /* WFI will trigger low power entry procedure */
        __DSB();
        __WFI();
        __ISB();

        /* Needs to be cleared first after exiting lowpower (HW issue: MCSE-484) */
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        __ISB();

        /* On wake up restore original clock */
        PLATFORM_ApplyNbuClkCfg(&nbuclk_cfg);

        /* Unset bit to prevent lowpower now */
        RF_CMC1->RADIO_LP &= ~RF_CMC1_RADIO_LP_SLEEP_EN_MASK;

        /* Re-enable systicks */
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

        PLATFORM_WaitForXtalReady();
    }

    /* Set sleep clock source to auto */
    PLATFORM_SwitchSleepClockSource(false);
}
