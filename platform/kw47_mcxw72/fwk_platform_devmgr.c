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
 * KW47 / MCXW72 carries a factory-provisioned BD address in the ROMCFG IFR
 * sector (IFR_BLE_BD_ADDR) but no proper IEEE 802.15.4 MAC address in NMPA.
 * The HWParameter backend is the active DevMgr backend on this platform, so the
 * platform-store accessors (PLATFORM_DEVMGR_Get/SetBdAddress,
 * PLATFORM_DEVMGR_Get/Set154Address) are not the primary address store here.
 *
 ********************************************************************************** */

int PLATFORM_DEVMGR_GetBdAddrFromNMPA(uint8_t *pOutAddr)
{
    int status = DEVMGR_Error_c;

    do
    {
        const uint8_t *pIfrBdAddr = (const uint8_t *)IFR_BLE_BD_ADDR;
        if (pOutAddr == NULL)
        {
            break;
        }

        /* An all-0xFF field means the BD address is unprovisioned (blank). */
        if (FLib_MemCmpToVal((const void *)pIfrBdAddr, 0xFFU, DEVMGR_BD_ADDR_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }

        /* The IFR stores the address in reverse byte order; copy it back to
         * the natural little-endian byte layout expected by consumers. */
        for (uint32_t i = 0U; i < DEVMGR_BD_ADDR_SZ; i++)
        {
            pOutAddr[i] = pIfrBdAddr[DEVMGR_BD_ADDR_SZ - 1U - i];
        }

        status = DEVMGR_Success_c;
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_Get154AddrFromNMPA(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    /* KW47/MCXW72 carries no proper IEEE 802.15.4 MAC address in NMPA. */
    return DEVMGR_NotSupported_c;
}
