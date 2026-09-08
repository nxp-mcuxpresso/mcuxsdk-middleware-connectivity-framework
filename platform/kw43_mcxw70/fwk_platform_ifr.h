/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FWK_PLATFORM_IFR_H_
#define FWK_PLATFORM_IFR_H_

/*!
 * @file fwk_platform_ifr.h
 * @brief DEVMGR (Flash Boot Device Configuration) IFR0 zone accessor engine
 *        for KW43/MCXW70.
 *
 * Provides getters and setters for:
 *   - CFPA  zone              (IFR0 DEVCFG sector offset 0x010)
 *   - CMPA  zone              (IFR0 DEVCFG sector offset 0x200)
 *   - CMPA FW Update zone     (IFR0 DEVCFG sector offset 0x330)
 *   - CMPA Customer Defined   (IFR0 DEVCFG sector offset 0x400)
 *
 * This module is the low-level transactional CFPA/CMPA flash engine that the
 * platform DevMgr backend (fwk_platform_devmgr.c) uses to service the
 * customer-store PLATFORM_DEVMGR_* accessors. It is compiled only when the
 * platform DevMgr backend is selected (see Kconfig
 * MCUX_COMPONENT_middleware.wireless.framework.devmgr_plat).
 *
 * Transactional write model
 * -------------------------
 * Multiple field modifications to a zone can be batched into a single flash
 * erase/program cycle:
 *
 *  1. Call PLATFORM_DevMgrOpenZone() to acquire the zone mutex and load the
 *     current DEVCFG content into the static RAM overlay buffer.
 *  2. Call any number of field setters.  While the zone is open, setters only
 *     update the RAM overlay - no flash operation occurs.
 *  3. Call PLATFORM_DevMgrCommitZone() to program the IFR0 DEVMGR scratch sector
 *     phrase-by-phrase and release the mutex.  Phrases outside the modified
 *     zone are sourced from the DEVCFG read-only mirror; the UPDATE zone
 *     header is written last after all other phrases are verified.
 *  4. Alternatively, call PLATFORM_DevMgrAbortZone() to discard the RAM
 *     overlay without programming flash and release the mutex.
 *
 * Thread safety
 * -------------
 * PLATFORM_DevMgrOpenZone() acquires an OSA mutex.  Only one zone can be open
 * at a time.  PLATFORM_DevMgrCommitZone() and PLATFORM_DevMgrAbortZone() both
 * release the mutex.  PLATFORM_DevMgrEraseSector() also takes the same mutex
 * for the duration of the erase, but does not load a zone into the RAM overlay
 * (the "zone open" state and "mutex held" state are tracked separately).
 *
 * Getters do not take the mutex - they may legitimately be called by the thread
 * that already holds an open zone, and the mutex is not recursive.  Instead
 * each getter reads the "zone open" flag, selects the overlay-or-flash source
 * and copies the requested bytes inside a short interrupt-masked critical
 * section, so a getter cannot observe a half-updated state while another
 * context commits/aborts a zone or opens a different one.
 *
 * Single-field convenience setters
 * ---------------------------------
 * Each individual field setter calls Open -> modify -> Commit internally,
 * which is the correct pattern when only one field needs updating.  If
 * batching several fields is desired, use the transactional API directly.
 *
 * Getters
 * -------
 * If the zone is currently open (RAM overlay is valid) getters read from the
 * RAM overlay.  Otherwise they read directly from the DEVCFG flash mirror.
 */

#include <stdint.h>
#include <stddef.h>
#include "ifr0map_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                              Public constants                              */
/* -------------------------------------------------------------------------- */

/*! Base physical address of the IFR0 DEVCFG read-only mirror (sector 0). */
#define DEVMGR_IFR0_DEVCFG_BASE (FSL_FEATURE_FLASH_IFR0_START_ADDRESS)

/*! Base physical address of the IFR0 DEVMGR scratch sector (sector 1, read/write). */
#define DEVMGR_IFR0_SCRATCH_BASE (DEVMGR_IFR0_DEVCFG_BASE + FSL_FEATURE_FLASH_PFLASH_SECTOR_SIZE)

