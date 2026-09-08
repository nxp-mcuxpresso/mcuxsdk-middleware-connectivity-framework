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
#include "fwk_devmgr_common.h"
#include "FunctionLib.h"
#include "fsl_rom_api.h"
#include "fsl_debug_console.h"

/**********************************************************************************
 *************************************************************************************
 * Private variables
 *************************************************************************************
 ********************************************************************************** */
static flash_config_t s_flashInstance;
static bool           s_flash_init_done = false;

/**********************************************************************************
 *************************************************************************************
 * Private functions
 *************************************************************************************
 *
 * MCXW23 reads a factory-provisioned BD address through the ROM FFR path. The
 * three FFR locations map to the three explicit resolution steps sequenced by
 * the platform/CFPA backend wrapper DEVMGR_GetBdAddress:
 *
 *   customer store -> kFFR_BdAddrLocationCmpa  (PLATFORM_DEVMGR_GetBdAddressCust)
 *   NMPA / IFR      -> kFFR_BdAddrLocationNmpa  (PLATFORM_DEVMGR_GetBdAddrFromNMPA)
 *   UID-derived     -> kFFR_BdAddrLocationUuid  (PLATFORM_DEVMGR_GetBdAddrFromUID)
 *
 * MCXW23 carries no IEEE 802.15.4 MAC address, so all 802.15.4 reads report
 * DEVMGR_NotSupported_c.
 *
 ********************************************************************************** */

/*! *********************************************************************************
 * \brief  Read the BD address from a single FFR location.
 *
 * \param[out] pOutAddr  buffer of DEVMGR_BD_ADDR_SZ bytes receiving the address
 * \param[in]  location  one of the kFFR_BdAddrLocation* single-location values
 *
 * \return DEVMGR_Success_c when the location holds a non-blank address,
 *      DEVMGR_Blank_c when it is unprovisioned/absent, DEVMGR_Error_c on a NULL
 *      pointer or flash-init failure.
 ********************************************************************************** */
static int s_PLATFORM_DEVMGR_GetBdAddrFromLocation(uint8_t *pOutAddr, ffr_bd_addr_location_t location)
{
    int status = DEVMGR_Error_c;

    do
    {
        status_t flashStatus;
        uint32_t searchLocation = location;

        if (pOutAddr == NULL)
        {
            break;
        }

        FLib_MemSet(pOutAddr, 0xFFU, DEVMGR_BD_ADDR_SZ);
        if (!s_flash_init_done)
        {
            flashStatus = FLASH_Init(&s_flashInstance);
            if (flashStatus != kStatus_Success)
            {
                break;
            }
            s_flash_init_done = true;
        }

        flashStatus = FFR_GetBdAddress(&s_flashInstance, pOutAddr, &searchLocation);
        if (flashStatus != kStatus_Success)
        {
            /* No address available at this location.
               FFR_GetBdAddress may legitimately return non-Success when address is absent */
            status = DEVMGR_Blank_c;
            break;
        }

        /* An all-0xFF address means the field is unprovisioned (blank). */
        if (FLib_MemCmpToVal((const void *)pOutAddr, 0xFFU, DEVMGR_BD_ADDR_SZ) != FALSE)
        {
            status = DEVMGR_Blank_c;
            break;
        }
        PRINTF("Using bdAddr from %s\r\n", location == kFFR_BdAddrLocationCmpa ? "CMPA" :
                                           location == kFFR_BdAddrLocationNmpa ? "NMPA" :
                                                                                 "UUID");

        status = DEVMGR_Success_c;
    } while (false);

    return status;
}

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */
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

int PLATFORM_DEVMGR_GetBdAddressCust(uint8_t *pOutAddr)
{
    /* Customer store on MCXW23 is the CMPA FFR location. */
    return s_PLATFORM_DEVMGR_GetBdAddrFromLocation(pOutAddr, kFFR_BdAddrLocationCmpa);
}

int PLATFORM_DEVMGR_GetBdAddrFromNMPA(uint8_t *pOutAddr)
{
    return s_PLATFORM_DEVMGR_GetBdAddrFromLocation(pOutAddr, kFFR_BdAddrLocationNmpa);
}

int PLATFORM_DEVMGR_GetBdAddrFromUID(uint8_t *pOutAddr)
{
    return s_PLATFORM_DEVMGR_GetBdAddrFromLocation(pOutAddr, kFFR_BdAddrLocationUuid);
}

int PLATFORM_DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    (void)pAddr;
    /* The FFR BD address region is read-only from the DevMgr platform layer. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get154Address(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    (void)pOutAddr;
    (void)fromLocation;
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Set154Address(const uint8_t *pAddr)
{
    (void)pAddr;
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get32MHzTrim(uint8_t *pOutTrim)
{
    (void)pOutTrim;
    /* MCXW23 carries no 32 MHz crystal trim in the platform store. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Set32MHzTrim(uint8_t trim)
{
    (void)trim;
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Get32kHzTrim(uint8_t *pOutCap)
{
    (void)pOutCap;
    /* MCXW23 carries no 32 kHz crystal trim in the platform store. */
    return DEVMGR_NotSupported_c;
}

int PLATFORM_DEVMGR_Set32kHzTrim(uint8_t cap)
{
    (void)cap;
    return DEVMGR_NotSupported_c;
}
