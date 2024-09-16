/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _BOARD_COMP_H_
#define _BOARD_COMP_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "board.h"
#include "fsl_component_button.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* The configuration of Button component. */

#ifndef BOARD_BUTTON_GPIO_PORT
#define BOARD_BUTTON_GPIO_PORT 5
#endif

#ifndef BOARD_BUTTON_GPIO_PIN
#define BOARD_BUTTON_GPIO_PIN BOARD_USER_BUTTON_GPIO_PIN
#endif

#ifndef BOARD_BUTTON_GPIO_PIN_STATE_DEFAULT
#define BOARD_BUTTON_GPIO_PIN_STATE_DEFAULT 1
#endif

#ifndef BOARD_BUTTON_GPIO_DIRECTION
#define BOARD_BUTTON_GPIO_DIRECTION kHAL_GpioDirectionIn
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * API
 ******************************************************************************/

/*!
 * @brief  Initialize Button Component
 *
 *    This API will initialize the button component
 *
 * @param ButtonHandle The button handle.
 */
button_status_t BOARD_InitButton(button_handle_t ButtonHandle);

#ifdef __cplusplus
}
#endif

#endif /* _BOARD_COMP_H_ */
