/*!
 * Copyright 2021 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PLATFORM_LCL_H_
#define _PLATFORM_LCL_H_

#include "EmbeddedTypes.h"

/*******************************************************************************
 * API
 ******************************************************************************/
/*!
 * \brief Setup PIN mux and RF GPO configuration for antenna switching
 */
void PLATFORM_InitLclRfGpo(void);

/*!
 * \brief Setup debug GPIOs and antenna switching IOs for BLE localization apps
 */
void PLATFORM_InitLcl(void);

/*! *********************************************************************************
 * \brief  Configure COEX control
 * \param[in]  p_config     pointer to COEX configuration
 * \param[in]  config_len   length of configuration, for checking
 *
 * \return 0/1 which is Success/Failure correspondingly
 ********************************************************************************** */
uint8_t PLATFORM_InitCOEX(const uint8_t *p_config, uint8_t config_len);

/*! *********************************************************************************
 * \brief  Configure FEM control
 * \param[in]  config_ptr   pointer to FEM configuration
 *
 * \return 0/1 which is Success/Failure correspondingly
 ********************************************************************************** */
uint8_t PLATFORM_InitFEM(const uint8_t *p_config, uint8_t config_len);

#endif /* _PLATFORM_LCL_H_ */
