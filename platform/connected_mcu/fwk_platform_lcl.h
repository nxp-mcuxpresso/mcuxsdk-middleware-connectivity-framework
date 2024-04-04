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

#endif /* _PLATFORM_LCL_H_ */
