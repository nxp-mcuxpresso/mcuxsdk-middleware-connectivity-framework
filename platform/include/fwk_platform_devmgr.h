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
#include <stdint.h>

#include "fwk_errno.h"

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

/* Number of bits in a byte, used when extracting CRC bytes during UID-derived
 * address composition. */
#define DEVMGR_BITS_PER_BYTE 8U

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

/*
 * Size of a Bluetooth device address (48-bit EUI).
 */
#define DEVMGR_BD_ADDR_SZ 6U

/*
 * Size of an IEEE 802.15.4 device address (64-bit EUI).
 */
#define DEVMGR_154_ADDR_SZ 8U

/* Number of device-specific (least-significant) bytes per address type. */
#define DEVMGR_MAC_GEN_BD_RAND_SZ  ((uint32_t)DEVMGR_BD_ADDR_SZ - (uint32_t)DEVMGR_MAC_GEN_OUI_SZ)
#define DEVMGR_MAC_GEN_154_RAND_SZ ((uint32_t)DEVMGR_154_ADDR_SZ - (uint32_t)DEVMGR_MAC_GEN_OUI_SZ)

#define DEVMGR_UID_SZ 16U

/*
 * MAC address source selection/reporting masks.
 *
 * These bits are used both as an input filter (which sources a caller will
 *      accept) and as an output tag (which source actually provided the
 *      returned address). When several bits are set, the sources are tried in
 *      order of precedence: CUST first, then NMPA, then UUID.
 */

/* Customer-provisioned store (CFPA or HWParameter). Highest precedence. */
#define DEVMGR_MAC_SRC_CUST (1U << 0)
/* Non-volatile Manufacturer Programming Area / IFR factory-programmed store. */
#define DEVMGR_MAC_SRC_NMPA (1U << 1)
/* Address derived from the device unique ID (CRC32 of the UUID). Last resort. */
#define DEVMGR_MAC_SRC_UUID (1U << 2)
/* Accept/allow any of the supported sources (union of all mask bits). */
#define DEVMGR_MAC_SRC_ANY (DEVMGR_MAC_SRC_CUST | DEVMGR_MAC_SRC_NMPA | DEVMGR_MAC_SRC_UUID)

/* Value of an erased flash byte, used to detect blank (unprovisioned) fields. */
#define DEVMGR_BLANK_BYTE 0xFFU

/*! \brief  DevMgr API return status. */
#define DEVMGR_Success_c      NOERROR  /*!< Operation completed successfully */
#define DEVMGR_Error_c        -EBADMSG /*!< Generic error (backend read/write failure) */
#define DEVMGR_Blank_c        -ENOENT  /*!< Requested parameter is unprovisioned (all 0xFF) and could not be resolved */
#define DEVMGR_NotSupported_c -ENOTSUP /*!< Parameter not supported by the active backend */

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

/*! *********************************************************************************
 * \brief  Read the 32 MHz crystal trim value from the platform store.
 *
 * \param[out] pOutTrim  receives the 32 MHz crystal trim value
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform carries no 32 MHz trim,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get32MHzTrim(uint8_t *pOutTrim);

/*! *********************************************************************************
 * \brief  Program the 32 MHz crystal trim value in the platform store.
 *
 * \param[in] trim  32 MHz crystal trim value to store
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      carries no 32 MHz trim, DEVMGR_Error_c on a backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Set32MHzTrim(uint8_t trim);

/*! *********************************************************************************
 * \brief  Read the 32 kHz crystal load-capacitance trim from the platform store.
 *
 * \param[out] pOutCap  receives the 32 kHz crystal load-capacitance value
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform carries no 32 kHz trim,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get32kHzTrim(uint8_t *pOutCap);

/*! *********************************************************************************
 * \brief  Program the 32 kHz crystal load-capacitance trim in the platform store.
 *
 * \param[in] cap  32 kHz crystal load-capacitance value to store
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      carries no 32 kHz trim, DEVMGR_Error_c on a backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Set32kHzTrim(uint8_t cap);

#if defined(__cplusplus)
}
#endif

/*!
 * @} end of DevMgr addtogroup
 */

#endif /* FWK_PLATFORM_DEVMGR_H_ */
