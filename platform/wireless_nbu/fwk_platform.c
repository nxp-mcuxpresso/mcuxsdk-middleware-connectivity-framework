/*
 * Copyright 2020-2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*${header:start}*/
#include "fwk_platform.h"
#include "fwk_debug.h"
#include "nxp2p4_xcvr.h"

#if !defined(BOARD_DBGPLTIOSET)
#define BOARD_DBGPLTIOSET(__x, __y)
#endif

#include "FunctionLib.h"

#if (defined(gPlatformUseLptmr_d)) && (gPlatformUseLptmr_d == 1U)
#include "fsl_component_timer_manager.h"
#endif

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/

/** IFR1 - Layout Typedef */
typedef struct
{
    uint8_t RESERVED_0[6160];
    struct
    {
        volatile uint32_t PHRASE[4];
    } SOCTRIM1; /**< SOCTRIM1 IFR field, offset: 0x1810 */
} IFR1_Type;

/************************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
************************************************************************************/

#define IFR1_BASE (0x00048000U)
#define IFR1      ((IFR1_Type *)IFR1_BASE)

#define PLATFORM_SOC_ACTIVE_DELAY_US 120U

#define PLATFORM_HOST_USE_POWER_DOWN (0xA5A5A5A5U)

/*! @brief The configuration of timer. */
#define PLATFORM_TM_INSTANCE 2U

#ifndef PLATFORM_TM_CLK_FREQ
#define PLATFORM_TM_CLK_FREQ 32768U
#endif

#ifndef PLATFORM_TM_CLK_SELECT
#define PLATFORM_TM_CLK_SELECT 2U /*Lptmr timer use (kLPTMR_PrescalerClock_2) 32k clock*/
#endif

#define RAISE_ERROR(__st, __error_code) -(int)((uint32_t)(((uint32_t)(__st) << 4) | (uint32_t)(__error_code)));

/************************************************************************************
 * Private memory declarations
 ************************************************************************************/

/* We have to adapt the divide register of the FRO depending the frequency of the core to keep a frequency for flash APB
 * clock & RF_CMC clock between 16MHz and 32MHz */
static const uint8_t PLATFORM_FroDiv[] = {1U, 1U, 2U, 2U, 3U};

/* Tells if the 32MHz Xtal trim value has been updated by CM33 (HWParameters) */
static bool_t xtal32MhzTrimInitialized = FALSE;

static volatile int8_t active_request_nb = 0;
static uint8_t         chip_revision     = 0xFFU;

/* By default the frequency constraints are 16MHz */
static uint8_t freq_constraint_from_host       = 0U;
static uint8_t freq_constraint_from_controller = 0U;

static volatile int timer_manager_initialized = 0;

static void PLATFORM_UpdateFrequency(void);
static void PLATFORM_SetClock(uint8_t range);

static void PLATFORM_RemoteActiveReqOptionalDelay(bool withDelay);

/************************************************************************************
 * Public functions
 ************************************************************************************/

void PLATFORM_RemoteActiveReq(void)
{
    PLATFORM_RemoteActiveReqOptionalDelay(true);
}

void PLATFORM_RemoteActiveReqWithoutDelay(void)
{
    PLATFORM_RemoteActiveReqOptionalDelay(false);
}

void PLATFORM_RemoteActiveRel(void)
{
    uint32_t intMask = PLATFORM_SET_INTERRUPT_MASK();

    // PWR_DBG_LOG("<--active_request_nb RELEASE=%d", active_request_nb);

    assert(active_request_nb > 0);
    active_request_nb--;

    if (active_request_nb == 0)
    {
        RF_CMC1->SOC_LP = 0;
    }
    else
    {
    }

    PLATFORM_CLEAR_INTERRUPT_MASK(intMask);
}

uint8_t PLATFORM_GetXtal32MhzTrim(void)
{
    uint8_t ret = 0xFFU;

    if (xtal32MhzTrimInitialized == TRUE)
    {
        ret = XCVR_GetXtalTrim();
    }

    return ret;
}

void PLATFORM_SetXtal32MhzTrim(uint8_t trim)
{
    XCVR_SetXtalTrim(trim);

    if (xtal32MhzTrimInitialized == FALSE)
    {
        xtal32MhzTrimInitialized = TRUE;
    }
}

void PLATFORM_SetChipRevision(uint8_t chip_rev_l)
{
    chip_revision = chip_rev_l;

    PWR_DBG_LOG("chip rev received:%d", chip_revision);
}

