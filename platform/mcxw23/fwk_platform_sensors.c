/* -------------------------------------------------------------------------- */
/*                           Copyright 2021-2025 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fsl_common.h"
#include "fwk_platform_sensors.h"

bool PLATFORM_IsAdcInitialized(void)
{
    return true;
}

void PLATFORM_InitAdc(void)
{
}

void PLATFORM_DeinitAdc(void)
{
}

void PLATFORM_StartBatteryMonitor(void)
{
}

void PLATFORM_GetBatteryLevel(uint8_t *battery_level)
{
    *battery_level = PLATFORM_SENSOR_UNKNOWN_BATTERY_LVL;
}

void PLATFORM_StartTemperatureMonitor(void)
{
}

void PLATFORM_GetTemperatureValue(int32_t *temperature_value)
{
    *temperature_value = (int32_t)PLATFORM_SENSOR_UNKNOWN_TEMPERATURE;
}

void PLATFORM_SaveAdcContext(void)
{
}

void PLATFORM_RestoreAdcContext(void)
{
}

void PLATFORM_RegisterTemperatureReadyEventCb(temp_ready_event_callback_t cb)
{
    (void)cb;
}
