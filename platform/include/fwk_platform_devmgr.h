/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef FWK_PLATFORM_DEVMGR_H_
#define FWK_PLATFORM_DEVMGR_H_

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include "fwk_platform_devmgr_types.h"

/*!
 * @addtogroup DevMgr
 *
 * Platform contract for the DevMgr module.
 *
 * This header declares the per-platform PLATFORM_DEVMGR_* accessors that the
 *      DevMgr backends (fwk_devmgr_hwparam.c and fwk_devmgr_platf.c) delegate
 *      to, together with the OUI defaults and MAC-generation size macros that
 *      are shared by every platform's fwk_platform_devmgr.c implementation and
 *      by the optional MAC-generation service (fwk_devmgr_mac_gen.c).
 *
 * @{
 */

/**********************************************************************************
 *************************************************************************************
 * Public macros
 *************************************************************************************
 ********************************************************************************** */

/*
 * Fixed OUI (3 most-significant address bytes) used for each generated address
 * type. Default to the NXP OUI 00:60:37 when the application/board does not
 * define its own.
 */
#ifndef BD_ADDR_OUI
#define BD_ADDR_OUI 0x37U, 0x60U, 0x00U
#endif

#ifndef IEEE802_15_4_ADDR_OUI
#define IEEE802_15_4_ADDR_OUI 0x37U, 0x60U, 0x00U
#endif

/* Number of OUI (most-significant) bytes shared by both address types. */
#define DEVMGR_MAC_GEN_OUI_SZ 3U

/* Number of bits in a byte, used when extracting CRC bytes during UID-derived
 * address composition. */
#define DEVMGR_BITS_PER_BYTE 8U

/* Number of device-specific (least-significant) bytes per address type. */
#define DEVMGR_MAC_GEN_BD_RAND_SZ  ((uint32_t)DEVMGR_BD_ADDR_SZ - (uint32_t)DEVMGR_MAC_GEN_OUI_SZ)
#define DEVMGR_MAC_GEN_154_RAND_SZ ((uint32_t)DEVMGR_154_ADDR_SZ - (uint32_t)DEVMGR_MAC_GEN_OUI_SZ)

#define DEVMGR_UID_SZ 16U

/**********************************************************************************
 *************************************************************************************
 * Public prototypes
 *************************************************************************************
 ********************************************************************************** */

#if defined(__cplusplus)
extern "C" {
#endif

/*! *********************************************************************************
 * \brief  Read the Bluetooth device address from the customer (CFPA/HWParameter)
 *      store.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform has no customer store,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_GetBdAddressCust(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Read the Bluetooth device address from the NMPA / IFR region.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform carries no NMPA BD address,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_GetBdAddrFromNMPA(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Derive a Bluetooth device address from the chip UID (deterministic).
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the UID is blank,
 *      DEVMGR_NotSupported_c if UID derivation is unavailable,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_GetBdAddrFromUID(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Program the Bluetooth device address in the platform store.
 *
 * \param[in] pAddr  buffer of DEVMGR_BD_ADDR_SZ bytes containing the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      store is read-only, DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_SetBdAddress(const uint8_t *pAddr);

/*! *********************************************************************************
 * \brief  Read the IEEE 802.15.4 device address from the platform store.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_154_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform has no such store,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get154AddressCust(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Read the IEEE 802.15.4 device address from the NMPA / IFR region.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_154_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform carries no NMPA 802.15.4 address,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get154AddrFromNMPA(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Derive an IEEE 802.15.4 device address from the chip UID (deterministic).
 *
 * \param[out] pOutAddr  buffer of DEVMGR_154_ADDR_SZ bytes receiving the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the UID is blank,
 *      DEVMGR_NotSupported_c if UID derivation is unavailable,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get154AddrFromUID(uint8_t *pOutAddr);

/*! *********************************************************************************
 * \brief  Program the IEEE 802.15.4 device address in the platform store.
 *
 * \param[in] pAddr  buffer of DEVMGR_154_ADDR_SZ bytes containing the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      store is read-only, DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Set154Address(const uint8_t *pAddr);

#if defined(__cplusplus)
}
#endif

/*!
 * @} end of DevMgr addtogroup
 */

#endif /* FWK_PLATFORM_DEVMGR_H_ */
