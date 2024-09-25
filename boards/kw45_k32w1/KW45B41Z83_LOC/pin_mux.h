/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/***********************************************************************************************************************
 * This file was generated by the MCUXpresso Config Tools. Any manual edits made to this file
 * will be overwritten if the respective MCUXpresso Config Tools is used to update this file.
 **********************************************************************************************************************/

#ifndef _PIN_MUX_H_
#define _PIN_MUX_H_

/*!
 * @addtogroup pin_mux
 * @{
 */

/***********************************************************************************************************************
 * API
 **********************************************************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif


/*! @name PORTC1 (number 38), SW2
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITPINBUTTON0_SW2_GPIO GPIOC               /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITPINBUTTON0_SW2_GPIO_PIN_MASK (1U << 1U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITPINBUTTON0_SW2_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_INITPINBUTTON0_SW2_PIN 1U                   /*!<@brief PORT pin number */
#define BOARD_INITPINBUTTON0_SW2_PIN_MASK (1U << 1U)      /*!<@brief PORT pin mask */
                                                          /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinButton0(void);

/*! @name PORTC7 (number 45), SW3
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITPINBUTTON1_SW3_GPIO GPIOC               /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITPINBUTTON1_SW3_GPIO_PIN_MASK (1U << 7U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITPINBUTTON1_SW3_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_INITPINBUTTON1_SW3_PIN 7U                   /*!<@brief PORT pin number */
#define BOARD_INITPINBUTTON1_SW3_PIN_MASK (1U << 7U)      /*!<@brief PORT pin mask */
                                                          /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinButton1(void);

/*! @name PORTA4 (number 10), SWO
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITPINSWO_GPIO GPIOA                        /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITPINSWO_GPIO_PIN_MASK (1U << 4U)          /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITPINSWO_PORT PORTA                       /*!<@brief PORT peripheral base pointer */
#define BOARD_INITPINSWO_PIN 4U                           /*!<@brief PORT pin number */
#define BOARD_INITPINSWO_PIN_MASK (1U << 4U)              /*!<@brief PORT pin mask */
                                                          /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinSWO(void);

/*! @name PORTC0 (number 37), LED1_R
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITPINLED1_LED1_GPIO GPIOC                 /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITPINLED1_LED1_GPIO_PIN_MASK (1U << 0U)   /*!<@brief GPIO pin mask */
#define BOARD_MONOCHROME_GPIO_PIN_DEFAULT_STATE 0U        /*!<@brief GPIO default state */

/* Symbols to be used with PORT driver */
#define BOARD_INITPINLED1_LED1_PORT PORTC                 /*!<@brief PORT peripheral base pointer */
#define BOARD_INITPINLED1_LED1_PIN 0U                     /*!<@brief PORT pin number */
#define BOARD_INITPINLED1_LED1_PIN_MASK (1U << 0U)        /*!<@brief PORT pin mask */
                                                          /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLED1(void);

/*! @name PORTC6 (number 44), LED2_Y
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITPINLED2_LED2_Y_GPIO GPIOC               /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITPINLED2_LED2_Y_GPIO_PIN_MASK (1U << 6U) /*!<@brief GPIO pin mask */
#define BOARD_RGB_RED_GPIO_PIN_DEFAULT_STATE    0U        /*!<@brief GPIO default state */

/* Symbols to be used with PORT driver */
#define BOARD_INITPINLED2_LED2_Y_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_INITPINLED2_LED2_Y_PIN 6U                   /*!<@brief PORT pin number */
#define BOARD_INITPINLED2_LED2_Y_PIN_MASK (1U << 6U)      /*!<@brief PORT pin mask */
                                                          /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLED2(void);

#define BOARD_RGB_GREEN_GPIO_PIN_DEFAULT_STATE  0U        /*!<@brief GPIO default state */
/*!
 * @brief Not supported on this board. Keep for compatibility.
 *
 */
void BOARD_InitPinLED3(void);

#define BOARD_RGB_BLUE_GPIO_PIN_DEFAULT_STATE   0U        /*!<@brief GPIO default state */
/*!
 * @brief Not supported on this board. Keep for compatibility.
 *
 */