uint8_t PLATFORM_GetChipRevision(void)
{
    return chip_revision;
}

uint64_t PLATFORM_GetTimeStamp(void)
{
    return (uint64_t)COUNT_TO_USEC(SysTick->VAL, CLOCK_GetRfFro192MFreq());
}

uint64_t PLATFORM_GetMaxTimeStamp(void)
{
    return (uint64_t)COUNT_TO_USEC(SysTick->LOAD, CLOCK_GetRfFro192MFreq());
}

void PLATFORM_WaitTimeout(uint64_t timestamp, uint64_t delayUs)
{
    uint64_t now, duration;
    do
    {
        now = PLATFORM_GetTimeStamp();

        if (now > timestamp)
        {
            /* Handle counter wrapping */
            duration = PLATFORM_GetMaxTimeStamp() - now + timestamp;
        }
        else
        {
            duration = timestamp - now;
        }
    } while (duration < delayUs);
}

void PLATFORM_Delay(uint64_t delayUs)
{
    /* PLATFORM_Delay() is similar to PLATFORM_WaitTimeout() but timestamp is taken right now */
    PLATFORM_WaitTimeout(PLATFORM_GetTimeStamp(), delayUs);
}

void PLATFORM_InitFro192M(void)
{
    uint64_t soctrim1_01;
    uint32_t ifr1_soctrim1_0 = IFR1->SOCTRIM1.PHRASE[0];
    uint32_t ifr1_soctrim1_1 = IFR1->SOCTRIM1.PHRASE[1];

    /* check if NBU IFR1 is blank (no SOCTRIM1 values) */
    if ((ifr1_soctrim1_0 == 0xFFFFFFFFUL) && (ifr1_soctrim1_1 == 0xFFFFFFFFUL))
    {
        /* use default values */
        ifr1_soctrim1_0 = 0x14ffffff;
        ifr1_soctrim1_1 = 0xfffffdba;
    }

    /* Workaround to solve IFR load issue leading to FRO192M using a wrong trim value, See KFOURWONE_3731
     * On impacted parts, this workaround will correct the clock rate from ~24M to expected 32M
     * The FROTRIM can be retrieved from IFR1, in the SOCTRIM1 field (16 bytes)
     * The FROTRIM is written in SOCTRIM1 in the following order:
     * TRIMTEMP = SOCTRIM1[27:22]
     * TRIMCOAR = SOCTRIM1[33:28]
     * TRIMFINE = SOCTRIM1[41:34]
     * We have to read the 2 first 32bits phrases from SOCTRIM1 in order to recover the FROTRIM value
     * */
    soctrim1_01 = (uint64_t)(ifr1_soctrim1_0) | (((uint64_t)(ifr1_soctrim1_1)) << 32);

    FRO192M0->FROTRIM = FRO192M_FROTRIM_TRIMTEMP((uint32_t)((soctrim1_01 >> 22) & 0x3FU)) |
                        FRO192M_FROTRIM_TRIMCOAR((uint32_t)((soctrim1_01 >> 28) & 0x3FU)) |
                        FRO192M_FROTRIM_TRIMFINE((uint32_t)((soctrim1_01 >> 34) & 0xFFU));
}

void PLATFORM_SetFrequencyConstraintFromHost(uint8_t freq_constraint)
{
    freq_constraint_from_host = freq_constraint;
    PLATFORM_UpdateFrequency();
}

void PLATFORM_SetFrequencyConstraintFromController(uint8_t freq_constraint)
{
    freq_constraint_from_controller = freq_constraint;
    PLATFORM_UpdateFrequency();
}

uint8_t PLATFORM_GetFrequencyConstraintFromHost(void)
{
    return freq_constraint_from_host;
}

#if (defined(gPlatformUseLptmr_d)) && (gPlatformUseLptmr_d == 1U)

