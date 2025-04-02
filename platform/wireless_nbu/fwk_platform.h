/*
 * Copyright 2020-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_PLATFORM_H_
#define _FWK_PLATFORM_H_

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "EmbeddedTypes.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define PLATFORM_MAX_INTERRUPT_PRIORITY         3U
#define PLATFORM_MAX_INTERRUPT_PRIORITY_BASEPRI (PLATFORM_MAX_INTERRUPT_PRIORITY << (8 - __NVIC_PRIO_BITS))
#define PLATFORM_SET_INTERRUPT_MASK()                       \
    __get_BASEPRI();                                        \
    __set_BASEPRI(PLATFORM_MAX_INTERRUPT_PRIORITY_BASEPRI); \
    __DSB();                                                \
    __ISB()
#define PLATFORM_CLEAR_INTERRUPT_MASK(x) \
    __set_BASEPRI(x);                    \
    __DSB();                             \
    __ISB()

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/*!
 * \brief  Request main domain to be active
 *
 *  On return from this function, the main domain and all its HW ressources can be accessed safely
 *    until PLATFORM_RemoteActiveRel() is called
 *
 */
void PLATFORM_RemoteActiveReq(void);

/*!
 * \brief  Request main domain to be active
 *
 *  On return from this function, the request to the main domain and all its HW ressources
 *    has been requested
 *
 * \note An additional delay can be required if you want to access to the HW ressources
 *    of the main domain see PLATFORM_RemoteActiveReq() function
 *
 */
void PLATFORM_RemoteActiveReqWithoutDelay(void);

/*!
 * \brief  Release main domain from being active
 *
 *  On return from this function, the main domain and all its HW ressources can not be accessed
 *    if the main domain has turned into lowpower,
 *   Need to call PLATFORM_RemoteActiveReq() for accessing safely to the ressources it contains
 *
 */
void PLATFORM_RemoteActiveRel(void);

/*!
 * \brief  Get Xtal 32MHz trim value
 *
 * \return trimming value of the Xtal 32MHz
 *
 */
uint8_t PLATFORM_GetXtal32MhzTrim(void);

/*!
 * \brief  Set Xtal 32MHz trim value
 *
 * \param[in] trim  trimming value to be set
 *
 */
void PLATFORM_SetXtal32MhzTrim(uint8_t trim);

/*!
 * \brief  Set chip revision
 *
 * \param[in] chip revision, 0 : A0, 1 : A1
 *
 */
void PLATFORM_SetChipRevision(uint8_t chip_rev);

/*!
 * \brief  Set chip revision
 *
 * \return chip revision, 0 : A0, 1 : A1
 *
 */
uint8_t PLATFORM_GetChipRevision(void);

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
 * \brief  wait for the given delay in us
 *
 * \param[in] delayUs time delay in us
 *
 */
void PLATFORM_Delay(uint64_t delayUs);

/*!
 * \brief  wait for the given delay in us starting from
 *  given Timestamp. Timestamp shall be get from PLATFORM_GetTimeStamp()
 *
 * \param[in] timestamp in us
 * \param[in] delayUs time delay in us
 *
 */
void PLATFORM_WaitTimeout(uint64_t timestamp, uint64_t delayUs);

/*!
 * \brief Init and configure FRO192M
 *
 */
void PLATFORM_InitFro192M(void);

/*!
 * \brief Set the nbu frequency constraint for the host and update the core
 *        frequency consequently
 *
 * \param[in] range 0 : 16MHz, 1 : 24MHz, 2 : 32MHz, 3 : 48MHz, 4 : 64MHz
 *
 */
void PLATFORM_SetFrequencyConstraintFromHost(uint8_t freq_constraint);

/*!
 * \brief  Set the nbu frequency constraint for the controller and update the core
 *         frequency consequently
 *
 * \note 64MHz is not supported as you need to ensure that LDO core output voltage is 1.1V if you want to run to this
 *frequency
 *
 * \param[in] range 0 : 16MHz, 1 : 24MHz, 2 : 32MHz, 3 : 48MHz
 *
 */
void PLATFORM_SetFrequencyConstraintFromController(uint8_t freq_constraint);

/*!
 * \brief Returns current frequency constraint set by the host
 *
 * \return uint8_t 0 : 16MHz, 1 : 24MHz, 2 : 32MHz, 3 : 48MHz
 */
uint8_t PLATFORM_GetFrequencyConstraintFromHost(void);

#if (defined(gPlatformUseLptmr_d)) && (gPlatformUseLptmr_d == 1U)

/*!
 * \brief  Initialize Timer Manager
 *
 *    This API will initialize the Timer Manager and the required clocks
 *
 * \return int 0 if success, 1 if already initialized, negative value if error.
 */
int PLATFORM_InitTimerManager(void);

/*!
 * \brief  Deinitialize Timer Manager
 *
 *    This API will deinitialize the Timer Manager
 *
 */
void PLATFORM_DeinitTimerManager(void);

#endif

/*!
 * \brief  get 4 words of information that uniquely identifies the MCU
 *
 * \param[out] aOutUid16B pointer to UID bytes
 * \param[out] pOutLen pointer to UID length
 *
 * \return int 0 if success, negative value if error.
 */
int PLATFORM_GetMCUUid(uint8_t *aOutUid16B, uint8_t *pOutLen);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _FWK_PLATFORM_H_ */
