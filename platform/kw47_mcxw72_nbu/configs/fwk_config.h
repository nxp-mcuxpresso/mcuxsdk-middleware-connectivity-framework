/*
 * Copyright 2022-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_CONFIG_H_
#define _FWK_CONFIG_H_

/*
 * KW47 NBU requires RNG but is not allowed to invoke S200 services directly.
 */
#define gRngUseSecureSubSystem_d 0

/*********************************************************************
 *        SecLib
 *********************************************************************/

/*
 * NBU core is without DSP extension -> prevents the use of the NXP UltraFast EC P256 library on this core
 */
#define gSecLibUseDspExtension_d 0

/* If SecLib.c is used, prevent from using LTC HW as this module is located on NBU side and is used exclusively
 by Ble controller and 15.4 MAC/Phy code */
#undef FSL_FEATURE_SOC_LTC_COUNT

/*
 * On KW45/K32W148 NBU avoid usage of SecLib in RNG to prevent calls to SHA256, HMAC or AES-CTR when not required.
 * Call Lehmer LCG random generation instead.
 */
#define gRngUseSecLib_d 0

#endif /* _FWK_CONFIG_H_ */
