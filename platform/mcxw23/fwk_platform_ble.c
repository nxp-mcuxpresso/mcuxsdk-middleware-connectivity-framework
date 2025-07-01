/*!
 * Copyright 2024-2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file fwk_platform_ble.c
 *
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_platform_ble.h"
#include "ble_controller.h"
#include "fsl_rom_api.h"
#include "fsl_debug_console.h"

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int PLATFORM_InitBle(void)
{
    flash_config_t flashInstance;
    uint8_t        bdAddr[6];
    status_t       status;

    status = FLASH_Init(&flashInstance);
    assert_equal(status, kStatus_Success);

    if (status == kStatus_Success)
    {
        uint32_t location = kFFR_BdAddrLocationCmpa | kFFR_BdAddrLocationNmpa | kFFR_BdAddrLocationUuid;
        status            = FFR_GetBdAddress(&flashInstance, bdAddr, &location);
        assert_equal(status, kStatus_Success);
        PRINTF("Using bdAddr from %s\r\n", location == kFFR_BdAddrLocationCmpa ? "CMPA" :
                                           location == kFFR_BdAddrLocationNmpa ? "NMPA" :
                                                                                 "UUID");
    }

    if (status == kStatus_Success)
    {
        status = BLEController_WriteBdAddr(bdAddr);
        assert_equal(status, kBLEC_Success);
        PRINTF("bd_addr = ");
        for (int32_t i = 5; i >= 0; i--)
        {
            PRINTF("%s%x%s", bdAddr[i] < 0x10 ? "0" : "", bdAddr[i], i ? ":" : "");
        }
        PRINTF("\r\n");
    }

    return 0;
}

bool PLATFORM_CheckNextBleConnectivityActivity(void)
{
    return true;
}