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

/*! *********************************************************************************
 * \brief  Read the Bluetooth device address from the customer (CFPA/HWParameter)
 *      store.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 *
 * \note dummy implementation, actual access is done via HwParameter
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform has no customer store,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_GetBdAddressCust(uint8_t *pOutAddr)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)pOutAddr;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Program the Bluetooth device address in the platform store.
 *
 * \param[in] pAddr  buffer of DEVMGR_BD_ADDR_SZ bytes containing the address
 *
 * \note dummy implementation, actual access is done via HwParameter
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      store is read-only, DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)pAddr;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Read the IEEE 802.15.4 device address from the platform store.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_154_ADDR_SZ bytes receiving the address
 *
 * \note dummy implementation, actual access is done via HwParameter
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform has no such store,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get154AddressCust(uint8_t *pOutAddr)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)pOutAddr;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Program the IEEE 802.15.4 device address in the platform store.
 *
 * \param[in] pAddr  buffer of DEVMGR_154_ADDR_SZ bytes containing the address
 *
 * \note dummy implementation, actual access is done via HwParameter
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      store is read-only, DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Set154Address(const uint8_t *pAddr)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)pAddr;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Read the 32 MHz crystal trim value from the platform store.
 *
 * \param[out] pOutTrim  receives the 32 MHz crystal trim value
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform carries no 32 MHz trim,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get32MHzTrim(uint8_t *pOutTrim)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)pOutTrim;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Program the 32 MHz crystal trim value in the platform store.
 *
 * \param[in] trim  32 MHz crystal trim value to store
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      carries no 32 MHz trim, DEVMGR_Error_c on a backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Set32MHzTrim(uint8_t trim)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)trim;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Read the 32 kHz crystal load-capacitance trim from the platform store.
 *
 * \param[out] pOutCap  receives the 32 kHz crystal load-capacitance value
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Blank_c if the field is blank,
 *      DEVMGR_NotSupported_c if the platform carries no 32 kHz trim,
 *      DEVMGR_Error_c on a NULL pointer or backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Get32kHzTrim(uint8_t *pOutCap)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)pOutCap;
    return DEVMGR_NotSupported_c;
}

/*! *********************************************************************************
 * \brief  Program the 32 kHz crystal load-capacitance trim in the platform store.
 *
 * \param[in] cap  32 kHz crystal load-capacitance value to store
 *
 * \return DEVMGR_Success_c on success, DEVMGR_NotSupported_c if the platform
 *      carries no 32 kHz trim, DEVMGR_Error_c on a backend failure.
 ********************************************************************************** */
int PLATFORM_DEVMGR_Set32kHzTrim(uint8_t cap)
{
    /* MISRA rule 8.6 avoidance: dummy implementation, actual access is done via HwParameter */
    (void)cap;
    return DEVMGR_NotSupported_c;
}
