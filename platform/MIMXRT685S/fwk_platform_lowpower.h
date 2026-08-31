/* -------------------------------------------------------------------------- */
/*                           Copyright 2026 NXP                               */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

#ifndef FWK_PLATFORM_LOWPOWER_H_
#define FWK_PLATFORM_LOWPOWER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @addtogroup FWK_Platform_module
 * @{
 */
/*!
 * @addtogroup FWK_Platform_LowPower
 * The FWK_Platform_LowPower module
 *
 * FWK_Platform_LowPower module provides APIs to handle Low Power.
 * @{
 */
/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdint.h>
#include "fsl_pm_device.h"

/* -------------------------------------------------------------------------- */
/*                                Public macros                               */
/* -------------------------------------------------------------------------- */

#define PLATFORM_WFI_STATE             PM_LP_STATE_SLEEP
#define PLATFORM_SLEEP_STATE           PM_LP_STATE_SLEEP
#define PLATFORM_DEEP_SLEEP_STATE      PM_LP_STATE_DEEP_SLEEP
#define PLATFORM_POWER_DOWN_STATE      PM_LP_STATE_DEEP_POWER_DOWN
#define PLATFORM_DEEP_POWER_DOWN_STATE PM_LP_STATE_FULL_DEEP_POWER_DOWN

/* No constraints in WFI mode, all peripherals are available.*/
#define PLATFORM_WFI_CONSTRAINTS                                                                                \
    18U, PM_RESC_MAIN_CLK_ON, PM_RESC_VDDCOREREG_HP, PM_RESC_PMCREF_HP, PM_RESC_PORCORE_HP, PM_RESC_LVDCORE_HP, \
        PM_RESC_SYSXTAL_ON, PM_RESC_SYSPLLLDO_ON, PM_RESC_SYSPLLANA_ON, PM_RESC_FLEXSPI_SRAM_ACTIVE,            \
        PM_RESC_SRAM12_128KB_ACTIVE, PM_RESC_SRAM13_128KB_ACTIVE, PM_RESC_SRAM14_128KB_ACTIVE,                  \
        PM_RESC_SRAM15_128KB_ACTIVE, PM_RESC_SRAM16_256KB_ACTIVE, PM_RESC_SRAM17_256KB_ACTIVE,                  \
        PM_RESC_SRAM18_256KB_ACTIVE, PM_RESC_SRAM19_256KB_ACTIVE, PM_RESC_SRAM20_256KB_ACTIVE

/* No constraints in SLEEP mode, all peripherals are available.*/
#define PLATFORM_SLEEP_CONSTRAINTS                                                                              \
    18U, PM_RESC_MAIN_CLK_ON, PM_RESC_VDDCOREREG_HP, PM_RESC_PMCREF_HP, PM_RESC_PORCORE_HP, PM_RESC_LVDCORE_HP, \
        PM_RESC_SYSXTAL_ON, PM_RESC_SYSPLLLDO_ON, PM_RESC_SYSPLLANA_ON, PM_RESC_FLEXSPI_SRAM_ACTIVE,            \
        PM_RESC_SRAM12_128KB_ACTIVE, PM_RESC_SRAM13_128KB_ACTIVE, PM_RESC_SRAM14_128KB_ACTIVE,                  \
        PM_RESC_SRAM15_128KB_ACTIVE, PM_RESC_SRAM16_256KB_ACTIVE, PM_RESC_SRAM17_256KB_ACTIVE,                  \
        PM_RESC_SRAM18_256KB_ACTIVE, PM_RESC_SRAM19_256KB_ACTIVE, PM_RESC_SRAM20_256KB_ACTIVE

/* Use selective RAM retention in deep sleep. */
#define PLATFORM_DEEP_SLEEP_CONSTRAINTS                                                                  \
    10U, PM_RESC_FLEXSPI_SRAM_RETENTION, PM_RESC_SRAM12_128KB_RETENTION, PM_RESC_SRAM13_128KB_RETENTION, \
        PM_RESC_SRAM14_128KB_RETENTION, PM_RESC_SRAM15_128KB_RETENTION, PM_RESC_SRAM16_256KB_RETENTION,  \
        PM_RESC_SRAM17_256KB_RETENTION, PM_RESC_SRAM18_256KB_RETENTION, PM_RESC_SRAM19_256KB_RETENTION,  \
        PM_RESC_SRAM20_256KB_RETENTION

/* POWER DOWN mode has no configuration options. */
#define PLATFORM_POWER_DOWN_CONSTRAINTS 0U

/* DEEP POWER DOWN mode has no configuration options. */
#define PLATFORM_DEEP_POWER_DOWN_CONSTRAINTS 0U

/* -------------------------------------------------------------------------- */
/*                              Public type definition                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Initialize platform specific resources for low power support.
 *
 */
void PLATFORM_LowPowerInit(void);

/*!
 * \brief Initialize a wake up timer
 *        Should be called during low power initialization.
 *
 */
void PLATFORM_InitWakeUpTimer(void);

/*!
 * \brief Initialize the BT controller low power support.
 *        When CONFIG_BT_CONTROLLER_SLEEP is defined this calls the
 *        controller_low_power_init() routine supplied by the controller
 *        library; otherwise it is a no-op that returns 0.
 *        Must be called once during system low power initialization,
 *        after the BT controller has been started.
 *
 * \return uint8_t Zero on success, non-zero on error.
 */
uint8_t PLATFORM_ControllerLowPowerInit(void);

/*!
 * \brief Start a wake up timer for next tickless idle period
 *        Should be called during low power entry procedure.
 *
 * \param[in] timeOutUs maximum timeout in microsec
 */
void PLATFORM_StartWakeUpTimer(uint64_t timeOutUs);

/*!
 * \brief Stops previously started wake up timer if the tickless period ended
 *         earlier than expected. Should be called after exiting low power.
 *
 */
void PLATFORM_StopWakeUpTimer(void);

/*!
 * \brief Returns current timestamp in us, usually called before and after tickless
 *        period to compute the number of ticks to update RTOS timebase.
 *
 * \return uint64_t Timestamp value in us
 */
uint64_t PLATFORM_GetLowPowerTimestampUs(void);

/*!
 * \brief Converts timestamp values returned by PLATFORM_GetLowPowerTimestamp to
 *        a duration in us and handles counter wrapping.
 *
 * \param[in] enterLowPowerTimestamp Timestamp measured before entering tickless period
 * \param[in] exitLowPowerTimestamp Timestamp measured after exiting tickless period
 * \return uint64_t Computed duration in us
 */
uint64_t PLATFORM_GetLowPowerDurationUs(uint64_t enterLowPowerTimestamp, uint64_t exitLowPowerTimestamp);

/*!
 * \brief Platform module can implement platform specific methods to execute
 *        when entering and exiting any low power mode. \n
 *        Those methods should implement only mandatory procedures for the
 *        platform, compatible with any connectivity protocol.
 */
void PLATFORM_EnterLowPower(void);

/*!
 * \brief Platform specific procedures to execute when exiting any low power mode
 *
 */
void PLATFORM_ExitLowPower(void);

/*!
 * \brief Platform specific procedures to execute when entering deep sleep mode
 *
 */
void PLATFORM_EnterDeepSleep(void);

/*!
 * \brief Platform specific procedures to execute when exiting deep sleep mode
 *
 */
void PLATFORM_ExitDeepSleep(void);

/*!
 * \brief Power gated low power modes often require specific
 *        entry/exit low power procedures, those should be implemented
 *        in the following API.
 *
 */
void PLATFORM_EnterPowerDown(void);

/*!
 * \brief Power gated low power modes often require specific
 *        entry/exit low power procedures, those should be implemented
 *        in the following API.
 *
 */
void PLATFORM_ExitPowerDown(void);

/*!
 * \brief Specific low power entry procedure when going to Deep Power Down mode (RAMOFF)
 *
 */
void PLATFORM_EnterDeepPowerDown(void);

/*!
 * \brief Specific low power exit procedure when exit Deep Power Down mode (RAMOFF)
 *
 */
void PLATFORM_ExitDeepPowerDown(void);

/*!
 * \brief Check if the BT controller allows the host to enter low power mode.
 *
 * \return uint8_t Non-zero if low power entry is allowed, zero if it must be deferred.
 */
uint8_t PLATFORM_AllowEnterLowPower(void);

/*!
 * @}  end of FWK_Platform_LowPower addtogroup
 */
/*!
 * @}  end of FWK_Platform_module addtogroup
 */

#ifdef __cplusplus
}
#endif

#endif /* FWK_PLATFORM_LOWPOWER_H_ */
