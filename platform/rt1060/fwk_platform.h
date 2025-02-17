/* -------------------------------------------------------------------------- */
/*                           Copyright 2022-2023 NXP                          */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

#ifndef _FWK_PLATFORM_H_
#define _FWK_PLATFORM_H_

/*!
 * @addtogroup FWK_Platform_module
 * The FWK_Platform module
 *
 * FWK_Platform module provides APIs to set platform parameters.
 * @{
 */
/*!
 * @addtogroup FWK_Platform
 * The FWK_Platform main module
 *
 * FWK_Platform main module provides APIs to set main platform parameters.
 * @{
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*                                 Definitions                                */
/* -------------------------------------------------------------------------- */

/*! @brief The configuration of timer. */

#ifndef PLATFORM_TM_INSTANCE
#define PLATFORM_TM_INSTANCE 0
#endif

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* -------------------------------------------------------------------------- */
/*                        Public functions declaration                        */
/* -------------------------------------------------------------------------- */

/*!
 * \brief  Initialize Timer Manager
 *
 *    This API will initialize the Timer Manager and the required clocks
 *
 */
int PLATFORM_InitTimerManager(void);

/*!
 * \brief  Deinitialize Timer Manager
 *
 *    This API will deinitialize the Timer Manager
 *
 */
void PLATFORM_DeinitTimerManager(void);

/*!
 * \brief Initializes timestamp module
 *
 */
void PLATFORM_InitTimeStamp(void);

/*!
 * \brief Returns current timestamp in us
 *
 * \return uint64_t timestamp in us
 */
uint64_t PLATFORM_GetTimeStamp(void);

/*!
 * \brief Returns the max timestamp value that can be returned by PLATFORM_GetTimeStamp
 *        Can be used by the user to handle timestamp wrapping
 *
 * \return uint64_t the max timestamp value
 */
uint64_t PLATFORM_GetMaxTimeStamp(void);

/*!
 * \brief Configures IO_Expander to enable SPI interface through M.2 connector
 *
 * \param[in] addr I2C 7-bits address
 * \param[in] reg register to set
 * \param[in] val register value to set
 * \return kStatus_Success if IO_Expander configuration succeed otherwise status of failing operation.
 */
int PLATFORM_IOEXP_I2C_program(uint8_t addr, uint8_t reg, uint8_t val);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

/*!
 * @}  end of FWK_Platform addtogroup
 */
/*!
 * @}  end of FWK_Platform_module addtogroup
 */
#endif /* _FWK_PLATFORM_H_ */
