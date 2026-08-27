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

#include "fwk_devmgr.h"
#include "fwk_devmgr_common.h"
#include "fwk_platform_devmgr.h"
#include "HWParameter.h"
#include "FunctionLib.h"
#include "fsl_common.h"
#include "fwk_platform_devmgr_types.h"

/* DEVMGR_MAC_GEN_ENABLED is defined (to 1) by the DevMgr CMakeLists.txt when the
 * MCUX_COMPONENT_middleware.wireless.framework.devmgr_mac_gen option is selected. */
#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
#include "fwk_devmgr_mac_gen.h"
#endif

/**********************************************************************************
 *************************************************************************************
 * Private functions prototypes
 *************************************************************************************
 ********************************************************************************** */

static int s_DEVMGR_GetCustBdAddress(uint8_t *pOutAddr);
static int s_DEVMGR_GetCust154Address(uint8_t *pOutAddr);

#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
static int s_DEVMGR_GenerateAndSaveBdAddress(uint8_t *pOutAddr);
static int s_DEVMGR_GenerateAndSave154Address(uint8_t *pOutAddr);

#endif
/**********************************************************************************
 *************************************************************************************
 * Private functions
 *************************************************************************************
 ********************************************************************************** */

static int s_DEVMGR_GetCustBdAddress(uint8_t *pOutAddr)
{
    int                   status;
    hardwareParameters_t *pHwParams = NULL;
    uint32_t              readStatus;

    /* Single-iteration do/while used only as a structured-exit ("break") construct. */
    do
    {
        uint32_t regPrimask;
        assert(pOutAddr != NULL); /* already checked by caller */

        /* NV_ReadHWParameters reads the hardwareParameters_t structure and verifies the checksum */
        readStatus = NV_ReadHWParameters(&pHwParams);

        /* A blank store is not an error here: it just means no value has been
         * programmed yet. Any other non-success code is a genuine read failure. */
        if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
        {
            status = DEVMGR_Error_c;
            break;
        }
        /* Read / check contents in critical section so the copy and the
         * blank-check observe a consistent snapshot of the HW parameters. */
        regPrimask = DisableGlobalIRQ();
        /* Always return the stored octets to the caller. */
        FLib_MemCpy(pOutAddr, pHwParams->bluetooth_address, DEVMGR_BD_ADDR_SZ);
        /* A non-blank field means a valid address is present (success); an
         * all-blank field means this source holds no value (DEVMGR_Blank_c). */
        if (FLib_MemCmpToVal((const void *)pHwParams->bluetooth_address, DEVMGR_BLANK_BYTE, DEVMGR_BD_ADDR_SZ) == FALSE)
        {
            status = DEVMGR_Success_c;
        }
        else
        {
            status = DEVMGR_Blank_c;
        }
        EnableGlobalIRQ(regPrimask);
    } while (false);

    return status;
}

static int s_DEVMGR_GetCust154Address(uint8_t *pOutAddr)
{
    int                   status;
    hardwareParameters_t *pHwParams = NULL;
    uint32_t              readStatus;

    /* Single-iteration do/while used only as a structured-exit ("break") construct. */
    do
    {
        uint32_t regPrimask;
        assert(pOutAddr != NULL); /* already checked by caller */

        /* NV_ReadHWParameters reads the hardwareParameters_t structure and verifies the checksum */
        readStatus = NV_ReadHWParameters(&pHwParams);

        /* A blank store is not an error here: it just means no value has been
         * programmed yet. Any other non-success code is a genuine read failure. */
        if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
        {
            status = DEVMGR_Error_c;
            break;
        }

        /* Read / check contents in critical section so the copy and the
         * blank-check observe a consistent snapshot of the HW parameters. */
        regPrimask = DisableGlobalIRQ();
        /* Always return the stored octets to the caller. */
        FLib_MemCpy(pOutAddr, pHwParams->ieee_802_15_4_address, DEVMGR_154_ADDR_SZ);
        /* A non-blank field means a valid address is present (success); an
         * all-blank field means this source holds no value (DEVMGR_Blank_c). */
        if (FLib_MemCmpToVal((const void *)pHwParams->ieee_802_15_4_address, DEVMGR_BLANK_BYTE, DEVMGR_154_ADDR_SZ) ==
            FALSE)
        {
            status = DEVMGR_Success_c;
        }
        else
        {
            status = DEVMGR_Blank_c;
        }
        EnableGlobalIRQ(regPrimask);
    } while (false);

    return status;
}

#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
static int s_DEVMGR_GenerateAndSaveBdAddress(uint8_t *pOutAddr)
{
    int status;

    /* Step 4: generation fallback (only when the generation service is built
     * in). The generated address is persisted into the customer store and
     * reported as DEVMGR_MAC_SRC_CUST, so it must run only when the caller
     * accepts the CUST source (i.e. the DEVMGR_MAC_SRC_CUST bit is set in
     * fromLocation). A persistence failure does not corrupt the returned
     * value. */
    status = DEVMGR_GenerateBdAddress(pOutAddr);
    if (status == DEVMGR_Success_c)
    {
        (void)DEVMGR_SetBdAddress(pOutAddr);
    }
    return status;
}

