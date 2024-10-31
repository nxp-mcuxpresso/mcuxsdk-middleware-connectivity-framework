/* -------------------------------------------------------------------------- */
/*                             Copyright 2023 NXP                             */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

#ifndef _FWK_PLATFORM_RESET_H_
#define _FWK_PLATFORM_RESET_H_

/*!
 * @addtogroup FWK_Platform_module
 * @{
 */
/*!
 * @addtogroup FWK_Platform_Reset
 * The FWK_Platform_Reset module
 *
 * FWK_Platform_Reset module provides APIs to handle Reset via Deep Power Down transition.
 * @{
 */
/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                Public Macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Public APIs                                    */
/* -------------------------------------------------------------------------- */

/*!
 * \brief  Trigger a deep power down reset.
 *
 * \note If debugger is attached, the lowpower entry will be denied and the
 *       reset will fail.
 */
void PLATFORM_ForceDeepPowerDownReset(void);

/*!
 * \brief  Trigger a deep power down reset if last reset was a pin reset.
 *
 */
void PLATFORM_CheckAndForceDeepPowerDownResetOnResetPin(void);

/*!
 * \brief  Shutdown radio then trigger an LVD reset
 *
 */
void PLATFORM_ForceLvdReset(void);

/*!
 * \brief  Trigger an LVD reset if last reset was a pin reset.(see PLATFORM_ForceLvdReset)
 *
 */
void PLATFORM_ForceLvdResetFromResetPin(void);

/*!
 * \brief  Trigger a plain system reset.
 *
 */
void PLATFORM_NvicSystemReset(void);

/*!
 * \brief  Implement a custom reset based on gPlatResetMethod_c definition
 *       gUseResetByNvicReset_c (default)
 *       gUseResetByLvdForce_c force Low Voltage Detection reset
 *       gUseResetByDeepPowerDown_c force a Deep Power Down reset
 *
 */
void PLATFORM_ResetCpu(void);

/*!
 * @}  end of FWK_Platform_Reset addtogroup
 */
/*!
 * @}  end of FWK_Platform_module addtogroup
 */

#endif /* _FWK_PLATFORM_LOWPOWER_H_ */