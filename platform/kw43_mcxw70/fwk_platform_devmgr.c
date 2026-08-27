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
#include "fwk_platform_devmgr_types.h"
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
 * KW43 / MCXW70 carries a factory-provisioned BD address in the NMPA IFR
 * sector alongside an IEEE 802.15.4 MAC address. The NMPA read-back accessors
 * and the UID-derived composition helpers below are common to both DevMgr
 * backends (HWParameter and platform/CFPA) and are always compiled.
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
    int status = DEVMGR_Error_c;

    do
    {
        const uint8_t *pIfr154Addr = (const uint8_t *)IFR_802_15_4_ADDR;
        if (pOutAddr == NULL)
        {
            break;
        }

        /* An all-0xFF field means the 802.15.4 address is unprovisioned (blank). */
        if (FLib_MemCmpToVal((const void *)pIfr154Addr, 0xFFU, DEVMGR_154_ADDR_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }

        /* The IFR stores the address in reverse byte order; copy it back to
         * the natural little-endian byte layout expected by consumers. */
        for (uint32_t i = 0U; i < DEVMGR_154_ADDR_SZ; i++)
        {
            pOutAddr[i] = pIfr154Addr[DEVMGR_154_ADDR_SZ - 1U - i];
        }

        status = DEVMGR_Success_c;
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_GetBdAddressCust(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    /* Customer BD address store in CFPA on KW43/MCXW7; but not activated yet. HWParameter backend is
     * the active address store and the customer value lives there. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    (void)pAddr;

    /* The IFR BD address region is read-only from the DevMgr platform layer. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get154AddressCust(uint8_t *pOutAddr)
{
    (void)pOutAddr;
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Set154Address(const uint8_t *pAddr)
{
    (void)pAddr;
    return DEVMGR_NotSupported_c;
}