static int s_DEVMGR_GenerateAndSave154Address(uint8_t *pOutAddr)
{
    int status;

    /* Step 4: generation fallback (only when the generation service is built
     * in). The generated address is persisted into the customer store and
     * reported as DEVMGR_MAC_SRC_CUST, so it must run only when the caller
     * accepts the CUST source (i.e. the DEVMGR_MAC_SRC_CUST bit is set in
     * fromLocation). A persistence failure does not corrupt the returned
     * value. */
    status = DEVMGR_Generate154Address(pOutAddr);
    if (status == DEVMGR_Success_c)
    {
        (void)DEVMGR_Set154Address(pOutAddr);
    }
    return status;
}
#endif

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */
int DEVMGR_GetBdAddress(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    /* The order of the functions in the array define the precedence of the MAC address sources:
     * CUST (HWParameter/CFPA)->NMPA->UUID.
     * The 4th option to generate and write the MAC address depends on the DEVMGR_MAC_GEN_ENABLED
     * build option.
     */
    static const devmgr_src_entry_t tbl[] = {
        {DEVMGR_MAC_SRC_CUST, s_DEVMGR_GetCustBdAddress},
        {DEVMGR_MAC_SRC_NMPA, PLATFORM_DEVMGR_GetBdAddrFromNMPA},
        {DEVMGR_MAC_SRC_UUID, PLATFORM_DEVMGR_GetBdAddrFromUID},
#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
        {DEVMGR_MAC_SRC_CUST, s_DEVMGR_GenerateAndSaveBdAddress},
#endif
    };
    /* fromLocation is returned with the bit mask corresponding to the successful MAC retrieval / or 0 otherwise */
    return DEVMGR_MacAddrReader(pOutAddr, DEVMGR_BD_ADDR_SZ, fromLocation, tbl, ARRAY_SIZE(tbl));
}

int DEVMGR_SetBdAddress(const uint8_t *pAddr)
{
    int status = DEVMGR_Success_c;

    do
    {
        uint32_t              regPrimask;
        hardwareParameters_t *pHwParams = NULL;
        uint32_t              readStatus;

        if (pAddr == NULL)
        {
            /* pAddr argument must be a valid pointer */
            status = DEVMGR_Error_c;
            break;
        }
        /* Read current value to compare against value to be written.*/
        readStatus = NV_ReadHWParameters(&pHwParams);

        if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
        {
            /* Read failed */
            status = DEVMGR_Error_c;
            break;
        }

        regPrimask = DisableGlobalIRQ();
        if (FLib_MemCmp((const void *)pAddr, pHwParams->bluetooth_address, DEVMGR_BD_ADDR_SZ) == FALSE)
        {
            /* Value differs from current field contents: program the new value. */
            FLib_MemCpy(pHwParams->bluetooth_address, pAddr, DEVMGR_BD_ADDR_SZ);
            if (NV_WriteHWParameters() != gHWParameterSuccess_c)
            {
                status = DEVMGR_Error_c;
            }
        }

        EnableGlobalIRQ(regPrimask);

    } while (false);

    return status;
}

int DEVMGR_Get154Address(uint8_t *pOutAddr, uint32_t *fromLocation)
{
    /* The order of the functions in the array define the precedence of the MAC address sources:
     * CUST (HWParameter/CFPA)->NMPA->UUID.
     * The 4th option to generate and write the MAC address depends on the DEVMGR_MAC_GEN_ENABLED
     * build option.
     */
    static const devmgr_src_entry_t tbl[] = {
        {DEVMGR_MAC_SRC_CUST, s_DEVMGR_GetCust154Address},
        {DEVMGR_MAC_SRC_NMPA, PLATFORM_DEVMGR_Get154AddrFromNMPA},
        {DEVMGR_MAC_SRC_UUID, PLATFORM_DEVMGR_Get154AddrFromUID},
#if defined(DEVMGR_MAC_GEN_ENABLED) && (DEVMGR_MAC_GEN_ENABLED > 0)
        {DEVMGR_MAC_SRC_CUST, s_DEVMGR_GenerateAndSave154Address},
#endif
    };
    /* fromLocation is returned with the bit mask corresponding to the successful MAC retrieval / or 0 otherwise */
    return DEVMGR_MacAddrReader(pOutAddr, DEVMGR_154_ADDR_SZ, fromLocation, tbl, ARRAY_SIZE(tbl));
}

