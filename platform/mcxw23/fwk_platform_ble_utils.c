/*!
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file fwk_platform_ble_utils.c
 *
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdint.h>
#include <stdbool.h>

#include "fwk_platform_ble.h"
#include "fsl_rom_api.h"
#include "fsl_debug_console.h"

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void PLATFORM_GetBDAddr(uint8_t *bleDeviceAddress)
{
    flash_config_t flashInstance;
    status_t       status;

    status = FLASH_Init(&flashInstance);
    assert_equal(status, kStatus_Success);

    if (status == kStatus_Success)
    {
        uint32_t location = kFFR_BdAddrLocationCmpa | kFFR_BdAddrLocationNmpa | kFFR_BdAddrLocationUuid;
        status            = FFR_GetBdAddress(&flashInstance, bleDeviceAddress, &location);
        assert_equal(status, kStatus_Success);
        PRINTF("Using bdAddr from %s\r\n", location == kFFR_BdAddrLocationCmpa ? "CMPA" :
                                           location == kFFR_BdAddrLocationNmpa ? "NMPA" :
                                                                                 "UUID");
    }
}

bool PLATFORM_CheckNextBleConnectivityActivity(void)
{
    return true;
}
