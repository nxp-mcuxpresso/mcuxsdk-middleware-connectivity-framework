/**********************************************************************************
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/*
 * DevMgr backend-shared implementation.
 *
 * Holds the generic MAC-address source-precedence reader used by every DevMgr
 * storage backend (HWParameter, platform/CFPA). Each backend supplies its own
 * source table (devmgr_src_entry_t []) and calls DEVMGR_MacAddrReader() to walk
 * it; the reader itself is backend-agnostic.
 */

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */

#include <stdbool.h>
#include "FunctionLib.h"
#include "fwk_devmgr_common.h"

/**********************************************************************************
 *************************************************************************************
 * Public functions
 *************************************************************************************
 ********************************************************************************** */

int DEVMGR_MacAddrReader(
    uint8_t *pOutAddr, uint32_t sz, uint32_t *fromLocation, const devmgr_src_entry_t *tbl, uint32_t n)
{
    int      status = DEVMGR_Error_c;
    uint32_t srcMask;
    /* If the fromLocation pointer is NULL, it is a valid case, meaning the caller does not
     * care where the result comes from. Likewise, if it is not NULL but its value is 0, it
     * is treated as if any source is accepted (DEVMGR_MAC_SRC_ANY).
     */
    srcMask = ((fromLocation != NULL) && (*fromLocation != 0U)) ? *fromLocation : DEVMGR_MAC_SRC_ANY;
    /* Once srcMask is initialized, if fromLocation is not NULL, clear its value */
    if (fromLocation != NULL)
    {
        *fromLocation = 0U;
    }
    if (pOutAddr != NULL)
    {
        /* A valid RAM pointer is required to return the resulting octet string */
        bool_t found = FALSE;
        FLib_MemSet(pOutAddr, DEVMGR_BLANK_BYTE, sz);
        /* Try successively all MAC read methods for which the srcMask bit is set */
        for (uint32_t i = 0U; i < n; i++)
        {
            if ((srcMask & tbl[i].srcBit) == 0U)
            {
                /* Skip this source: not selected by srcMask */
                continue;
            }
            /* Invoke the source-specific getter for this table entry */
            status = tbl[i].get(pOutAddr);
            if (status == DEVMGR_Success_c)
            {
                /* In case of success, it is final, update fromLocation with the
                 * current source location mask, and exit loop */
                if (fromLocation != NULL)
                {
                    *fromLocation = tbl[i].srcBit;
                }
                /* Mark success independently of fromLocation: a NULL fromLocation
                 * is a valid caller choice and must not prevent 'found' from being
                 * set, otherwise the not-found fallback below would wrongly run. */
                found = TRUE;
                break;
            }
        } /* for */
        if (found == FALSE)
        {
            /* Nothing was found, report so */
            if (fromLocation != NULL)
            {
                /* none of the registered methods has succeeded */
                *fromLocation = 0U;
            }
        }
    }

    return status;
}