/*! Access to the top of the DEVMGR page where NFPA is located is banned */
#define DEVMGR_NFPA_SIZE 0x800U

/*! Return codes */
#define DEVMGR_STATUS_OK         (0)
#define DEVMGR_STATUS_ERR_PARAM  (-1)
#define DEVMGR_STATUS_ERR_FLASH  (-2)
#define DEVMGR_STATUS_ERR_VERIFY (-3)
#define DEVMGR_STATUS_ERR_BUSY   (-4) /*!< Mutex acquisition failed (timeout). */
#define DEVMGR_STATUS_ERR_STATE  (-5) /*!< Operation invalid in current open/closed state. */

/* -------------------------------------------------------------------------- */
/*                              Zone identifiers                              */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Identifies which zone to open for a transactional write session.
 */
typedef enum _fbdc_zone_id
{
    kDevMgrZone_Cfpa         = 0u, /*!< CFPA zone (offset 0x010, sizeof cfpa_zone_t). */
    kDevMgrZone_Cmpa         = 1u, /*!< CMPA zone (offset 0x200, sizeof cmpa_zone_t). */
    kDevMgrZone_CmpaCustomer = 2u, /*!< CMPA Customer Defined zone (offset 0x400, sizeof cmpa_cust_defined_zone_t). */
} fbdc_zone_id_t;

/* -------------------------------------------------------------------------- */
/*                         Transactional open/commit/abort                    */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Open a zone for batched modification.
 *
 * Acquires the DEVMGR mutex, loads the zone content from the DEVCFG read-only
 * mirror into the static RAM overlay buffer and marks the zone as open.
 * Subsequent getter calls return values from the RAM overlay; setter calls
 * update only the RAM overlay.
 *
 * @param[in] zone_id   Zone to open (@ref fbdc_zone_id_t).
 * @param[in] timeout_ms  Mutex acquisition timeout in milliseconds.
 *                        Pass 0 to poll without blocking.
 *
 * @return DEVMGR_STATUS_OK on success.
 * @return DEVMGR_STATUS_ERR_BUSY   if the mutex could not be acquired within
 *                                @p timeout_ms.
 * @return DEVMGR_STATUS_ERR_STATE  if a zone is already open.
 */
int PLATFORM_DevMgrOpenZone(fbdc_zone_id_t zone_id, uint32_t timeout_ms);

/*!
 * @brief Commit a previously opened zone to the IFR0 DEVMGR scratch sector.
 *
 * Programs the scratch sector phrase-by-phrase:
 *  - Phrases outside the modified zone are sourced from the DEVCFG mirror.
 *  - Phrases inside the modified zone are sourced from the RAM overlay.
 *  - The UPDATE zone header (upd_type + zeroed params) is written as the
 *    very last phrase so that only a fully verified write triggers ROM action.
 *
 * On return (success or failure) the zone is closed and the mutex released.
 * \note This function never returns as it provokes an intentional system reset.
 *
 * @return DEVMGR_STATUS_OK on success, negative error code on failure.
 */
int PLATFORM_DevMgrCommitZone(void);

/*!
 * @brief Abort a previously opened zone without programming flash.
 *
 * Discards the RAM overlay and releases the mutex.
 */
void PLATFORM_DevMgrAbortZone(void);

/*!
 * @brief Erase scratch sector under mutex protection.
 *
 * \note : Does nothing if mutex is already taken.
 */
int PLATFORM_DevMgrEraseSector(void);

/* -------------------------------------------------------------------------- */
/*                          CFPA zone getters/setters                         */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Read the full CFPA zone.
 *
 * Reads from the RAM overlay if the CFPA zone is currently open, otherwise
 * reads directly from the DEVCFG flash mirror.
 *
 * @param[out] dst   Caller-allocated @ref cfpa_zone_t buffer.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM if dst is NULL.
 */
int PLATFORM_DevMgrGetCfpaZone(cfpa_zone_t *dst);

