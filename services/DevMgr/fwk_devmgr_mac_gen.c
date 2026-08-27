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

#include "fwk_devmgr_mac_gen.h"
#include "FunctionLib.h"
#include "RNG_Interface.h"

/**********************************************************************************
 *************************************************************************************
 * Private macros
 *************************************************************************************
 ********************************************************************************** */

/* The OUI defaults and the MAC-generation size macros (DEVMGR_MAC_GEN_OUI_SZ,
 * DEVMGR_MAC_GEN_BD_RAND_SZ, DEVMGR_MAC_GEN_154_RAND_SZ) are shared with the
 * per-platform layer and provided by fwk_platform_devmgr.h (via
 * fwk_devmgr_mac_gen.h). */

/**********************************************************************************
 *************************************************************************************
 * Private memory declarations
 *************************************************************************************
 ********************************************************************************** */

/* Fixed OUI (3 most-significant bytes) used for each generated address type. */
static const uint8_t s_bdAddrOui[DEVMGR_MAC_GEN_OUI_SZ]  = {BD_ADDR_OUI};
static const uint8_t s_154AddrOui[DEVMGR_MAC_GEN_OUI_SZ] = {IEEE802_15_4_ADDR_OUI};

/**********************************************************************************
 *************************************************************************************
 * Private functions prototypes
 *************************************************************************************
 ********************************************************************************** */

static int s_DEVMGR_GetRandomBytes(uint8_t *pBuffer, uint8_t length);

/**********************************************************************************
 *************************************************************************************
 * Private functions
 *************************************************************************************
 ********************************************************************************** */

/*! *********************************************************************************
 * \brief  Fill a buffer with pseudo-random bytes from the RNG service.
 *
 * \param[out] pBuffer  destination buffer
 * \param[in]  length   number of random bytes to produce
 *
 * \return DEVMGR_Success_c on success, DEVMGR_Error_c on RNG failure.
 ********************************************************************************** */
static int s_DEVMGR_GetRandomBytes(uint8_t *pBuffer, uint8_t length)
{
    int status = DEVMGR_Success_c;
    int ret;

    ret = RNG_Init();
    if (ret != 0)
    {
        status = DEVMGR_Error_c;
    }
    else
    {
        ret = RNG_GetPseudoRandomData(pBuffer, length, NULL);
        if (ret < 0)
        {
            status = DEVMGR_Error_c;
        }
    }

    return status;
}

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */

int DEVMGR_GenerateBdAddress(uint8_t *pOutAddr)
{
    int status;

    do
    {
        if (pOutAddr == NULL)
        {
            status = DEVMGR_Error_c;
            break;
        }

        /* Fill the least-significant bytes with random data. */
        status = s_DEVMGR_GetRandomBytes(pOutAddr, DEVMGR_MAC_GEN_BD_RAND_SZ);
        if (status != DEVMGR_Success_c)
        {
            break;
        }

        /* Copy the OUI to the most-significant bytes. */
        FLib_MemCpy(&pOutAddr[DEVMGR_MAC_GEN_BD_RAND_SZ], s_bdAddrOui, DEVMGR_MAC_GEN_OUI_SZ);

        status = DEVMGR_Success_c;

    } while (false);

    return status;
}

int DEVMGR_Generate154Address(uint8_t *pOutAddr)
{
    int status;

    do
    {
        if (pOutAddr == NULL)
        {
            status = DEVMGR_Error_c;
            break;
        }

        /* Fill the least-significant bytes with random data. */
        status = s_DEVMGR_GetRandomBytes(pOutAddr, DEVMGR_MAC_GEN_154_RAND_SZ);
        if (status != DEVMGR_Success_c)
        {
            break;
        }

        /* Copy the OUI to the most-significant bytes. */
        FLib_MemCpy(&pOutAddr[DEVMGR_MAC_GEN_154_RAND_SZ], s_154AddrOui, DEVMGR_MAC_GEN_OUI_SZ);

        status = DEVMGR_Success_c;

    } while (false);

    return status;
}
