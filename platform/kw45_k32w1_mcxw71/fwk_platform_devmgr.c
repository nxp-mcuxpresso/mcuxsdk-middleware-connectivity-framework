/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include <stdbool.h>

#include "fwk_platform_devmgr.h"
#include "fwk_platform_definitions.h"
#include "FunctionLib.h"

/* The UID-derived MAC composition helpers (PLATFORM_DEVMGR_Get*AddrFromUID)
 * along with the shared OUI tables and CRC-32 configuration live in the common
 * platform/wireless_mcu/fwk_platform_uid_mac.c source shared by the wireless_mcu
 * platform family. */

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 *
 * KW45 / MCXW71 uses the HWParameter backend (mandatory). It carries neither a
 * BD address nor an IEEE 802.15.4 MAC address in NMPA, so both NMPA reads report
 * DEVMGR_NotSupported_c and the HWParameter resolution chain relies on the
 * HWParameter store, a UID-derived composition, (and optional MAC generation).
 *
 ********************************************************************************** */

int PLATFORM_DEVMGR_GetBdAddrFromNMPA(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    /* KW45/MCXW71 carries no BD address in NMPA. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get154AddrFromNMPA(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    /* KW45/MCXW71 carries no IEEE 802.15.4 MAC address in NMPA. */
    return DEVMGR_NotSupported_c;
}
