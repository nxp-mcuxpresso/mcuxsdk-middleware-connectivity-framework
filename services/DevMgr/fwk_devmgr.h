/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef FWK_DEVMGR_H_
#define FWK_DEVMGR_H_

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include "EmbeddedTypes.h"
#include "fwk_platform_devmgr_types.h"

/*!
 * @addtogroup DevMgr
 * The Dev Manager / Manufacturing Data abstraction module.
 *
 * DevMgr is the entry point for accessing manufacturing-related device
 *      information (BD address, IEEE 802.15.4 address, crystal trim values and
 *      future provisioning data). It abstracts the underlying storage backend
 *      (currently Hardware Parameter, later IFR / Dev Manager / secure access)
 *      behind a single stable public API.
 *
 * All DEVMGR backends implement the same public DEVMGR_* API (identical function
 *      names, signatures and return semantics); backends differ only in their
 *      internal implementation.
 *
 * @{
 */

/**********************************************************************************
 *************************************************************************************
 * Public prototypes
 *************************************************************************************
 ********************************************************************************** */

#if defined(__cplusplus)
extern "C" {
#endif

/*! *********************************************************************************
 * \brief  Get the Bluetooth device address from the manufacturing data backend.
 *
 * \details On the HWParameter backend the address is resolved through the chain
 *      HWParameter -> per-platform NMPA read -> optional MAC generation. If no
 *      source yields an address, DEVMGR_Blank_c is returned and 'pOutAddr' is
 *      left filled with 0xFF. On the platform/CFPA backend the call delegates to
 *      the per-platform implementation and returns its status unchanged.
 *
 * \note When the optional MAC-generation fallback is built in and produces an
 *      address, this getter persists it to the customer store as a side effect
 *      (an internal DEVMGR_SetBdAddress call, i.e. a flash write). The write
 *      status is intentionally discarded so a persistence failure never
 *      corrupts the returned value. Because the write path may block, do not
 *      call this getter from an ISR or a short critical section when the
 *      MAC-generation fallback is enabled.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 * \param[in,out] fromLocation 0: any combination of possible MAC address source is accepted
 *                returned with DEVMGR_MAC_SRC_CUST, DEVMGR_MAC_SRC_NMPA or DEVMGR_MAC_SRC_UUID
 *                if a valid source was found, returned 0 otherwise.
 *                - DEVMGR_MAC_SRC_CUST: Requested MAC address comes from CFPA or HWParameter strictly
 *                - DEVMGR_MAC_SRC_NMPA: Requested MAC address comes from NMPA / IFR
 *                - DEVMGR_MAC_SRC_UUID: Requested MAC address is derived from UUID
 *                May be NULL when the caller neither restricts nor needs the source.
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if blank,
 *      DEVMGR_NotSupported_c where unsupported, DEVMGR_Error_c on a NULL pointer
 *      or backend failure.
 ********************************************************************************** */
int DEVMGR_GetBdAddress(uint8_t *pOutAddr, uint32_t *fromLocation);

/*! *********************************************************************************
 * \brief  Set (program) the Bluetooth device address in the backend.
 *         The considered permanent storage is the CUST persistent memory, i.e. HwParameter or CFPA.
 *
 * \param[in] pAddr  buffer of DEVMGR_BD_ADDR_SZ bytes containing the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on backend failure.
 ********************************************************************************** */
int DEVMGR_SetBdAddress(const uint8_t *pAddr);

/*! *********************************************************************************
 * \brief  Get the IEEE 802.15.4 device address from the manufacturing data backend.
 *
 * \details On the HWParameter backend the address is resolved through the chain
 *      HWParameter -> per-platform NMPA read -> optional MAC generation. If no
 *      source yields an address, DEVMGR_Blank_c is returned and 'pOutAddr' is
 *      left filled with 0xFF. On the platform/CFPA backend the call delegates to
 *      the per-platform implementation and returns its status unchanged.
 *
 * \note When the optional MAC-generation fallback is built in and produces an
 *      address, this getter persists it to the customer store as a side effect
 *      (an internal DEVMGR_Set154Address call, i.e. a flash write). The write
 *      status is intentionally discarded so a persistence failure never
 *      corrupts the returned value. Because the write path may block, do not
 *      call this getter from an ISR or a short critical section when the
 *      MAC-generation fallback is enabled.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_154_ADDR_SZ bytes receiving the address
 * \param[in,out] fromLocation 0: any combination of possible MAC address source is accepted
 *                returned with DEVMGR_MAC_SRC_CUST, DEVMGR_MAC_SRC_NMPA or DEVMGR_MAC_SRC_UUID
 *                if a valid source was found, returned 0 otherwise.
 *                - DEVMGR_MAC_SRC_CUST: Requested MAC address comes from CFPA or HWParameter strictly
 *                - DEVMGR_MAC_SRC_NMPA: Requested MAC address comes from NMPA / IFR
 *                - DEVMGR_MAC_SRC_UUID: Requested MAC address is derived from UUID
 *                May be NULL when the caller neither restricts nor needs the source.
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if blank,
 *      DEVMGR_NotSupported_c where unsupported, DEVMGR_Error_c on a NULL pointer
 *      or backend failure.
 ********************************************************************************** */
int DEVMGR_Get154Address(uint8_t *pOutAddr, uint32_t *fromLocation);

/*! *********************************************************************************
 * \brief  Set (program) the IEEE 802.15.4 device address in the backend.
 *         The considered permanent storage is the CUST persistent memory, i.e. HwParameter or CFPA.
 *
 * \param[in] pAddr  buffer of DEVMGR_154_ADDR_SZ bytes containing the address
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on backend failure.
 ********************************************************************************** */
int DEVMGR_Set154Address(const uint8_t *pAddr);

/*! *********************************************************************************
 * \brief  Get the 32 MHz crystal trim value from the manufacturing data backend.
 *
 * \details On a supported platform the value is read from the active backend
 *      (HWParameter 'xtalTrim' or the platform/CFPA store). If the stored value
 *      is blank (0xFF), DEVMGR_Blank_c is returned and '*pOutTrim' is left set to
 *      0xFF. On a platform that carries no crystal-trim field,
 *      DEVMGR_NotSupported_c is returned.
 *
 * \param[out] pOutTrim  output receiving the 32 MHz trim value
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if blank,
 *      DEVMGR_NotSupported_c if the platform carries no such field,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int DEVMGR_Get32MHzTrim(uint8_t *pOutTrim);

/*! *********************************************************************************
 * \brief  Set (program) the 32 MHz crystal trim value in the backend.
 *
 * \param[in] trim  the 32 MHz trim value to program
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on backend failure.
 ********************************************************************************** */
int DEVMGR_Set32MHzTrim(uint8_t trim);

/*! *********************************************************************************
 * \brief  Get the 32 kHz crystal capacitance/trim value from the backend.
 *         The considered permanent storage is the CUST persistent memory, i.e. HwParameter or CFPA.
 *
 * \details If the stored value is blank (0xFF), DEVMGR_Blank_c is returned and
 *      '*pOutCap' is left set to 0xFF.
 *
 * \param[out] pOutCap  output receiving the 32 kHz capacitance value
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if blank,
 *      DEVMGR_NotSupported_c if the platform carries no such field,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int DEVMGR_Get32kHzTrim(uint8_t *pOutCap);

/*! *********************************************************************************
 * \brief  Set (program) the 32 kHz crystal capacitance/trim value in the backend.
 *         The considered permanent storage is the CUST persistent memory, i.e. HwParameter or CFPA.
 *
 * \param[in] cap  the 32 kHz capacitance value to program
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on backend failure.
 ********************************************************************************** */
int DEVMGR_Set32kHzTrim(uint8_t cap);

#if defined(__cplusplus)
}
#endif

/*!
 * @} end of DevMgr addtogroup
 */

#endif /* FWK_DEVMGR_H_ */
