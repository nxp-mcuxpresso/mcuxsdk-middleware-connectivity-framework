/* -------------------------------------------------------------------------- */
/*                             Copyright 2022 NXP                             */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

#ifndef _FWK_PLATFORM_MWS_H_
#define _FWK_PLATFORM_MWS_H_

/*!
 * @addtogroup FWK_Platform_module
 * @{
 */
/*!
 * @addtogroup FWK_Platform_MWS
 * The FWK_Platform_MWS module
 *
 * FWK_Platform_MWS module provides APIs to set MWS platform parameters.
 * @{
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "EmbeddedTypes.h"
#include "fsl_adapter_rpmsg.h"

/* -------------------------------------------------------------------------- */
/*                              Public prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Init Rpmsg for Coexistence channel
 *
 * \return rpmsg handle pointer
 *
 */

void PLATFORM_InitMwsRpmsg(void);

/*!
 * \brief Returns RPMSG handle for MWS module
 *
 * \return hal_rpmsg_handle_t
 */
hal_rpmsg_handle_t PLATFORM_GetMwsRpmsgHandle(void);

/*!
 * @}  end of FWK_Platform_MWS addtogroup
 */
/*!
 * @}  end of FWK_Platform_module addtogroup
 */

#endif /* _FWK_PLATFORM_MWS_H_ */