int DEVMGR_Set154Address(const uint8_t *pAddr)
{
    int status = DEVMGR_Success_c;

    do
    {
        uint32_t              regPrimask;
        hardwareParameters_t *pHwParams = NULL;
        uint32_t              readStatus;

        if (pAddr == NULL)
        {
            /* pAddr argument must be a valid pointer */
            status = DEVMGR_Error_c;
            break;
        }
        /* Read current value to compare against value to be written.*/
        readStatus = NV_ReadHWParameters(&pHwParams);

        if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
        {
            /* Read failed */
            status = DEVMGR_Error_c;
            break;
        }

        regPrimask = DisableGlobalIRQ();
        if (FLib_MemCmp((const void *)pAddr, pHwParams->ieee_802_15_4_address, DEVMGR_154_ADDR_SZ) == FALSE)
        {
            /* Value differs from current field contents: program the new value. */
            FLib_MemCpy(pHwParams->ieee_802_15_4_address, pAddr, DEVMGR_154_ADDR_SZ);
            if (NV_WriteHWParameters() != gHWParameterSuccess_c)
            {
                status = DEVMGR_Error_c;
            }
        }

        EnableGlobalIRQ(regPrimask);

    } while (false);

    return status;
}

int DEVMGR_Get32MHzTrim(uint8_t *pOutTrim)
{
    int                   status    = DEVMGR_Success_c;
    hardwareParameters_t *pHwParams = NULL;
    uint32_t              readStatus;

    if (pOutTrim == NULL)
    {
        /* Pointer argument cannot be NULL */
        status = DEVMGR_Error_c;
    }
    else
    {
        readStatus = NV_ReadHWParameters(&pHwParams);

        if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
        {
            /* neither a valid value nor a blank value is the sign of an error */
            status = DEVMGR_Error_c;
        }
        else
        {
            uint32_t regPrimask = DisableGlobalIRQ();
            *pOutTrim           = pHwParams->xtalTrim; /* if not blank, return DEVMGR_Success_c */
            if (pHwParams->xtalTrim == DEVMGR_BLANK_BYTE)
            {
                status = DEVMGR_Blank_c;
            }
            EnableGlobalIRQ(regPrimask);
        }
    }

    return status;
}

int DEVMGR_Set32MHzTrim(uint8_t trim)
{
    int                   status    = DEVMGR_Success_c;
    hardwareParameters_t *pHwParams = NULL;
    uint32_t              readStatus;

    /* Read current value to compare against value to be written.*/
    readStatus = NV_ReadHWParameters(&pHwParams);

    if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
    {
        /* read operation failed and content is not blank */
        status = DEVMGR_Error_c;
    }
    else
    {
        uint32_t regPrimask;

        regPrimask = DisableGlobalIRQ();
        if (trim != pHwParams->xtalTrim)
        {
            /* Value differs from current field contents: program the new value. */
            pHwParams->xtalTrim = trim;
            if (NV_WriteHWParameters() != gHWParameterSuccess_c)
            {
                status = DEVMGR_Error_c;
            }
        }
        EnableGlobalIRQ(regPrimask);
    }

    return status;
}

int DEVMGR_Get32kHzTrim(uint8_t *pOutCap)
{
    int                   status    = DEVMGR_Success_c;
    hardwareParameters_t *pHwParams = NULL;
    uint32_t              readStatus;

    if (pOutCap == NULL)
    {
        /* Pointer argument cannot be NULL */
        status = DEVMGR_Error_c;
    }
    else
    {
        readStatus = NV_ReadHWParameters(&pHwParams);

        if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
        {
            /* neither a valid value nor a blank value is the sign of an error */
            status = DEVMGR_Error_c;
        }
        else
        {
            uint32_t regPrimask = DisableGlobalIRQ();
            *pOutCap            = pHwParams->xtalCap32K; /* if not blank, return DEVMGR_Success_c */
            if (pHwParams->xtalCap32K == DEVMGR_BLANK_BYTE)
            {
                status = DEVMGR_Blank_c;
            }
            EnableGlobalIRQ(regPrimask);
        }
    }

    return status;
}

int DEVMGR_Set32kHzTrim(uint8_t cap)
{
    int                   status    = DEVMGR_Success_c;
    hardwareParameters_t *pHwParams = NULL;
    uint32_t              readStatus;

    /* Read current value to compare against value to be written.*/
    readStatus = NV_ReadHWParameters(&pHwParams);

    if ((readStatus != gHWParameterSuccess_c) && (readStatus != gHWParameterBlank_c))
    {
        /* read operation failed and content is not blank */
        status = DEVMGR_Error_c;
    }
    else
    {
        uint32_t regPrimask;

        regPrimask = DisableGlobalIRQ();
        if (cap != pHwParams->xtalCap32K)
        {
            /* Value differs from current field contents: program the new value. */
            pHwParams->xtalCap32K = cap;
            if (NV_WriteHWParameters() != gHWParameterSuccess_c)
            {
                status = DEVMGR_Error_c;
            }
        }
        EnableGlobalIRQ(regPrimask);
    }

    return status;
}
