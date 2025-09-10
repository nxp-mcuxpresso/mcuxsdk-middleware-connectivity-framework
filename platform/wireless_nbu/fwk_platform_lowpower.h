/* -------------------------------------------------------------------------- */
/*                           Copyright 2021-2022 NXP                          */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

#ifndef _FWK_PLATFORM_LOWPOWER_H_
#define _FWK_PLATFORM_LOWPOWER_H_

/* -------------------------------------------------------------------------- */
/*                              Public protoypes                              */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Initializes low power ressources required before requesting low power
 *        entry
 */
void PLATFORM_LowPowerInit(void);

/*!
 * \brief Tries to go to low power, if that's not possible or if
 *        PLATFORM_LowPowerInit hasn't been called, it will go to WFI only.
 */
void PLATFORM_EnterLowPower(void);

/*!
 * \brief Set value of wake up delay received from the host.
 *
 * \param[in] wakeupDelayReceivedFromHost Value sent by the host
 *
 */
void PLATFORM_SetWakeupDelay(uint8_t wakeupDelayReceivedFromHost);

/*!
 * \brief Set instant of next planned activity in RAM for MCU to check converting from LL number of half-slots.
 *
 * \param duration_32kHz_tick number of 32kHz ticks until next NBU activity
 *
 */
void PLATFORM_SetNextNbuActivityDeadline(uint32_t duration_32kHz_tick);

#endif /* _FWK_PLATFORM_LOWPOWER_H_ */
