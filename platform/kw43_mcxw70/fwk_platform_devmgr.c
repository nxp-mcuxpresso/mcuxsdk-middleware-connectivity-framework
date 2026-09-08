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
#include "fsl_common.h"

#include "fwk_platform_devmgr.h"
#include "fwk_platform_definitions.h"
#include "fwk_devmgr_common.h"

/* The CFPA/CMPA IFR transactional flash engine is always compiled, but only referenced
 * when the platform-specific DevMgr backend is selected (see the DevMgr
 * Kconfig / CMakeLists). Under the default HWParameter backend the engine is
 * built but never pulled in by linker, keeping existing builds unchanged. */
#include "fwk_platform_ifr.h"

#include "FunctionLib.h"

/* DEVMGR_MAC_GEN_ENABLED is defined (to 1) by the DevMgr CMakeLists.txt when
 * the MCUX_COMPONENT_middleware.wireless.framework.devmgr_mac_gen option is
 * selected. On this platform/CFPA backend it enables the interim CFPA
 * self-provisioning step rather than a per-call fallback inside
 * the resolution chain. */
#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
#include "fwk_devmgr_mac_gen.h"
#endif

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

#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
/* Internal sentinel: block forever when acquiring the DEVMGR IFR zone mutex
 * (mirrors DEVMGR_WAIT_FOREVER in fwk_platform_ifr.c; no public constant is
 * exposed by fwk_platform_ifr.h). */
#define DEVMGR_SELFPROV_WAIT_FOREVER 0xFFFFFFFFU

/*! *********************************************************************************
 * \brief  Interim CFPA self-provisioning of the BLE / IEEE 802.15.4 MAC addresses
 *      (MCSE-1750).
 *
 * \details KW43/MCXW70 does not yet factory-provision CFPA MAC addresses. This
 *      helper is the interim bridge: it reads both customer (CFPA) addresses
 *      directly and, for whichever one is missing (read failure or blank),
 *      generates a replacement via the MAC-generation service
 *      (DEVMGR_GenerateBdAddress uses BD_ADDR_OUI 00:60:37, DEVMGR_Generate154Address
 *      uses IEEE802_15_4_ADDR_OUI 44:D4:65.)
 *
 *      Both Set operations (when needed) are batched into a single CFPA zone
 *      open/commit so that at most one device reset is provoked, instead of one
 *      reset per address.
 *
 *      If both addresses are already valid, this is a no-op: no zone is opened,
 *      no flash write occurs and no reset is provoked.
 *
 * \note On the path where at least one address needs generation,
 *      PLATFORM_DevMgrCommitZone() triggers an intentional system reset and does
 *      not return.
 *
 * \return DEVMGR_Success_c if both addresses are already valid (no-op path),
 *      DEVMGR_Error_c if generation or the zone open/commit fails. Otherwise
 *      does not return (device reset on a successful commit).
 ********************************************************************************** */