/*!
 * @brief Write a complete new CFPA zone image (single-operation convenience).
 *
 * Equivalent to Open(kDevMgrZone_Cfpa) + copy @p src into overlay + Commit.
 *
 * @param[in] src   Pointer to the new @ref cfpa_zone_t data.
 * @return DEVMGR_STATUS_OK on success, negative error code on failure.
 */
int PLATFORM_DevMgrSetCfpaZone(const cfpa_zone_t *src);

/* -- Individual CFPA field getters ---------------------------------------- */

uint32_t PLATFORM_DevMgrGetCfpaHeader(void);

/*!
 * @brief Get customer BLE MAC address (6 bytes).
 * @param[out] buf  Output buffer (must be at least 6 bytes).
 * @param[in]  len  Buffer length (must be 6).
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM on invalid arguments.
 */
int PLATFORM_DevMgrGetCfpaCustBleMacAddr(uint8_t *buf, uint8_t len);

/*!
 * @brief Get customer IEEE 802.15.4 MAC address (8 bytes).
 * @param[out] buf  Output buffer (must be at least 8 bytes).
 * @param[in]  len  Buffer length (must be 8).
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM on invalid arguments.
 */
int PLATFORM_DevMgrGetCfpaCust802154MacAddr(uint8_t *buf, uint8_t len);

/*!
 * @brief Get XTAL 32kHz configuration.
 * @return XTAL 32kHz trim value.
 */
uint8_t PLATFORM_DevMgrGetCfpaXtal32kHzCfg(void);

/*!
 * @brief Get XTAL 32MHz configuration.
 * @return XTAL 32MHz capacitance value.
 */
uint8_t PLATFORM_DevMgrGetCfpaXtal32MHzCfg(void);

/* -- Individual CFPA field setters ---------------------------------------- */

/*!
 * @brief Individual CFPA field setters : Update a single CFPA field.
 *
 * If the CFPA zone is open, updates the RAM overlay only (no flash write).
 * If the zone is not open, performs Open + modify + Commit as a single
 * atomic operation (convenience setter).
 */

/*!
 * @brief Set customer BLE MAC address (6 bytes).
 * @param[in] buf  Source buffer containing MAC address.
 * @param[in] len  Buffer length (must be 6).
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM on invalid arguments.
 */
int PLATFORM_DevMgrSetCfpaCustBleMacAddr(uint8_t *buf, uint8_t len);

/*!
 * @brief Set customer IEEE 802.15.4 MAC address (8 bytes).
 * @param[in] buf  Source buffer containing MAC address.
 * @param[in] len  Buffer length (must be 8).
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM on invalid arguments.
 */
int PLATFORM_DevMgrSetCfpaCust802154MacAddr(uint8_t *buf, uint8_t len);

/*!
 * @brief Set XTAL 32kHz configuration.
 * @param[in] val  XTAL 32kHz trim value.
 * @return DEVMGR_STATUS_OK on success.
 */
int PLATFORM_DevMgrSetCfpaXtal32kHzCfg(uint8_t val);

/*!
 * @brief Set XTAL 32MHz configuration.
 * @param[in] val  XTAL 32MHz capacitance value.
 * @return DEVMGR_STATUS_OK on success.
 */
int PLATFORM_DevMgrSetCfpaXtal32MHzCfg(uint8_t val);

/* -------------------------------------------------------------------------- */
/*                          CMPA zone getters/setters                         */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Read the full CMPA zone.
 *
 * Reads from the RAM overlay if the CMPA zone is currently open, otherwise
 * reads directly from the DEVCFG flash mirror.
 *
 * @param[out] dst   Caller-allocated @ref cmpa_zone_t buffer.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM if dst is NULL.
 */
int PLATFORM_DevMgrGetCmpaZone(cmpa_zone_t *dst);

/*!
 * @brief Write a complete new CMPA zone image (single-operation convenience).
 *
 * @param[in] src   Pointer to the new @ref cmpa_zone_t data.
 * @return DEVMGR_STATUS_OK on success, negative error code on failure.
 */
int PLATFORM_DevMgrSetCmpaZone(const cmpa_zone_t *src);

/* -------------------------------------------------------------------------- */
/*                    CMPA FW Update zone getters/setters                     */
/* -------------------------------------------------------------------------- */

