/*!
 * Copyright 2021-2024 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file fwk_platform_sensors.c
 * \brief PLATFORM abstraction for Sensors service
 *
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fsl_common.h"
#include "fwk_platform_sensors.h"

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

bool PLATFORM_IsAdcInitialized(void)
{
    return true;
}

void PLATFORM_InitAdc(void)
{
    ;
}

void PLATFORM_DeinitAdc(void)
{
    ;
}

void PLATFORM_StartBatteryMonitor(void)
{
    ;
}

void PLATFORM_GetBatteryLevel(uint8_t *battery_level)
{
    *battery_level = 100U;
}

void PLATFORM_StartTemperatureMonitor(void)
{
    ;
}

void PLATFORM_GetTemperatureValue(int32_t *temperature_value)
{
    uint32_t temperature = 0U;

    temperature = (((SENSOR_CTRL->TSEN_CTRL_1_REG_2) & SENSOR_CTRL_TSEN_CTRL_1_REG_2_TSEN_TEMP_VALUE_MASK) >>
                   SENSOR_CTRL_TSEN_CTRL_1_REG_2_TSEN_TEMP_VALUE_SHIFT);

    if (temperature == 0U)
    {
        /* This is a workaround for A1 chips, the temperature sensor seems to return 0 on this revision
         * So we treat 0 as a wrong value, and simulate a value instead */
        *temperature_value = 21;
    }
    else
    {
        /* (temperature * 0.480561F - 220.7074F) */
        *temperature_value = (int32_t)((temperature * 481U) / 1000U - 221U);
    }
}
