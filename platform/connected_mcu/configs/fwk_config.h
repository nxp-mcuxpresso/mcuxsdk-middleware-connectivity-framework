/*
 * Copyright 2022-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_CONFIG_H_
#define _FWK_CONFIG_H_

#include "fwk_platform_definitions.h"

/*********************************************************************
 *        General
 *********************************************************************/

/* Defines the calibration duration of the ADC, it will block the task during this time in milisec before trigger the
 * ADC on a channel*/
#define gSensorsAdcCalibrationDurationInMs_c 4U

/*
 * KW45/K32W148 CM33 core has the DSP extension (__DSP_PRESENT), which allows the use of the NXP UltraFast EC P256
 * library The flags may be set to 0 if this library is not required. It mainly makes sense for Matter SPAKE2P
 * procedures and for BLE point validation.
 */
#define gSecLibUseDspExtension_d 1

/*********************************************************************
 *        HW parameters enablement and placement
 *********************************************************************/

#if !defined(gPlatformUseHwParameter_d)
#define gPlatformUseHwParameter_d 1
#endif
/*
 * gHwParamsProdDataPlacementLegacy_c HWParameters PROD_DATA remain at top of main flash,
 * where the linker script defines PROD_DATA_BASE_ADDR.
 * When transiting from LegacyMode to IfrMode, an interim mode Legacy2IfrMode is used to
 * enable the transfer of HWParameters from the main flash to the IFR.
 * After this has been applied the size of the PROD_DATA reserved space in the linker script
 * will be made 0 in order to gain a sector.
 * These modes are used to configure gHwParamsProdDataPlacement_c
 */
#define gHwParamsProdDataPlacementLegacyMode_c     0
#define gHwParamsProdDataPlacementLegacy2IfrMode_c 1
#define gHwParamsProdDataPlacementIfrMode_c        2

/*
 * Place Application FactoryData in same sector as ProdData.
 * HWParameters and App Factory Data being colocated has an implication on the life cycle of information stored in the
 * sector.
 */
#ifndef gHwParamsAppFactoryDataExtension_d
#define gHwParamsAppFactoryDataExtension_d 0
#endif

/*
 * To be defined as 1 if NV_WriteHWParameters is liable to be invoked after Application Factory Data were written,
 * and you intend not to have to preprogram them. Default to 0, assuming that this can happne in the context of
 * a factory reset. Not needed if gHwParamsAppFactoryDataExtension_d is not set.
 */
#if (gHwParamsAppFactoryDataExtension_d != 0)
#ifndef gHwParamsAppFactoryDataPreserveOnHwParamUpdate_d
#define gHwParamsAppFactoryDataPreserveOnHwParamUpdate_d 0
#endif
#else
#define gHwParamsAppFactoryDataPreserveOnHwParamUpdate_d 0
#endif
/*
 * Define gHwParamsProdDataPlacement_c as :
 *   - gHwParamsProdDataPlacementLegacy_c if you mean to remain backward compatible.
 *   - gHwParamsProdDataPlacementLegacy2IfrMode_c if you wish to conserve previous
 *     HWParameter setting (MAC addresses, xtal trimming data) during migration phase.
 *   - gHwParamsProdDataPlacementIfrMode_c for new devices or once gHwParamsProdDataPlacementLegacy2IfrMode_c
 *     mode has populated the IFR with legacy values. -> after this phase update linker script to remove
 *     flash space reserved for PROD_DATA.
 */
#ifndef gHwParamsProdDataPlacement_c
#define gHwParamsProdDataPlacement_c gHwParamsProdDataPlacementLegacyMode_c
//#define gHwParamsProdDataPlacement_c gHwParamsProdDataPlacementLegacy2IfrMode_c
//#define gHwParamsProdDataPlacement_c gHwParamsProdDataPlacementIfrMode_c
#endif

#if (gHwParamsProdDataPlacement_c == gHwParamsProdDataPlacementLegacyMode_c)
#define PROD_DATA_FLASH_ADDR (LEGACY_PROD_DATA_ADDR + PROD_DATA_OFFSET)
#define USER_DATA_SECTOR     PROD_DATA_FLASH_ADDR
#else
#define PROD_DATA_FLASH_ADDR (IFR_USER_ADDR + PROD_DATA_OFFSET)
#define USER_DATA_SECTOR     PROD_DATA_FLASH_ADDR
#endif

#ifdef gHwParamsAppFactoryDataExtension_d
#define APP_FACTORY_DATA_FLASH_ADDR (PROD_DATA_FLASH_ADDR + APP_FACTORY_DATA_OFFSET)
#endif

#define APP_FACTORY_DATA_MAX_LEN 0x800U

/*********************************************************************
 *        Reset Method
 * Define the alternative method from warm reset to use for Device reset
 *   when calling PLATFORM_ResetCpu()
 *********************************************************************/
#define gUseResetByNvicReset_c     1
#define gUseResetByLvdForce_c      2
#define gUseResetByDeepPowerDown_c 3

#if !defined(gPlatResetMethod_c)
#if defined(FPGA_SUPPORT) && (FPGA_SUPPORT == 1)
#define gPlatResetMethod_c gUseResetByNvicReset_c
#else
#define gPlatResetMethod_c gUseResetByDeepPowerDown_c
#endif
#endif

/*********************************************************************
 *   Flash check for ECC error after IFR OT_CFG programming.
 *
 *  Defaults to 1 to avoid bus fault in case of error
 *  Temporarily leave as 0 to before full OTA testing is done.
 *********************************************************************/
#ifndef gOtaCheckEccFaults_d
#define gOtaCheckEccFaults_d 1
#endif

/*********************************************************************
 *   NVS Sector size is necessarily a multiple of physical flash sector size.
 *
 *  Defaults to 1 because 8kB is already pretty large. cannot exceed 8 sectors.
 *
 *********************************************************************/
#ifndef gPlatNvsSectorSize_c
#define gPlatNvsSectorSize_c (PLATFORM_INTFLASH_SECTOR_SIZE * 1u)
#endif

/*********************************************************************
 *        RNG source of entropy
 *********************************************************************/

/* On connected_mcu platform the only source of entropy disponible is the S200 */
#ifndef gRngUseSecureSubSystem_d
#define gRngUseSecureSubSystem_d 1
#endif

#if defined(gRngUseSecureSubSystem_d) && (gRngUseSecureSubSystem_d == 0)
#warning \
    "On connected_mcu the only source of entropy is the S200, RNG will not be properly initialized if this flag is not enabled"
#endif

#endif /* _FWK_CONFIG_H_ */