/* -- Individual CMPA FW Update field getters ------------------------------- */

uint32_t PLATFORM_DevMgrGetCmpaNbuRecoveryImgAddr(void);
uint32_t PLATFORM_DevMgrGetCmpaNbuUpdateImgAddr(void);
uint32_t PLATFORM_DevMgrGetCmpaCpu0RecoveryImgAddr(void);
uint32_t PLATFORM_DevMgrGetCmpaCpu0UpdateImgAddr(void);

/* -- Individual CMPA FW Update field setters ------------------------------- */

int PLATFORM_DevMgrSetCmpaNbuRecoveryImgAddr(uint32_t val);
int PLATFORM_DevMgrSetCmpaNbuUpdateImgAddr(uint32_t val);
int PLATFORM_DevMgrSetCmpaCpu0RecoveryImgAddr(uint32_t val);
int PLATFORM_DevMgrSetCmpaCpu0UpdateImgAddr(uint32_t val);

/* -------------------------------------------------------------------------- */
/*               CMPA Customer Defined zone getters/setters                  */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Read the full CMPA customer-defined zone.
 *
 * Reads from the RAM overlay if kDevMgrZone_CmpaCustomer is open, otherwise from
 * the DEVCFG flash mirror.
 *
 * @param[out] dst  Caller-allocated @ref cmpa_cust_defined_zone_t buffer.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM if dst is NULL.
 */
int PLATFORM_DevMgrGetCmpaCustomerDefinedZone(cmpa_cust_defined_zone_t *dst);

/*!
 * @brief Write a complete new CMPA customer-defined zone (single-operation convenience).
 *
 * @param[in] src  Pointer to the new @ref cmpa_cust_defined_zone_t data.
 * @return DEVMGR_STATUS_OK on success, negative error code on failure.
 */
int PLATFORM_DevMgrSetCmpaCustomerDefinedZone(const cmpa_cust_defined_zone_t *src);

/*!
 * @brief Read a single 32-bit word from the customer-defined area (index 0-1279).
 *
 * Reads from the RAM overlay if kDevMgrZone_CmpaCustomer is open.
 *
 * @param[in]  idx  Word index (0-1279).
 * @param[out] val  Output value.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM if idx >= 1280 or val == NULL.
 */
int PLATFORM_DevMgrGetCmpaCustomerDefinedWord(uint16_t idx, uint32_t *val);

/*!
 * @brief Write a single 32-bit word to the customer-defined area (index 0-1279).
 *
 * If the zone is open, updates the RAM overlay only.  If not open, performs an
 * Open + modify + Commit cycle.
 *
 * @param[in] idx  Word index (0-1279).
 * @param[in] val  Value to write.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM if idx >= 1280.
 */
int PLATFORM_DevMgrSetCmpaCustomerDefinedWord(uint16_t idx, uint32_t val);

/*!
 * @brief Read a byte-aligned sub-region of the customer-defined area (0..5119 bytes).
 *
 * Reads from the RAM overlay if kDevMgrZone_CmpaCustomer is open.
 *
 * @param[in]  byte_offset  Byte offset within the customer-defined area.
 * @param[out] buf          Destination buffer.
 * @param[in]  len          Number of bytes to copy.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM on invalid arguments.
 */
int PLATFORM_DevMgrGetCmpaCustomerDefinedBytes(uint16_t byte_offset, uint8_t *buf, uint16_t len);

/*!
 * @brief Write a byte-aligned sub-region of the customer-defined area.
 *
 * If the zone is open, updates the RAM overlay only.  If not open, performs an
 * Open + modify + Commit cycle.
 *
 * @param[in] byte_offset  Byte offset within the customer-defined area (0..5119).
 * @param[in] buf          Source buffer.
 * @param[in] len          Number of bytes to write.
 * @return DEVMGR_STATUS_OK, or DEVMGR_STATUS_ERR_PARAM on invalid arguments.
 */
int PLATFORM_DevMgrSetCmpaCustomerDefinedBytes(uint16_t byte_offset, const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* FWK_PLATFORM_IFR_H_ */
