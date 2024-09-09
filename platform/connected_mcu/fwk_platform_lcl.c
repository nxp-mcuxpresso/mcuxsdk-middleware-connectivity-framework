/*!
 * Copyright 2021, 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fwk_platform_lcl.h"
#include "FunctionLib.h"
#if defined(gMWS_Enabled_d) && (gMWS_Enabled_d > 0)
#include "MWS.h"
#endif
#include "RNG_Interface.h"

#if defined(gAppLowpowerEnabled_d) && (gAppLowpowerEnabled_d > 0)
// TODO : remove dependancy
#include "board_lp.h"
#endif

#include "pin_mux.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

/*******************************************************************************
 * Private macros
 ******************************************************************************/
#define DTEST_RSM0     (0x04)
#define DTEST_RSM1     (0x05)
#define DTEST_RSM2     (0x06)
#define DTEST_XCVR_DMA (0x07)
#define DTEST_RX_IQ    (0x17)
#define DTEST_NBU_LL   (0x43)

#define DTEST_OVRD_TSM_COMBO_RX_EN 0x5U

/* DTEST signal selectors for use with dtest_pins_init() */
#define DTEST0    (1UL << 0UL)
#define DTEST1    (1UL << 1UL)
#define DTEST2    (1UL << 2UL)
#define DTEST3    (1UL << 3UL)
#define DTEST4    (1UL << 4UL)
#define DTEST5    (1UL << 5UL)
#define DTEST6    (1UL << 6UL)
#define DTEST7    (1UL << 7UL)
#define DTEST8    (1UL << 8UL)
#define DTEST9    (1UL << 9UL)
#define DTEST10   (1UL << 10UL)
#define DTEST11   (1UL << 11UL)
#define DTEST12   (1UL << 12UL)
#define DTEST13   (1UL << 13UL)
#define DTEST_ALL (0x1FFFUL)

/* DTEST5 conflicts with PTC1 (SW2) on localization board */
#if ((defined(BOARD_LOCALIZATION_REVISION_SUPPORT)) && BOARD_LOCALIZATION_REVISION_SUPPORT)
#undef DTEST1
#define DTEST1 0UL
#endif

/*
 * DTEST2 & DTEST3 conflict with UART1
 * DTEST4 (PTC4) is used for GPIO debug on CM33
 * DTEST6 conflicts with PTC6 (SW3)
 * DTEST7 (aa_matched_to_ll in DTEST_RSM2) conflicts with PTB0 = monochrome LED (LED2)
 * PTA19, PTA20, PTA21 (RGB LED) conflicts with RF_GPO (antenna switching)
 */
#if (defined(DEFAULT_APP_UART) && (DEFAULT_APP_UART == 0))
#define DTEST_LIST_BASE                                                                                            \
    (DTEST0 | DTEST1 | DTEST2 | DTEST3 | /* DTEST4 |*/ DTEST5 | /*DTEST7 |*/ DTEST8 | DTEST9 | DTEST10 | DTEST11 | \
     DTEST12 | DTEST13)
#else
#define DTEST_LIST_BASE                                                                                                \
    (DTEST0 | DTEST1 | /*DTEST2 | DTEST3 |*/ /* DTEST4 |*/ DTEST5 | /*DTEST7 |*/ DTEST8 | DTEST9 | DTEST10 | DTEST11 | \
     DTEST12 | DTEST13)
#endif

#if (!defined(gAppLedCnt_c) || (gAppLedCnt_c == 0))
#define DTEST_LIST (DTEST_LIST_BASE | DTEST7) /* DTEST7 may be used as no LED is active */
#else
#define DTEST_LIST DTEST_LIST_BASE
#endif

/*******************************************************************************
 * Private memory declarations
 ******************************************************************************/

/*******************************************************************************
 *  private functions
 ******************************************************************************/