static int s_DEVMGR_SelfProvisionMacAddresses(void)
{
    int     status = DEVMGR_Success_c;
    uint8_t bdMac[DEVMGR_BD_ADDR_SZ];
    uint8_t addr154Mac[DEVMGR_154_ADDR_SZ];
    bool    needBd  = false;
    bool    need154 = false;
    int     cfpa_st;

    do
    {
        /* Determine which address(es) are missing by reading the raw CFPA
         * fields directly (bypassing the NMPA/UID fallback used by the public
         * Get*AddressCust accessors): "unsuccessful" covers both a genuine read
         * failure and a blank (unprovisioned) field. */
        cfpa_st = PLATFORM_DevMgrGetCfpaCustBleMacAddr(bdMac, DEVMGR_BD_ADDR_SZ);
        if ((cfpa_st != DEVMGR_STATUS_OK) ||
            (FLib_MemCmpToVal((const void *)bdMac, 0xFFU, DEVMGR_BD_ADDR_SZ) != FALSE) ||
            (FLib_MemCmpToVal((const void *)bdMac, 0x00U, DEVMGR_BD_ADDR_SZ) != FALSE))
        {
            needBd = true;
        }

        cfpa_st = PLATFORM_DevMgrGetCfpaCust802154MacAddr(addr154Mac, DEVMGR_154_ADDR_SZ);
        if ((cfpa_st != DEVMGR_STATUS_OK) ||
            (FLib_MemCmpToVal((const void *)addr154Mac, 0xFFU, DEVMGR_154_ADDR_SZ) != FALSE) ||
            (FLib_MemCmpToVal((const void *)addr154Mac, 0x00U, DEVMGR_154_ADDR_SZ) != FALSE))
        {
            need154 = true;
        }

        /* Both already provisioned: nothing to do, no reset. */
        if (!needBd && !need154)
        {
            break;
        }

        /* Open the CFPA zone once so both fields (when both are missing) are
         * batched into a single flash commit, i.e. a single device reset. */
        if (PLATFORM_DevMgrOpenZone(kDevMgrZone_Cfpa, DEVMGR_SELFPROV_WAIT_FOREVER) != DEVMGR_STATUS_OK)
        {
            status = DEVMGR_Error_c;
            break;
        }

        if (needBd)
        {
            uint8_t  genAddr[DEVMGR_BD_ADDR_SZ];
            uint8_t  cfpaAddr[DEVMGR_BD_ADDR_SZ];
            uint32_t i;

            if (DEVMGR_GenerateBdAddress(genAddr) != DEVMGR_Success_c)
            {
                PLATFORM_DevMgrAbortZone();
                status = DEVMGR_Error_c;
                break;
            }
            /* Reverse to the CFPA (most-significant octet first) layout,
             * matching the convention used by PLATFORM_DEVMGR_SetBdAddress. */
            for (i = 0U; i < DEVMGR_BD_ADDR_SZ; i++)
            {
                cfpaAddr[i] = genAddr[DEVMGR_BD_ADDR_SZ - 1U - i];
            }
            /* Zone is open: this only updates the RAM overlay, no flash write yet. */
            (void)PLATFORM_DevMgrSetCfpaCustBleMacAddr(cfpaAddr, DEVMGR_BD_ADDR_SZ);
        }

        if (need154)
        {
            uint8_t  genAddr[DEVMGR_154_ADDR_SZ];
            uint8_t  cfpaAddr[DEVMGR_154_ADDR_SZ];
            uint32_t i;

            if (DEVMGR_Generate154Address(genAddr) != DEVMGR_Success_c)
            {
                PLATFORM_DevMgrAbortZone();
                status = DEVMGR_Error_c;
                break;
            }
            for (i = 0U; i < DEVMGR_154_ADDR_SZ; i++)
            {
                cfpaAddr[i] = genAddr[DEVMGR_154_ADDR_SZ - 1U - i];
            }
            /* Zone is open: this only updates the RAM overlay, no flash write yet. */
            (void)PLATFORM_DevMgrSetCfpaCust802154MacAddr(cfpaAddr, DEVMGR_154_ADDR_SZ);
        }

        /* Single batched commit for both fields: triggers exactly one device
         * reset on success (PLATFORM_DevMgrCommitZone() does not return). */
        status = s_CfpaStatusToDevMgr(PLATFORM_DevMgrCommitZone());
    } while (false);

    return status;
}
#endif /* DEVMGR_MAC_GEN_ENABLED */

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
#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
        /* Interim CFPA self-provisioning (MCSE-1750): if the customer BLE MAC
         * read is unsuccessful (read failure or blank), self-provision both
         * MAC addresses in a single batched CFPA commit. On success this does
         * not return (device reset); after reboot the CFPA field is populated
         * and this read succeeds normally. */
        if ((cfpa_st != DEVMGR_STATUS_OK) ||
            (FLib_MemCmpToVal((const void *)cfpaMac, 0xFFU, DEVMGR_BD_ADDR_SZ) != FALSE) ||
            (FLib_MemCmpToVal((const void *)cfpaMac, 0x00U, DEVMGR_BD_ADDR_SZ) != FALSE))
        {
            (void)s_DEVMGR_SelfProvisionMacAddresses();
        }
#endif
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

int PLATFORM_DEVMGR_GetBdAddress(uint8_t *pOutAddr, uint32_t *fromLocation)
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

int PLATFORM_DEVMGR_Get154Address(uint8_t *pOutAddr, uint32_t *fromLocation)
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
#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
        /* Interim CFPA self-provisioning (MCSE-1750): if the customer 802.15.4
         * MAC read is unsuccessful (read failure or blank), self-provision both
         * MAC addresses in a single batched CFPA commit. On success this does
         * not return (device reset); after reboot the CFPA field is populated
         * and this read succeeds normally. */
        if ((cfpa_st != DEVMGR_STATUS_OK) ||
            (FLib_MemCmpToVal((const void *)cfpaMac, 0xFFU, DEVMGR_154_ADDR_SZ) != FALSE) ||
            (FLib_MemCmpToVal((const void *)cfpaMac, 0x00U, DEVMGR_154_ADDR_SZ) != FALSE))
        {
            (void)s_DEVMGR_SelfProvisionMacAddresses();
        }
#endif
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
