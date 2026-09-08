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

/* The CFPA/CMPA IFR transactional flash engine is always compiled, but only referenced
 * when the platform-specific DevMgr backend is selected (see the DevMgr
 * Kconfig / CMakeLists). Under the default HWParameter backend the engine is
 * built but never pulled in by linker, keeping existing builds unchanged. */
#include "fwk_platform_ifr.h"

#include "FunctionLib.h"

/* The UID-derived MAC composition helpers (PLATFORM_DEVMGR_Get*AddrFromUID)
 * along with the shared OUI tables and CRC-32 configuration live in the common
 * platform/wireless_mcu/fwk_platform_uid_mac.c source shared by the wireless_mcu
 * platform family. */
/* -------------------------------------------------------------------------- */
/*                              Private helpers                               */
/* -------------------------------------------------------------------------- */
static int s_CfpaStatusToDevMgr(int dev_stat)
{
    return (dev_stat == DEVMGR_STATUS_OK) ? DEVMGR_Success_c : DEVMGR_Error_c;
}

/**********************************************************************************
 *************************************************************************************
 * Public functions - shared by all DevMgr backends
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
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t  cfpaMac[DEVMGR_BD_ADDR_SZ];
        int      cfpa_st;
        uint32_t i;

        if (pOutAddr == NULL)
        {
            break;
        }

        /* Read the raw customer BLE MAC from the CFPA store. The CFPA field
         * layout matches the NMPA IFR (most-significant octet first). */
        cfpa_st = PLATFORM_DevMgrGetCfpaCustBleMacAddr(cfpaMac, DEVMGR_BD_ADDR_SZ);
        if (cfpa_st != DEVMGR_STATUS_OK)
        {
            status = s_CfpaStatusToDevMgr(cfpa_st);
            break;
        }

        /* An all-0xFF field means the customer BD address is blank. */
        if (FLib_MemCmpToVal((const void *)cfpaMac, 0xFFU, DEVMGR_BD_ADDR_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }

        /* CFPA stores the address in reverse byte order (like the NMPA IFR);
         * copy it back to the natural little-endian layout expected by
         * consumers. */
        for (i = 0U; i < DEVMGR_BD_ADDR_SZ; i++)
        {
            pOutAddr[i] = cfpaMac[DEVMGR_BD_ADDR_SZ - 1U - i];
        }

        status = DEVMGR_Success_c;
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t  cfpaMac[DEVMGR_BD_ADDR_SZ];
        uint8_t  curMac[DEVMGR_BD_ADDR_SZ];
        int      cfpa_st;
        uint32_t i;

        if (pAddr == NULL)
        {
            break;
        }

        /* Consumers pass the address in the natural little-endian layout;
         * store it into CFPA in the reversed (most-significant octet first)
         * layout, matching the NMPA IFR convention. */
        for (i = 0U; i < DEVMGR_BD_ADDR_SZ; i++)
        {
            cfpaMac[i] = pAddr[DEVMGR_BD_ADDR_SZ - 1U - i];
        }

        /* Per the DevMgr spec, a set that would not change the stored value is
         * a no-op: read back the current customer BLE MAC and, if it already
         * equals the requested value, return success without programming CFPA
         * (which would otherwise erase/commit and reset the device). */
        cfpa_st = PLATFORM_DevMgrGetCfpaCustBleMacAddr(curMac, DEVMGR_BD_ADDR_SZ);
        if ((cfpa_st == DEVMGR_STATUS_OK) &&
            (FLib_MemCmp((const void *)curMac, (const void *)cfpaMac, DEVMGR_BD_ADDR_SZ) != FALSE))
        {
            status = DEVMGR_Success_c;
            break;
        }

        /* Note: a successful CFPA commit resets the device (see
         * PLATFORM_DevMgrCommitZone), so this call may not return. */
        cfpa_st = PLATFORM_DevMgrSetCfpaCustBleMacAddr(cfpaMac, DEVMGR_BD_ADDR_SZ);
        status  = s_CfpaStatusToDevMgr(cfpa_st);
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_Get154AddressCust(uint8_t *pOutAddr)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t  cfpaMac[DEVMGR_154_ADDR_SZ];
        int      cfpa_st;
        uint32_t i;

        if (pOutAddr == NULL)
        {
            break;
        }

        /* Read the raw customer 802.15.4 MAC from the CFPA store. */
        cfpa_st = PLATFORM_DevMgrGetCfpaCust802154MacAddr(cfpaMac, DEVMGR_154_ADDR_SZ);
        if (cfpa_st != DEVMGR_STATUS_OK)
        {
            status = s_CfpaStatusToDevMgr(cfpa_st);
            break;
        }

        /* An all-0xFF field means the customer 802.15.4 address is blank. */
        if (FLib_MemCmpToVal((const void *)cfpaMac, 0xFFU, DEVMGR_154_ADDR_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }

        /* CFPA stores the address in reverse byte order (like the NMPA IFR);
         * copy it back to the natural little-endian layout expected by
         * consumers. */
        for (i = 0U; i < DEVMGR_154_ADDR_SZ; i++)
        {
            pOutAddr[i] = cfpaMac[DEVMGR_154_ADDR_SZ - 1U - i];
        }

        status = DEVMGR_Success_c;
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_Set154Address(const uint8_t *pAddr)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t  cfpaMac[DEVMGR_154_ADDR_SZ];
        uint8_t  curMac[DEVMGR_154_ADDR_SZ];
        int      cfpa_st;
        uint32_t i;

        if (pAddr == NULL)
        {
            break;
        }

        /* Consumers pass the address in the natural little-endian layout;
         * store it into CFPA in the reversed (most-significant octet first)
         * layout, matching the NMPA IFR convention. */
        for (i = 0U; i < DEVMGR_154_ADDR_SZ; i++)
        {
            cfpaMac[i] = pAddr[DEVMGR_154_ADDR_SZ - 1U - i];
        }

        /* Per the DevMgr spec, a set that would not change the stored value is
         * a no-op: read back the current customer 802.15.4 MAC and, if it
         * already equals the requested value, return success without
         * programming CFPA (which would otherwise erase/commit and reset the
         * device). */
        cfpa_st = PLATFORM_DevMgrGetCfpaCust802154MacAddr(curMac, DEVMGR_154_ADDR_SZ);
        if ((cfpa_st == DEVMGR_STATUS_OK) &&
            (FLib_MemCmp((const void *)curMac, (const void *)cfpaMac, DEVMGR_154_ADDR_SZ) != FALSE))
        {
            status = DEVMGR_Success_c;
            break;
        }

        /* Note: a successful CFPA commit resets the device (see
         * PLATFORM_DevMgrCommitZone), so this call may not return. */
        cfpa_st = PLATFORM_DevMgrSetCfpaCust802154MacAddr(cfpaMac, DEVMGR_154_ADDR_SZ);
        status  = s_CfpaStatusToDevMgr(cfpa_st);
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_Get32MHzTrim(uint8_t *pOutTrim)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t trim;

        if (pOutTrim == NULL)
        {
            break;
        }

        /* The 32 MHz crystal trim (XTAL capacitance) is carried in the CFPA
         * store. An erased (0xFF) field means the trim is unprovisioned. */
        trim = PLATFORM_DevMgrGetCfpaXtal32MHzCfg();
        if (trim == DEVMGR_BLANK_BYTE)
        {
            *pOutTrim = DEVMGR_BLANK_BYTE;
            status    = DEVMGR_Blank_c;
            break;
        }

        *pOutTrim = trim;
        status    = DEVMGR_Success_c;
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_Set32MHzTrim(uint8_t trim)
{
    int status;

    /* Per the DevMgr spec, a set that would not change the stored value is a
     * no-op: if the CFPA field already holds the requested value, return
     * success without programming CFPA (which would otherwise erase/commit and
     * reset the device). */
    if (PLATFORM_DevMgrGetCfpaXtal32MHzCfg() == trim)
    {
        status = DEVMGR_Success_c;
    }
    else
    {
        /* Note: a successful CFPA commit resets the device (see
         * PLATFORM_DevMgrCommitZone), so this call may not return. */
        status = s_CfpaStatusToDevMgr(PLATFORM_DevMgrSetCfpaXtal32MHzCfg(trim));
    }

    return status;
}

int PLATFORM_DEVMGR_Get32kHzTrim(uint8_t *pOutCap)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t cap;

        if (pOutCap == NULL)
        {
            break;
        }

        /* The 32 kHz crystal load-capacitance trim is carried in the CFPA
         * store. An erased (0xFF) field means the trim is unprovisioned. */
        cap = PLATFORM_DevMgrGetCfpaXtal32kHzCfg();
        if (cap == DEVMGR_BLANK_BYTE)
        {
            *pOutCap = DEVMGR_BLANK_BYTE;
            status   = DEVMGR_Blank_c;
            break;
        }

        *pOutCap = cap;
        status   = DEVMGR_Success_c;
    } while (false);

    return status;
}

int PLATFORM_DEVMGR_Set32kHzTrim(uint8_t cap)
{
    int status;

    /* Per the DevMgr spec, a set that would not change the stored value is a
     * no-op: if the CFPA field already holds the requested value, return
     * success without programming CFPA (which would otherwise erase/commit and
     * reset the device). */
    if (PLATFORM_DevMgrGetCfpaXtal32kHzCfg() == cap)
    {
        status = DEVMGR_Success_c;
    }
    else
    {
        /* Note: a successful CFPA commit resets the device (see
         * PLATFORM_DevMgrCommitZone), so this call may not return. */
        status = s_CfpaStatusToDevMgr(PLATFORM_DevMgrSetCfpaXtal32kHzCfg(cap));
    }

    return status;
}
