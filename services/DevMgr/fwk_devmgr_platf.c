/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/*
 * DevMgr platform/CFPA backend.
 *
 * On platforms whose manufacturing data lives in the platform store (CFPA/NMPA)
 * rather than in the HWParameter flash region, the service-level DEVMGR_Get*
 * functions explicitly sequence the resolution chain:
 *
 *   BD address : customer store -> NMPA -> UID-derived
 *   802.15.4   : platform store -> NMPA -> UID-derived
 *
 * Each step is implemented by the matching PLATFORM_DEVMGR_* function in the
 * platform's fwk_platform_devmgr.c. A step is attempted only when the previous
 * one returns DEVMGR_Blank_c or DEVMGR_NotSupported_c ("no value from this
 * source"). No MAC generation occurs on this backend; if every step fails the
 * getter returns DEVMGR_Blank_c. The setters remain thin delegations.
 *
 * Selected on:
 *   - KW43 / MCXW70
 *   - MCXW23
 */

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include <stdbool.h>
#include "fsl_common.h"
#include "FunctionLib.h"
#include "fwk_devmgr.h"
#include "fwk_devmgr_common.h"
#include "fwk_platform_devmgr.h"
#include "fwk_platform_devmgr.h"

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */

int DEVMGR_GetBdAddress(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    /* The order of the functions in the array define the precedence of the MAC address sources:
     * CUST (HWParameter/CFPA)->NMPA->UUID.
     * A 4th option that generates and writes the MAC address (based on DEVMGR_MAC_GEN_ENABLED build option)
     * exists for the HwParameter based implementation. In the platform based implementation it might be considered
     * (e.g if CFPA based on KW43/MCXW70, but not for MCXW23. It is discarded for now.
     */
    static const devmgr_src_entry_t tbl[] = {
        {DEVMGR_MAC_SRC_CUST, PLATFORM_DEVMGR_GetBdAddressCust},
        {DEVMGR_MAC_SRC_NMPA, PLATFORM_DEVMGR_GetBdAddrFromNMPA},
        {DEVMGR_MAC_SRC_UUID, PLATFORM_DEVMGR_GetBdAddrFromUID},
    };
    /* fromLocation is returned with the bit mask corresponding to the successful MAC retrieval / or 0 otherwise */
    return DEVMGR_MacAddrReader(pOutAddr, DEVMGR_BD_ADDR_SZ, fromLocation, tbl, ARRAY_SIZE(tbl));
}

int DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    return PLATFORM_DEVMGR_SetBdAddress(pAddr);
}

int DEVMGR_Get154Address(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    /* The order of the functions in the array define the precedence of the MAC address sources:
     * CUST (HWParameter/CFPA)->NMPA->UUID.
     * A 4th option that generates and writes the MAC address (based on DEVMGR_MAC_GEN_ENABLED build option)
     * exists for the HwParameter based implementation. In the platform based implementation it might be considered
     * (e.g if CFPA based on KW43/MCXW70, but not for MCXW23. It is discarded for now.
     */
    static const devmgr_src_entry_t tbl[] = {
        {DEVMGR_MAC_SRC_CUST, PLATFORM_DEVMGR_Get154AddressCust},
        {DEVMGR_MAC_SRC_NMPA, PLATFORM_DEVMGR_Get154AddrFromNMPA},
        {DEVMGR_MAC_SRC_UUID, PLATFORM_DEVMGR_Get154AddrFromUID},
    };
    /* fromLocation is returned with the bit mask corresponding to the successful MAC retrieval / or 0 otherwise */
    return DEVMGR_MacAddrReader(pOutAddr, DEVMGR_154_ADDR_SZ, fromLocation, tbl, ARRAY_SIZE(tbl));
}

int DEVMGR_Set154Address(const uint8_t *pAddr)
{
    return PLATFORM_DEVMGR_Set154Address(pAddr);
}

int DEVMGR_Get32MHzTrim(uint8_t *pOutTrim)
{
    /* The crystal trims are backed by the per-platform store (e.g. the CFPA
     * XTAL config on KW43/MCXW70). Platforms that carry no such field return
     * DEVMGR_NotSupported_c from the accessor. */
    return PLATFORM_DEVMGR_Get32MHzTrim(pOutTrim);
}

int DEVMGR_Set32MHzTrim(uint8_t trim)
{
    return PLATFORM_DEVMGR_Set32MHzTrim(trim);
}

int DEVMGR_Get32kHzTrim(uint8_t *pOutCap)
{
    return PLATFORM_DEVMGR_Get32kHzTrim(pOutCap);
}

int DEVMGR_Set32kHzTrim(uint8_t cap)
{
    return PLATFORM_DEVMGR_Set32kHzTrim(cap);
}