#if !defined(NDEBUG)
static void dtest_pins_init(uint32_t option)
{
    if ((option & DTEST0) != 0U) /* PTC0 alt 8 */
    {
        PORT_SetPinMux(PORTC, 0U, kPORT_MuxAlt8);
    }
    if ((option & DTEST1) != 0U) /* PTC1 alt 8 */
    {
        PORT_SetPinMux(PORTC, 1U, kPORT_MuxAlt8);
    }
    if ((option & DTEST2) != 0U) /* PTC2 alt 8 */
    {
        PORT_SetPinMux(PORTC, 2U, kPORT_MuxAlt8);
    }
    if ((option & DTEST3) != 0U) /* PTC3 alt 8 */
    {
        PORT_SetPinMux(PORTC, 3U, kPORT_MuxAlt8);
    }
    if ((option & DTEST4) != 0U) /* PTC4 alt 8 */
    {
        PORT_SetPinMux(PORTC, 4U, kPORT_MuxAlt8);
    }
    if ((option & DTEST5) != 0U) /* PTC5 alt 8 */
    {
        PORT_SetPinMux(PORTC, 5U, kPORT_MuxAlt8);
    }
    if ((option & DTEST6) != 0U) /* PTC6 alt 8 */
    {
        PORT_SetPinMux(PORTC, 6U, kPORT_MuxAlt8);
    }
    if ((option & DTEST7) != 0U) /* PTB0 alt 8*/
    {
        PORT_SetPinMux(PORTB, 0U, kPORT_MuxAlt8);
    }
    if ((option & DTEST8) != 0U) /* PTB1 alt 8 */
    {
        PORT_SetPinMux(PORTB, 1U, kPORT_MuxAlt8);
    }
    if ((option & DTEST9) != 0U) /* PTB2 alt 8 */
    {
        PORT_SetPinMux(PORTB, 2U, kPORT_MuxAlt8);
    }
    if ((option & DTEST10) != 0U) /* PTB3 alt 8 */
    {
        PORT_SetPinMux(PORTB, 3U, kPORT_MuxAlt8);
    }
    if ((option & DTEST11) != 0U) /* PTB4 alt 8 */
    {
        PORT_SetPinMux(PORTB, 4U, kPORT_MuxAlt8);
    }
    if ((option & DTEST12) != 0U) /* PTCB5 alt 8  */
    {
        PORT_SetPinMux(PORTB, 5U, kPORT_MuxAlt8);
    }
    if ((option & DTEST13) != 0U) /* PTC7 alt 8 */
    {
        PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt8);
    }
}

static void set_dtest_page(uint32_t dtest_page)
{
    uint32_t temp;

    /* Set the DTEST page selection and DTEST enable */
    temp = RADIO_CTRL->DTEST_CTRL;
    temp &= ~(RADIO_CTRL_DTEST_CTRL_DTEST_PAGE_MASK | RADIO_CTRL_DTEST_CTRL_DTEST_EN_MASK);
    temp |= RADIO_CTRL_DTEST_CTRL_DTEST_PAGE(dtest_page) | RADIO_CTRL_DTEST_CTRL_DTEST_EN(1);
    RADIO_CTRL->DTEST_CTRL = temp;
}

static void PLATFORM_InitLclDtest(void)
{
    const gpio_pin_config_t config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic  = 0,
    };

    /* TRDC initialized during NBU initialization */

    /* Configure PTD3 and PTD2 as GPIO (may be used for debug purpose by the NBU) */
    PORT_SetPinMux(PORTD, 3U, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTD, 2U, kPORT_MuxAsGpio);
    GPIO_PinInit(GPIOD, 3U, &config);
    GPIO_PinInit(GPIOD, 2U, &config);

    /* Enable GPIO for CM33 debugging */
    PORT_SetPinMux(PORTC, 4U, kPORT_MuxAsGpio);
    GPIO_PinInit(GPIOC, 4U, &config);

    // dtest_page usually one of DTEST_RSM2, DTEST_NBU_LL, DTEST_RX_IQ
    set_dtest_page(DTEST_RSM2);
    dtest_pins_init(DTEST_LIST);

