/*!
 * \file fwk_platform_zb.h
 * \brief PLATFORM abstraction API for Zigbee
 *
 * Copyright 2024 NXP
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _FWK_PLATFORM_ZB_H_
#define _FWK_PLATFORM_ZB_H_

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Initialize the platform for Zigbee
 *
 * \return int return status: >=0 for success, <0 for errors
 */
int PLATFORM_InitZigbee(void);

#ifdef __cplusplus
}
#endif

#endif /* _FWK_PLATFORM_ZB_H_ */
