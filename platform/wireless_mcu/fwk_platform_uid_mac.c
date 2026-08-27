/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/**********************************************************************************
 *************************************************************************************
 * Overview
 *************************************************************************************
 *
 * Shared UID-derived MAC composition for the wireless_mcu platform family
 * (KW43/MCXW70, KW45/K32W1/MCXW71, KW47/MCXW72).
 *
 * This file regroups the fixed OUI tables (s_bdAddrOui, s_154AddrOui), the
 * CRC-32 configuration (crcConfig) and the deterministic UID-to-address
 * composition functions (PLATFORM_DEVMGR_GetBdAddrFromUID and
 * PLATFORM_DEVMGR_Get154AddrFromUID).
 *
 * The generation OUI defaults (BD_ADDR_OUI / IEEE802_15_4_ADDR_OUI), the
 * MAC-generation size macros (DEVMGR_MAC_GEN_*), DEVMGR_UID_SZ and the shared
 * DevMgr vocabulary (devmgr status codes, address sizes) are provided by the
 * platform layer headers fwk_platform_devmgr.h / fwk_platform_devmgr_types.h.
 * This source depends only on the platform layer and does not include the
 * DevMgr service header (fwk_devmgr.h).
 *
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
#include "fsl_adapter_crc.h"

/**********************************************************************************
 *************************************************************************************
 * Macros
 *************************************************************************************
 ********************************************************************************** */
/* offset 12 in UID  is a sweet spot where variations occur from device to device */
#define DEVMGR_UID_154_SEED_OFFSET (DEVMGR_UID_SZ - 4U)

/**********************************************************************************
 *************************************************************************************
 * Private memory declarations
 *************************************************************************************
 ********************************************************************************** */

/* Fixed OUI (3 most-significant bytes) used for each generated address type. */
static const uint8_t s_bdAddrOui[DEVMGR_MAC_GEN_OUI_SZ]  = {BD_ADDR_OUI};
static const uint8_t s_154AddrOui[DEVMGR_MAC_GEN_OUI_SZ] = {IEEE802_15_4_ADDR_OUI};

static const hal_crc_config_t s_crcConfig = {
    .crcRefIn           = KHAL_CrcRefInput,
    .crcRefOut          = KHAL_CrcRefOutput,
    .crcByteOrder       = KHAL_CrcLSByteFirst,
    .crcSeed            = 0xFFFFFFFFU,
    .crcPoly            = (uint32_t)KHAL_CrcPolynomial_CRC_32,
    .crcXorOut          = 0xFFFFFFFFU,
    .complementChecksum = 0U, /* false - disabled */
    .crcSize            = 4U,
    .crcStartByte       = 0U,
};

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */

/* UID-derived BD address composition  */
int PLATFORM_DEVMGR_GetBdAddrFromUID(uint8_t *pOutAddr)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t  uid_string[DEVMGR_UID_SZ];
        uint8_t  uid_len = 0U;
        uint32_t crc;

        if (pOutAddr == NULL)
        {
            break;
        }

        FLib_MemSet(pOutAddr, 0xFFU, DEVMGR_BD_ADDR_SZ);

        Chip_GetUID(uid_string, &uid_len);

        if (uid_len != DEVMGR_UID_SZ)
        {
            status = DEVMGR_Error_c;
            break;
        }

        if (FLib_MemCmpToVal((const void *)uid_string, 0xFFU, DEVMGR_UID_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }
        /* HAL_CrcCompute config argument should be const but the function does
         * not modify the structure, so no real problem */
        /* coverity[MISRA_C_2012_RULE_11.8:FALSE] */
        crc = HAL_CrcCompute((hal_crc_config_t *)&s_crcConfig, uid_string, DEVMGR_UID_SZ);
        /* Fill the device-specific (least-significant) random bytes from the CRC.
         */
        for (uint32_t i = 0U; i < DEVMGR_MAC_GEN_BD_RAND_SZ; i++)
        {
            pOutAddr[i] = (uint8_t)(crc & 0xFFU);
            crc >>= DEVMGR_BITS_PER_BYTE;
        }
        /* Copy the OUI to the most-significant bytes. */
        FLib_MemCpy(&pOutAddr[DEVMGR_MAC_GEN_BD_RAND_SZ], s_bdAddrOui, DEVMGR_MAC_GEN_OUI_SZ);

        status = DEVMGR_Success_c;

    } while (false);

    return status;
}

/* UID-derived IEEE-802.15.4 address composition  */
int PLATFORM_DEVMGR_Get154AddrFromUID(uint8_t *pOutAddr)
{
    int status = DEVMGR_Error_c;

    do
    {
        uint8_t  uid_string[DEVMGR_UID_SZ];
        uint8_t  uid_len = 0U;
        uint32_t crc;

        if (pOutAddr == NULL)
        {
            break;
        }

        FLib_MemSet(pOutAddr, 0xFFU, DEVMGR_154_ADDR_SZ);

        Chip_GetUID(uid_string, &uid_len);

        if (uid_len != DEVMGR_UID_SZ)
        {
            status = DEVMGR_Error_c;
            break;
        }

        if (FLib_MemCmpToVal((const void *)uid_string, 0xFFU, DEVMGR_UID_SZ) == TRUE)
        {
            status = DEVMGR_Blank_c;
            break;
        }
        /* HAL_CrcCompute config argument should be const but the function does
         * not modify the structure, so no real problem */
        /* coverity[MISRA_C_2012_RULE_11.8:FALSE] */
        crc = HAL_CrcCompute((hal_crc_config_t *)&s_crcConfig, uid_string, DEVMGR_UID_SZ);
        /* Fill the device-specific (random) bytes from the CRC, leaving the last
         * random byte to be taken from the UID below. */
        for (uint32_t i = 0U; i < (DEVMGR_MAC_GEN_154_RAND_SZ - 1U); i++)
        {
            pOutAddr[i] = (uint8_t)(crc & 0xFFU);
            crc >>= DEVMGR_BITS_PER_BYTE;
        }
        /* pick the missing byte from byte 12 of UUID */
        pOutAddr[DEVMGR_MAC_GEN_154_RAND_SZ - 1U] = uid_string[DEVMGR_UID_154_SEED_OFFSET];

        /* Copy the OUI to the most-significant bytes. */
        FLib_MemCpy(&pOutAddr[DEVMGR_MAC_GEN_154_RAND_SZ], s_154AddrOui, DEVMGR_MAC_GEN_OUI_SZ);

        status = DEVMGR_Success_c;

    } while (false);

    return status;
}
