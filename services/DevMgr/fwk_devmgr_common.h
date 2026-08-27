/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef FWK_DEVMGR_COMMON_H_
#define FWK_DEVMGR_COMMON_H_

/*
 * DevMgr backend-shared internals.
 *
 * This header holds the definitions and helper that are common to more than one
 * DevMgr storage backend (HWParameter, platform/CFPA): the MAC-source table
 * types and the generic source-precedence reader. It is an internal DevMgr
 * header, not part of the public DEVMGR_* API declared in fwk_devmgr.h.
 */

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include "EmbeddedTypes.h"
#include "fwk_platform_devmgr_types.h"

/**********************************************************************************
 *************************************************************************************
 * Public type definitions
 *************************************************************************************
 ********************************************************************************** */

/*! \brief  Source-specific getter reading one MAC address into pAddr. */
typedef int (*devmgr_mac_addr_src_reader)(uint8_t *pAddr);

/*! \brief  One entry of a MAC-address source precedence table. */
typedef struct
{
    uint32_t                   srcBit;
    devmgr_mac_addr_src_reader get;
} devmgr_src_entry_t;

/**********************************************************************************
 *************************************************************************************
 * Public prototypes
 *************************************************************************************
 ********************************************************************************** */

#if defined(__cplusplus)
extern "C" {
#endif

/*! *********************************************************************************
 * \brief  Resolve a MAC address by walking a source precedence table.
 *
 * \details Iterates over the entries of 'tbl' in order and invokes each entry's
 *      getter whose 'srcBit' is selected by the source mask derived from
 *      'fromLocation'. The first getter returning DEVMGR_Success_c wins; its
 *      source bit is written back to '*fromLocation'. If none succeeds,
 *      '*fromLocation' is set to 0. The output buffer is pre-filled with
 *      DEVMGR_BLANK_BYTE.
 *
 * \param[out]    pOutAddr       buffer of 'sz' bytes receiving the address; when
 *                               NULL the function performs no read.
 * \param[in]     sz             size in bytes of the address to read.
 * \param[in,out] fromLocation  0 or NULL: any source accepted; otherwise a
 *                               restricting source mask. On return holds the
 *                               successful source bit, or 0 if none succeeded.
 * \param[in]     tbl            source precedence table.
 * \param[in]     n              number of entries in 'tbl'.
 *
 * \return DEVMGR_Success_c on success, otherwise the status of the last getter
 *      attempted (or DEVMGR_Error_c when no getter ran).
 ********************************************************************************** */
int DEVMGR_MacAddrReader(
    uint8_t *pOutAddr, uint32_t sz, uint32_t *fromLocation, const devmgr_src_entry_t *tbl, uint32_t n);

#if defined(__cplusplus)
}
#endif

#endif /* FWK_DEVMGR_COMMON_H_ */
