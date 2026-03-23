/* -------------------------------------------------------------------------- */
/*                           Copyright 2025-2026 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fwk_platform.h"
#include "nxp2p4_xcvr.h"
#include "fsl_clock.h"
#include "ll_types.h"
#include "controller_api_ll.h"
#include "fwk_platform_lowpower.h"
#include "fwk_platform_mcu_nbu_common.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
#if defined gNbuDisableLowpower_d && (gNbuDisableLowpower_d != 0)
#warning "Beware KW43/MCXW70 NBUs should not define gNbuDisableLowpower_d for correct NVM operations"
#endif

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint32_t csr;
} CoreClkCfg_t;

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

IntercoreSharedCtx_t *p_Nbu_shared_ctx = NULL; /*!< Address of intercore shared context */

/* -------------------------------------------------------------------------- */
/*                              Private functions                              */
/* -------------------------------------------------------------------------- */
static void PLATFORM_SaveNbuClockCfg(CoreClkCfg_t *cfg)
{
    NOT_USED(cfg); /* Not implemented yet */
}

static void PLATFORM_ApplyNbuClkCfg(CoreClkCfg_t *cfg)
{
    /* Not implemented yet */
    NOT_USED(cfg);
}

static void PLATFORM_ApplySlowestNbuClkCfg(void)
{
    /* Not implemented yet : not so simple because register is modifiable by both cores */
    ;
}

static void PLATFORM_CheckReadyToDeepSleep(void)
{
    /* Wait for XCVR DMA completion : backup of unretained configuration ongoing */
    uint32_t timeout = 10000u; /* Reasonable timeout to prevent infinite loop */
    while (XCVR_FastPeriph_WaitComplete() != true)
    {
        timeout--;
        if (timeout == 0u)
        {
            break;
        }
    }
    assert(timeout > 0u);
}
#if defined(PLAT_FWK_INTERCORE_DBG_LP) && (PLAT_FWK_INTERCORE_DBG_LP > 0)
static uint32_t cnt = 0U; /* counter to keep track of the number of entries in Deep Sleep */
#endif

static void PLATFORM_PrepareDeepSleep(void)
{
#if 1
    /* For some unexplained reason, DSB clock config (MRCC CC field) must be set to 3 to
     * let NBU attempt entry in Deep Sleep, even in cases where DSB is unused.
     */
    CLOCK_EnableClockLPMode(kCLOCK_Data_stream_2p4, kCLOCK_IpClkControl_fun3);
#else
    /* Should work but prevents NBU from entering deep sleep */
    CLOCK_DisableClock(kCLOCK_Data_stream_2p4);
#endif

#if defined PLAT_FWK_INTERCORE_DBG_LP && (PLAT_FWK_INTERCORE_DBG_LP > 0)
    cnt++; /* increment deep sleep counter*/
    if (p_Nbu_shared_ctx != NULL)
    {
        p_Nbu_shared_ctx->time_stamp[0]     = (uint32_t)PLATFORM_Get32KTimeStamp();
        p_Nbu_shared_ctx->cnt[0]            = cnt;
        p_Nbu_shared_ctx->rfmc_2p4g_stat[0] = RFMC->RF2P4GHZ_STAT;
    }
#endif
}

static void PLATFORM_ExitDeepSleep(void)
{
#if defined PLAT_FWK_INTERCORE_DBG_LP && (PLAT_FWK_INTERCORE_DBG_LP > 0)
    if (p_Nbu_shared_ctx != NULL)
    {
        p_Nbu_shared_ctx->time_stamp[1]     = (uint32_t)PLATFORM_Get32KTimeStamp();
        p_Nbu_shared_ctx->cnt[1]            = cnt;
        p_Nbu_shared_ctx->rfmc_2p4g_stat[1] = RFMC->RF2P4GHZ_STAT;
    }
#endif
}

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */
uint64_t PLATFORM_Get32KTimeStamp(void)
{
    return PLATFORM_TSTMR_ReadTimeStamp(TSTMR_32KHZ_ID);
}

/*!
 * \brief Compute number of microseconds between 2 timestamps expressed in number of TSTMR 32kHz ticks
 *
 * \param [in] timestamp0 start timestamp from which duration is assessed.
 * \param [in] timestamp1 end timestamp till which duration is assessed.
 *
 * \return uint64_t number of microseconds
 *
 */
uint64_t PLATFORM_Get32KTimeStampDeltaUs(uint64_t timestamp0, uint64_t timestamp1)
{
    uint64_t duration_us;

    duration_us = PLATFORM_GetTstmrDeltaTicks(timestamp0, timestamp1);
    /* Prevent overflow */
    duration_us &= PLATFORM_TSTMR_MAX_VAL;
    /* Normally useless but let Coverity know that the input is necessarily less than 2^56 */
    /* Multiply by 1000000 (2^6 * 5^6) and divide by 32768 (2^15) can be be simplified to Multiplication by 125*125 and
     * division by 512 */
    /* Multiply by 125, inserting the division by 64 and then multiply again by 125 and finally divide by 8 prevents the
     * overflow considering the argument is smaller than 2^56
     */
    duration_us *= 125ULL; /* Since timestamps are no more than 56 bit wide, multiplying by 125 is smaller than 2^63 */
    duration_us >>= 6;     /* Dividing by 64 (2^6) yields a result strictly smaller than 2^57 */
    duration_us *= 125ULL; /* Multiplying by 125 is necessarily strictly smaller than 2^64-1 */
    duration_us >>= 3;     /* Divide by 8 (2^3)  */

    return duration_us;
}

