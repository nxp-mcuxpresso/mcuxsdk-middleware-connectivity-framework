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
/* fwk_platform_definitions.h provides the per-family FWK_*_FAMILIES selectors
 * (e.g. FWK_MCXW23_FAMILIES) used to fork platform-specific behavior below. */
#include "fwk_platform_definitions.h"

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */

int DEVMGR_GetBdAddress(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    return PLATFORM_DEVMGR_GetBdAddress(pOutAddr, fromLocation);
}

int DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    return PLATFORM_DEVMGR_SetBdAddress(pAddr);
}

int DEVMGR_Get154Address(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    return PLATFORM_DEVMGR_Get154Address(pOutAddr, fromLocation);
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