int PLATFORM_InitTimerManager(void)
{
    int status = 0;

    if (timer_manager_initialized == 0)
    {
        timer_status_t tm_st;
        timer_config_t timerConfig;

        timerConfig.instance       = PLATFORM_TM_INSTANCE;
        timerConfig.srcClock_Hz    = PLATFORM_TM_CLK_FREQ;
        timerConfig.clockSrcSelect = PLATFORM_TM_CLK_SELECT;

#if (defined(TM_ENABLE_TIME_STAMP) && (TM_ENABLE_TIME_STAMP > 0U))
        timerConfig.timeStampSrcClock_Hz    = PLATFORM_TM_STAMP_CLK_FREQ;
        timerConfig.timeStampInstance       = PLATFORM_TM_STAMP_INSTANCE;
        timerConfig.timeStampClockSrcSelect = PLATFORM_TM_STAMP_CLK_SELECT;
#endif

        tm_st = TM_Init(&timerConfig);
        if (tm_st != kStatus_TimerSuccess)
        {
            status = RAISE_ERROR(tm_st, 1);
            assert(0);
        }
        else
        {
            PLATFORM_RemoteActiveReq();
            /* Enable radio wakeup by lptimer in RFMC */
            RFMC->RF2P4GHZ_CFG |= RFMC_RF2P4GHZ_CFG_RFMC_EXT_WAKEUP_EN(0x1U);
            PLATFORM_RemoteActiveRel();

            /* Timer Manager initialization completed */
            timer_manager_initialized = 1;
        }
    }
    else
    {
        /* Timer Manager already initialized */
        status = 1;
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
#endif

/************************************************************************************
 * Private functions
 ************************************************************************************/

static void PLATFORM_UpdateFrequency(void)
{
    /* Take the higher frequency between the constraint set by the controller and the one from the host and apply it */
    uint8_t frequency = (freq_constraint_from_host > freq_constraint_from_controller) ? freq_constraint_from_host :
                                                                                        freq_constraint_from_controller;
    PLATFORM_SetClock(frequency);
}

static void PLATFORM_SetClock(uint8_t range)
{
    uint32_t fro192M_clock_ctrl;
    uint32_t fro192M_div;

    fro192M_clock_ctrl = FRO192M0->FROCCSR;
    fro192M_clock_ctrl &= ~(FRO192M_FROCCSR_POSTDIV_SEL_MASK);

    fro192M_div = FRO192M0->FRODIV;
    fro192M_div &= ~(FRO192M_FRODIV_FRODIV_MASK);

    if (range <= 4U)
    {
        fro192M_clock_ctrl |= FRO192M_FROCCSR_POSTDIV_SEL(range);
        fro192M_div |= FRO192M_FRODIV_FRODIV(PLATFORM_FroDiv[range]);
    }
    else
    {
        /* The range selected by default is 32Mhz and 16MHz for flash APB clock & RF_CMC clock*/
        fro192M_clock_ctrl |= FRO192M_FROCCSR_POSTDIV_SEL(2);
        fro192M_div |= FRO192M_FRODIV_FRODIV(PLATFORM_FroDiv[2]);
    }

    FRO192M0->FROCCSR = fro192M_clock_ctrl;
    FRO192M0->FRODIV  = fro192M_div;

    /* Wait for RF FRO192M clock to be valid. */
    while ((FRO192M0->FROCCSR & FRO192M_FROCCSR_VALID_MASK) != FRO192M_FROCCSR_VALID_MASK)
    {
    }
}

static uint32_t PLATFORM_GetLowPowerFlag(void)
{
    extern uint32_t    m_lowpower_flag_start[]; /* defined by linker */
    volatile uint32_t *p_lp_flag = (volatile uint32_t *)(uint32_t)m_lowpower_flag_start;

    return *p_lp_flag;
}

static void PLATFORM_RemoteActiveReqOptionalDelay(bool withDelay)
{
    uint32_t intMask = PLATFORM_SET_INTERRUPT_MASK();

    if (active_request_nb == 0)
    {
        /* Request access to SOC XBAR bus - only this bit is writeable */
        RF_CMC1->SOC_LP = RF_CMC1_SOC_LP_BUS_REQ(0x1);

        while ((RF_CMC1->SOC_LP & RF_CMC1_SOC_LP_BUS_AWAKE_MASK) == 0)
        {
            asm("NOP");
        }
        /* If the host is in power down, we need to wait an additional delay on OEM part for the SOC to be ready */
        if ((PLATFORM_GetLowPowerFlag() == PLATFORM_HOST_USE_POWER_DOWN) && withDelay)
        {
            PLATFORM_Delay(PLATFORM_SOC_ACTIVE_DELAY_US);
        }
    }

    active_request_nb++;

    PLATFORM_CLEAR_INTERRUPT_MASK(intMask);

    // PWR_DBG_LOG("-->active_request_nb REQUEST=%d", active_request_nb);
}
/*${function:end}*/
