/*!
 * Copyright 2021-2023, 2025-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**********************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ********************************************************************************** */
#include <assert.h>
#include <stdbool.h>
#ifndef __ZEPHYR__
#include "board_platform.h"
#include "pin_mux.h"
#include "fwk_devmgr.h"
#include "fwk_platform_devmgr.h"
#endif /* __ZEPHYR__ */

#include "fsl_common.h"
#include "fsl_adapter_rpmsg.h"
#include "fwk_platform_definitions.h"
#include "fwk_platform.h"
#include "fwk_platform_ics.h"
#include "fwk_platform_ot.h"
#include "FunctionLib.h"

/**********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */
/* Check if __st is negative,  if true, apply 4 bits shit and add new __error_code,
    assert in debug and break
   Shall be called in a do while(0) bracket */
#define CHECK_AND_RAISE_ERROR(__st, __error_code)                                                                 \
    {                                                                                                             \
        if ((__st) < 0)                                                                                           \
        {                                                                                                         \
            assert(false);                                                                                        \
            uint32_t _encoded = (((uint32_t)(-(__st)) << 4) | (uint32_t)(-(__error_code))) & (uint32_t)INT32_MAX; \
            (__st)            = -(int)_encoded;                                                                   \
            break;                                                                                                \
        }                                                                                                         \
    }

/* Raise error with status update , shift previous status by 4 bits and OR with new error code.
 * the returned status will be negative */
#define RAISE_ERROR(__st, __error_code) -(int)((uint32_t)(((uint32_t)(__st) << 4) | (uint32_t)(__error_code)));

/* -------------------------------------------------------------------------- */
/*                             Public functions                               */
/* -------------------------------------------------------------------------- */

int PLATFORM_InitOT(void)
{
    int status = 0;

    do
    {
        status = PLATFORM_InitTimerManager();
        CHECK_AND_RAISE_ERROR(status, -1);

#if USE_NBU
        /* Init NBU domain and configure RFMC module
         * CM3 is still in reset and will be released by MCMGR_StartCore during RPMSG init */
        status = PLATFORM_InitNbu();
        CHECK_AND_RAISE_ERROR(status, -2);

        /* Init of the multicore*/
        status = PLATFORM_InitMulticore();
        CHECK_AND_RAISE_ERROR(status, -3);

        /* Initialize PLatform Service intercore channel
         *  Used to retrieve NBU version information but not retricted to this sole use.
         */
        status = PLATFORM_FwkSrvInit();
        CHECK_AND_RAISE_ERROR(status, -5);
#endif

#if USE_NBU && !NBU_15_4_ONLY
        /* Send chip revision (A0 or A1) to NBU */
        status = PLATFORM_SendChipRevision();
        CHECK_AND_RAISE_ERROR(status, -6);
#endif

        /* Update 32MHz trim value with the one stored in HW parameters */
        PLATFORM_Xtal32MHzInit();

#if !defined(gBoardUseFro32k_d) || (gBoardUseFro32k_d == 0)
        /* Make sure OSC32k is ready and select it as clock source */
        status = PLATFORM_SwitchToOsc32k();
        CHECK_AND_RAISE_ERROR(status, -7);
#endif
    } while (false);

    return status;
}

/*!
 * \brief PLATFORM_GetIeee802_15_4Addr deprecated replaced by DEVMGR_Get154Address.
 *
 * \note kept for backward compatibility.
 *
 */
void PLATFORM_GetIeee802_15_4Addr(uint8_t *eui64_address)
{
    (void)DEVMGR_Get154Address(eui64_address, NULL);
}