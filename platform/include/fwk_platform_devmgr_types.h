/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef FWK_PLATFORM_DEVMGR_TYPES_H_
#define FWK_PLATFORM_DEVMGR_TYPES_H_

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include "EmbeddedTypes.h"
#include "fwk_errno.h"

/*!
 * @addtogroup DevMgr
 *
 * Shared DevMgr vocabulary (status codes, address sizes and MAC-source masks).
 *
 * This header carries only the type and macro definitions that are common to
 *      both the DevMgr service layer (fwk_devmgr.h) and the per-platform layer
 *      (fwk_platform_devmgr.h). Keeping them here lets the platform contract
 *      header stay self-contained at the platform layer without pulling in the
 *      public service API header.
 *
 * @{
 */

/**********************************************************************************
 *************************************************************************************
 * Public macros
 *************************************************************************************
 ********************************************************************************** */

/*
 * Size of a Bluetooth device address (48-bit EUI).
 */
#define DEVMGR_BD_ADDR_SZ 6U

/*
 * Size of an IEEE 802.15.4 device address (64-bit EUI).
 */
#define DEVMGR_154_ADDR_SZ 8U

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

/*!
 * @} end of DevMgr addtogroup
 */

#endif /* FWK_PLATFORM_DEVMGR_TYPES_H_ */