#ifdef BT51_TEST_MODE_AOA_AOD
    PORT_SetPinMux(PORTD, 1U, kPORT_MuxAlt4);
    PORT_SetPinMux(PORTD, 2U, kPORT_MuxAlt4);
    PORT_SetPinMux(PORTD, 3U, kPORT_MuxAlt4);

    // See Generation 4.5 Radio Platform Manual, Figure 78. Radio Coexistence/FEM/LANT Mux diagram
    // LCL_GPIO_SEL: 1b - Select NBU requestAntenna[3:0] as radio lant_gpio[3:0] output.
    XCVR_MISC->LCL_CFG0 =
        (XCVR_MISC->LCL_CFG0 & ~XCVR_MISC_LCL_CFG0_LCL_GPIO_SEL_MASK) | XCVR_MISC_LCL_CFG0_LCL_GPIO_SEL(1);

    /* RFMC_RFGPO_SRC[2:0]
      010b - RF_GPO[7:0] = {lant_lut_gpio[3:0], fem_ctrl[3:0]}
      011b - RF_GPO[7:0] = {fem_ctrl[3:0], lant_lut_gpio[3:0]}
      100b - RF_GPO[7:0] = {lant_lut_gpio[3:0], coext[3:0]}
      101b - RF_GPO[7:0] = {coext[3:0], lant_lut_gpio[3:0]} */
    RFMC->RF2P4GHZ_COEXT =
        (RFMC->RF2P4GHZ_COEXT & ~RFMC_RF2P4GHZ_COEXT_RFGPO_SRC_MASK) | RFMC_RF2P4GHZ_COEXT_RFGPO_SRC(2);

    // Enable RF_GPO[7:4]
    RFMC->RF2P4GHZ_COEXT =
        (RFMC->RF2P4GHZ_COEXT & ~RFMC_RF2P4GHZ_COEXT_RFGPO_OBE_MASK) | RFMC_RF2P4GHZ_COEXT_RFGPO_OBE(0xf0);

#ifdef BT51_TEST_MODE_AOA_AOD_DTEST
    // Enable DTEST
    // CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortC);
    // CLOCK_EnableClock(kCLOCK_PortD);

    PORT_SetPinMux(PORTC, 0U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTC, 1U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTC, 2U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTC, 3U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTC, 4U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTC, 5U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTC, 6U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTB, 0U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTB, 1U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTB, 2U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTB, 3U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTB, 4U, kPORT_MuxAlt8);
    PORT_SetPinMux(PORTB, 5U, kPORT_MuxAlt8);
    // PORT_SetPinMux(PORTD, 2U, kPORT_MuxAlt8);

    RADIO_CTRL->DTEST_CTRL = (RADIO_CTRL->DTEST_CTRL & ~RADIO_CTRL_DTEST_CTRL_DTEST_PAGE_MASK) |
                             RADIO_CTRL_DTEST_CTRL_DTEST_EN(1) | RADIO_CTRL_DTEST_CTRL_DTEST_PAGE(0);
#endif // BT51_TEST_MODE_AOA_AOD_DTEST
#endif // BT51_TEST_MODE_AOA_AOD
}
#endif

/*******************************************************************************
 * API
 ******************************************************************************/

void PLATFORM_InitLclRfGpo(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    /* enable RF_GPO[2:0] */
    PORT_SetPinMux(PORTA, 18, kPORT_MuxAlt6);
    PORT_SetPinMux(PORTA, 19, kPORT_MuxAlt6);
    PORT_SetPinMux(PORTA, 20, kPORT_MuxAlt6);

    /* Configure RFMC to route lant_lut_gpio[3:0] to SoC RF_GPO[3:0] */
    RFMC->RF2P4GHZ_COEXT &= ~(RFMC_RF2P4GHZ_COEXT_RFGPO_SRC_MASK | RFMC_RF2P4GHZ_COEXT_RFGPO_OBE_MASK);
    /* 101b - RF_GPO[7:0] = {coext[3:0], lant_lut_gpio[3:0]} */
    RFMC->RF2P4GHZ_COEXT |= RFMC_RF2P4GHZ_COEXT_RFGPO_SRC(5) | RFMC_RF2P4GHZ_COEXT_RFGPO_OBE(0x7);
}

void PLATFORM_InitLcl(void)
{
    PLATFORM_InitLclRfGpo();

#if !defined(NDEBUG)
    /* Enable DTEST when building debug target */
    PLATFORM_InitLclDtest();
#endif
}
