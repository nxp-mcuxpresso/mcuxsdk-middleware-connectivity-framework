/*
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_CONFIG_H_
#define _FWK_CONFIG_H_

/*********************************************************************
 *        SecLib
 *********************************************************************/

/*
 * No Secure subsystem on KW43
 */
#define gRngUseSecureSubSystem_d 0

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

/*********************************************************************
 *        FPGA
 *********************************************************************/
#define gUseSfcRf_d 0

/*********************************************************************
 *        PLATFORM
 *********************************************************************/
/* Enable ICS RX processing in the system workqueue thread
 * If enabled, the ICS will use the system workqueue to process its received data. This allows to reduce the time spent
 * in the ISR and reduces impact on system activities.
 * Note: the system workqueue requires a dedicated thread, so enabling this feature will consume a bit of RAM for the
 * thread stack (this can be configured).
 */
#ifndef gPlatformIcsUseWorkqueueRxProcessing_d
#define gPlatformIcsUseWorkqueueRxProcessing_d 1
#endif

/* Enable coredump debug feature */
#define gFaultHandlerCoredumpEnabled_d 1

/* Defer the ECDH P256 public key generation and DH key computation from the NBU (controller) to the host (MCU) core.
 * When enabled, the NBU forwards the P256 public key / DH key requests to the host and the related ICS messages and
 * handlers are compiled in. When disabled, the related APIs remain available but are empty so callers can keep the
 * calls unconditionally. */
#ifndef gPlatformIcsDeferDHKeyToHost_d
#define gPlatformIcsDeferDHKeyToHost_d 1
#endif

/* Enable the NCO32K usage
 * When active, the NBU will use the NCO32K as clock source instead of OSC32K/FRO32K
 * The NCO32K is derived from the 32MHz crystal
 * Before low power entry, the NCO32K must be disabled to switch back to OSC32K/FRO32K */
#ifndef gPlatformUseNco32k_d
#define gPlatformUseNco32k_d 1
#endif

/* Enable a dummy interrupt on NBU core that will be triggered by PLATFORM_RemoteActiveReq() on host core each time it
 * needs to access to NBU power domain */
#define gPlatformLowpowerEnableWakeUpInterrupt_d 1

#endif /* _FWK_CONFIG_H_ */
