/* -------------------------------------------------------------------------- */
/*                           Copyright 2026 NXP                               */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

#ifndef FWK_PLATFORM_H_
#define FWK_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*                                Public macros                               */
/* -------------------------------------------------------------------------- */

#define PLATFORM_OSTIMER_CLK kOSC32K_to_OSTIMER_CLK

/* OSTimer clock source index for the 32 kHz low-power oscillator (OSC32K).
 * This value is passed to hal_time_stamp_config_t::clockSrcSelect. */
#define PLATFORM_OSTIMER_CLK_SRC_32K_OSC 1U

/* Clock source used for the OSTimer timestamp peripheral. */
#define PLATFORM_OSTIMER_CLK_SRC_SELECT PLATFORM_OSTIMER_CLK_SRC_32K_OSC

/* Max OSTIMER counter value (44 bits) converted to µs at 32 768 Hz (OSC32K). */
#define PLATFORM_MAX_TIMESTAMP_US COUNT_TO_USEC(0xFFFFFFFFFFFULL, 32768U)

/* -------------------------------------------------------------------------- */
/*                              Public prototypes                             */
/* -------------------------------------------------------------------------- */

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
 * \brief Save timestamp module before entering power down
 *
 */
void PLATFORM_TimeStampEnterPowerDown(void);

/*!
 * \brief Restore timestamp module after exit from power down
 *
 */
void PLATFORM_TimeStampExitPowerDown(void);

#ifdef __cplusplus
}
#endif

#endif /* FWK_PLATFORM_H_ */
