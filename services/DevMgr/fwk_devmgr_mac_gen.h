/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef FWK_DEVMGR_MAC_GEN_H_
#define FWK_DEVMGR_MAC_GEN_H_

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include "fwk_devmgr.h"
#include "fwk_platform_devmgr.h"

/*!
 * @addtogroup DevMgr
 *
 * Optional MAC-address generation service.
 *
 * This service composes a device address when no provisioned value exists. The
 *      3 most-significant bytes are taken from a fixed OUI (BD_ADDR_OUI for BLE,
 *      IEEE802_15_4_ADDR_OUI for 802.15.4; both default to the NXP OUI 00:60:37)
 *      and the remaining bytes are filled by RNG or derived from the CRC32 of
 *      the chip UID.
 *
 * It is built only when
 *      MCUX_COMPONENT_middleware.wireless.framework.devmgr_mac_gen is enabled
 *      (default n) and is invoked exclusively by the HWParameter backend.
 *
 * @{
 */

/**********************************************************************************
 *************************************************************************************
 * Public macros
 *************************************************************************************
 ********************************************************************************** */

/* The generation OUI defaults (BD_ADDR_OUI / IEEE802_15_4_ADDR_OUI) and the
 * MAC-generation size macros (DEVMGR_MAC_GEN_*) are shared with the per-platform
 * layer and are provided by fwk_platform_devmgr.h (included above). */

/**********************************************************************************
 *************************************************************************************
 * Public prototypes
 *************************************************************************************
 ********************************************************************************** */

#if defined(__cplusplus)
extern "C" {
#endif

/*! *********************************************************************************
 * \brief  Generate a Bluetooth device address (OUI + RNG / CRC32(UID)).
 *
 * \details The 3 most-significant bytes are set from BD_ADDR_OUI; the 3
 *      least-significant bytes are filled by RNG or derived from CRC32 of the
 *      chip UID.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on a NULL pointer or
 *      entropy-source failure.
 ********************************************************************************** */
int DEVMGR_GenerateBdAddress(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Generate an IEEE 802.15.4 EUI-64 address (OUI + RNG / CRC32(UID)).
 *
 * \details The 3 most-significant bytes are set from IEEE802_15_4_ADDR_OUI; the
 *      5 least-significant bytes are filled by RNG or derived from CRC32 of the
 *      chip UID.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_154_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on a NULL pointer or
 *      entropy-source failure.
 ********************************************************************************** */
int DEVMGR_Generate154Address(uint8_t *pOutAddr);

#if defined(__cplusplus)
}
#endif

/*!
 * @} end of DevMgr addtogroup
 */

#endif /* FWK_DEVMGR_MAC_GEN_H_ */