/*
 *
 */
void PLATFORM_SetNextNbuActivityDeadline(uint32_t duration_32kHz_tick)
{
    if (p_Nbu_shared_ctx != NULL)
    {
        uint64_t ts_32k;
        /*
         * Once the CORE1_TS_CHANGE_ONGOING bit is raised, if core#0 happens to be reading p_Nbu_shared_ctx->ts simultaneously,
         * it would spinlock waiting for it to go low. Should a core#1 interrupt fires between MSB and LSB write on core#1, core#0 would be stuck
         * for the duration of the exception - not to mention possible occurrence of faults in this section.
         * For the above reasons, but also for the sake of timestamp precision, writing of p_Nbu_shared_ctx->ts is done under critical section.
         */
        uint32_t exe_state;
        exe_state                       = DisableGlobalIRQ();
        p_Nbu_shared_ctx->ts.u32.ts_msb = CORE1_TS_CHANGE_ONGOING;
        __ISB();
        __DSB();
        if (duration_32kHz_tick == ~0UL)
        {
            /* Time to next activity is infinite */
            p_Nbu_shared_ctx->ts.u32.ts_lsb = TSTMR_L_VALUE_MASK;
            __ISB();
            __DSB();
            p_Nbu_shared_ctx->ts.u32.ts_msb = (CORE1_NO_CONSTRAINT | TSTMR_H_VALUE_MASK);
        }
        else
        {
            ts_32k = PLATFORM_Get32KTimeStamp();

            ts_32k += (uint64_t)duration_32kHz_tick;

            p_Nbu_shared_ctx->ts.u32.ts_lsb = EXTRACT_TSTMR_LSB32(ts_32k);
#if !defined(gPlatformTstmr32Bit_d) || (gPlatformTstmr32Bit_d == 0)
            p_Nbu_shared_ctx->ts.u32.ts_msb = EXTRACT_TSTMR_MSB32(ts_32k);
#else
            p_Nbu_shared_ctx->ts.u32.ts_msb =
                0UL; /* has the effect of clearing CORE1_NO_CONSTRAINT and CORE1_TS_CHANGE_ONGOING */
#endif
        }
        EnableGlobalIRQ(exe_state);
    }
}

void PLATFORM_NBU_Busy(void)
{
    if (p_Nbu_shared_ctx != NULL)
    {
        uint32_t exe_state;
        exe_state                       = DisableGlobalIRQ();
        p_Nbu_shared_ctx->ts.u32.ts_msb = CORE1_TS_CHANGE_ONGOING;
        __ISB();
        __DSB();
        p_Nbu_shared_ctx->ts.u32.ts_lsb = 0UL;
        __ISB();
        __DSB();
        p_Nbu_shared_ctx->ts.u32.ts_msb = 0UL;
        EnableGlobalIRQ(exe_state);
    }
}

void PLATFORM_RxNbuSharedCtxAddress(uint8_t *data, uint32_t len)
{
    uint32_t val = 0U;
    if ((data != NULL) && (len >= (1u + sizeof(void *))))
    {
        /* Copy address from unaligned byte array */
        val |= data[1];
        val |= (data[2] << 8);
        val |= (data[3] << 16);
        val |= (data[4] << 24);
        p_Nbu_shared_ctx = (IntercoreSharedCtx_t *)val;
    }
}

uint32_t PLATFORM_GetNbuFreq(void)
{
    return CLOCK_GetFreq(kCLOCK_PlatClk); /* check CLOCK_GetPlatClkFreq */
}

void PLATFORM_PreDeepSleepSaveCfg(void)
{
    (void)XCVR_LIF_RBME_Backup();
}

void PLATFORM_PostWakeupRestoreCfg(void)
{
    (void)XCVR_LIF_RBME_Restore();
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
        /* In the current state, does nothing more but the plain WFI, without any optimization */
        CoreClkCfg_t nbuclk_cfg;

        PLATFORM_SaveNbuClockCfg(&nbuclk_cfg);
        PLATFORM_ApplySlowestNbuClkCfg();

        /* WFI may trigger low power entry procedure */
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
    /* Update intercore time indication to current time now after wakeup */
    PLATFORM_NBU_Busy();
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
        CoreClkCfg_t nbuclk_cfg;
        PLATFORM_PreDeepSleepSaveCfg();

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

        PLATFORM_CheckReadyToDeepSleep(); /* Has DMA completed yet ? */
        PLATFORM_PrepareDeepSleep();

        PLATFORM_ApplySlowestNbuClkCfg();

        /* WFI will trigger low power entry procedure */
        __DSB();
        __WFI();
        __ISB();

        /* Needs to be cleared first after exiting lowpower mode */
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        __ISB();

        /* On wake up restore original clock */
        PLATFORM_ApplyNbuClkCfg(&nbuclk_cfg);

        /* Unset bit to prevent lowpower now */
        RF_CMC1->RADIO_LP &= ~RF_CMC1_RADIO_LP_SLEEP_EN_MASK;

        /* Re-enable systicks */
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

        PLATFORM_ExitDeepSleep();

        /* Update intercore time indication to current time now after wakeup */
        PLATFORM_NBU_Busy();

        PLATFORM_PostWakeupRestoreCfg();

        PLATFORM_WaitForXtalReady();
    }

    /* Set sleep clock source to auto */
    PLATFORM_SwitchSleepClockSource(false);
}
