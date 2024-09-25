/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/***********************************************************************************************************************
 * This file was generated by the MCUXpresso Config Tools. Any manual edits made to this file
 * will be overwritten if the respective MCUXpresso Config Tools is used to update this file.
 **********************************************************************************************************************/

#ifndef _CLOCK_CONFIG_H_
#define _CLOCK_CONFIG_H_

#include "fsl_common.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BOARD_XTAL0_CLK_HZ                         32000000U  /*!< Board xtal0 frequency in Hz */

/*******************************************************************************
 ************************ BOARD_InitBootClocks function ************************
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/*!
 * @brief This function executes default configuration of clocks.
 *
 */
void BOARD_InitBootClocks(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

/*******************************************************************************
 ********************** Configuration BOARD_BootClockRUN ***********************
 ******************************************************************************/
/*******************************************************************************
 * Definitions for BOARD_BootClockRUN configuration
 ******************************************************************************/
#define BOARD_BOOTCLOCKRUN_CORE_CLOCK              48000000U  /*!< Core clock frequency: 48000000Hz */
#define BOARD_BOOTCLOCKRUN_ROSC_CLOCK                 32768U  /*!< ROSC clock frequency: 32768Hz */

/*! @brief SCG set for BOARD_BootClockRUN configuration.
 */
extern const scg_sys_clk_config_t g_sysClkConfig_BOARD_BootClockRUN;
/*! @brief System OSC set for BOARD_BootClockRUN configuration.
 */
extern const scg_sosc_config_t g_scgSysOscConfig_BOARD_BootClockRUN;
/*! @brief SIRC set for BOARD_BootClockRUN configuration.
 */
extern const scg_sirc_config_t g_scgSircConfig_BOARD_BootClockRUN;
/*! @brief FIRC set for BOARD_BootClockRUN configuration.
 */
extern const scg_firc_config_t g_scgFircConfig_BOARD_BootClockRUN;

/*******************************************************************************
 * API for BOARD_BootClockRUN configuration
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/*!
 * @brief This function executes configuration of clocks.
 *
 */
void BOARD_BootClockRUN(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

/*******************************************************************************
 ********************* Configuration BOARD_BootClockHSRUN **********************
 ******************************************************************************/
/*******************************************************************************
 * Definitions for BOARD_BootClockHSRUN configuration
 ******************************************************************************/
#define BOARD_BOOTCLOCKHSRUN_CORE_CLOCK            96000000U  /*!< Core clock frequency: 96000000Hz */
#define BOARD_BOOTCLOCKHSRUN_ROSC_CLOCK               32768U  /*!< ROSC clock frequency: 32768Hz */

/*! @brief SCG set for BOARD_BootClockHSRUN configuration.
 */
extern const scg_sys_clk_config_t g_sysClkConfig_BOARD_BootClockHSRUN;
/*! @brief System OSC set for BOARD_BootClockHSRUN configuration.
 */
extern const scg_sosc_config_t g_scgSysOscConfig_BOARD_BootClockHSRUN;
/*! @brief SIRC set for BOARD_BootClockHSRUN configuration.
 */
extern const scg_sirc_config_t g_scgSircConfig_BOARD_BootClockHSRUN;
/*! @brief FIRC set for BOARD_BootClockHSRUN configuration.
 */
extern const scg_firc_config_t g_scgFircConfig_BOARD_BootClockHSRUN;

/*******************************************************************************
 * API for BOARD_BootClockHSRUN configuration
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/*!
 * @brief This function executes configuration of clocks.
 *
 */
void BOARD_BootClockHSRUN(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif /* _CLOCK_CONFIG_H_ */