void BOARD_InitPinLED4(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLPUART0_RX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLPUART0_TX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLPUART1_RX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLPUART1_TX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLPUART0_RX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLPUART0_TX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLPUART1_RX(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLPUART1_TX(void);

/*! @name PORTC7 (number 45), SW3
  @{ */

/* Symbols to be used with PORT driver */
#define BOARD_UNINITPINBUTTON1_SW3_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_UNINITPINBUTTON1_SW3_PIN 7U                   /*!<@brief PORT pin number */
#define BOARD_UNINITPINBUTTON1_SW3_PIN_MASK (1U << 7U)      /*!<@brief PORT pin mask */
                                                            /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinButton1(void);

/*! @name PORTC1 (number 38), SW2
  @{ */

/* Symbols to be used with PORT driver */
#define BOARD_UNINITPINBUTTON0_SW2_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_UNINITPINBUTTON0_SW2_PIN 1U                   /*!<@brief PORT pin number */
#define BOARD_UNINITPINBUTTON0_SW2_PIN_MASK (1U << 1U)      /*!<@brief PORT pin mask */
                                                            /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinButton0(void);

/*! @name PORTA4 (number 10), SWO
  @{ */

/* Symbols to be used with PORT driver */
#define BOARD_UNINITPINSWO_PORT PORTA                       /*!<@brief PORT peripheral base pointer */
#define BOARD_UNINITPINSWO_PIN 4U                           /*!<@brief PORT pin number */
#define BOARD_UNINITPINSWO_PIN_MASK (1U << 4U)              /*!<@brief PORT pin mask */
                                                          /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinSWO(void);


/*! @name PORTC0 (number 37), LED1_R
  @{ */

/* Symbols to be used with PORT driver */
#define BOARD_UNINITPINLED1_LED1_R_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_UNINITPINLED1_LED1_R_PIN 0U                   /*!<@brief PORT pin number */
#define BOARD_UNINITPINLED1_LED1_R_PIN_MASK (1U << 0U)      /*!<@brief PORT pin mask */
                                                            /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLED1(void);

/*! @name PORTC6 (number 44), LED2_Y
  @{ */

/* Symbols to be used with PORT driver */
#define BOARD_UNINITPINLED2_LED2_Y_PORT PORTC               /*!<@brief PORT peripheral base pointer */
#define BOARD_UNINITPINLED2_LED2_Y_PIN 6U                   /*!<@brief PORT pin number */
#define BOARD_UNINITPINLED2_LED2_Y_PIN_MASK (1U << 6U)      /*!<@brief PORT pin mask */
                                                            /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLED2(void);

/*!
 * @brief Not supported on this board. Keep for compatibility.
 *
 */
void BOARD_UnInitPinLED3(void);

/*!
 * @brief Not supported on this board. Keep for compatibility.
 *
 */
void BOARD_UnInitPinLED4(void);

/*! @name PORTB0 (number 46), LED1/LPSPI1_PCS0
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS0_GPIO GPIOB               /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS0_GPIO_PIN_MASK (1U << 0U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS0_PORT PORTB               /*!<@brief PORT peripheral base pointer */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS0_PIN 0U                   /*!<@brief PORT pin number */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS0_PIN_MASK (1U << 0U)      /*!<@brief PORT pin mask */
                                                                    /* @} */

/*! @name PORTB4 (number 2), LPSPI1_PCS3
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS3_GPIO GPIOB               /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS3_GPIO_PIN_MASK (1U << 4U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS3_PORT PORTB               /*!<@brief PORT peripheral base pointer */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS3_PIN 4U                   /*!<@brief PORT pin number */
#define BOARD_INITEXTFLASHPINS_LPSPI1_PCS3_PIN_MASK (1U << 4U)      /*!<@brief PORT pin mask */
                                                                    /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitExtFlashPins(void);

/*! @name PORTB0 (number 46), LPSPI1_PCS0
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS0_GPIO GPIOB               /*!<@brief GPIO peripheral base pointer */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS0_GPIO_PIN_MASK (1U << 0U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS0_PORT PORTB               /*!<@brief PORT peripheral base pointer */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS0_PIN 0U                   /*!<@brief PORT pin number */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS0_PIN_MASK (1U << 0U)      /*!<@brief PORT pin mask */
                                                                      /* @} */

/*! @name PORTB4 (number 2), LPSPI1_PCS3
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS3_GPIO GPIOB               /*!<@brief GPIO peripheral base pointer */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS3_GPIO_PIN_MASK (1U << 4U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS3_PORT PORTB               /*!<@brief PORT peripheral base pointer */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS3_PIN 4U                   /*!<@brief PORT pin number */
#define BOARD_DEINITEXTFLASHPINS_LPSPI1_PCS3_PIN_MASK (1U << 4U)      /*!<@brief PORT pin mask */
                                                                      /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_DeinitExtFlashPins(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLPUART0_RTS(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPinLPUART0_CTS(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLPUART0_RTS(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_UnInitPinLPUART0_CTS(void);

/*! @name PORTA18 (number 13), BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_0_GPIO
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_0_GPIO GPIOA                /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_0_GPIO_PIN_MASK (1U << 18U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_0_PORT PORTA                /*!<@brief PORT peripheral base pointer */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_0_PIN 18U                   /*!<@brief PORT pin number */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_0_PIN_MASK (1U << 18U)      /*!<@brief PORT pin mask */
                                                                         /* @} */

/*! @name PORTA19 (number 14), BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_1_GPIO
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_1_GPIO GPIOA                /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_1_GPIO_PIN_MASK (1U << 19U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_1_PORT PORTA                /*!<@brief PORT peripheral base pointer */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_1_PIN 19U                   /*!<@brief PORT pin number */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_1_PIN_MASK (1U << 19U)      /*!<@brief PORT pin mask */
                                                                         /* @} */

/*! @name PORTA20 (number 17), BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_2_GPIO
  @{ */

/* Symbols to be used with GPIO driver */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_2_GPIO GPIOA                /*!<@brief GPIO peripheral base pointer */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_2_GPIO_PIN_MASK (1U << 20U) /*!<@brief GPIO pin mask */

/* Symbols to be used with PORT driver */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_2_PORT PORTA                /*!<@brief PORT peripheral base pointer */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_2_PIN 20U                   /*!<@brief PORT pin number */
#define BOARD_INITRFSWITCHCONTROLPINS_RF_GPO_2_PIN_MASK (1U << 20U)      /*!<@brief PORT pin mask */
                                                                         /* @} */

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitRFSwitchControlPins(void);

#if defined(__cplusplus)
}
#endif

/*!
 * @}
 */
#endif /* _PIN_MUX_H_ */

/***********************************************************************************************************************
 * EOF
 **********************************************************************************************************************/
