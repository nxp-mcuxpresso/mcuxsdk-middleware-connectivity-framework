/* -------------------------------------------------------------------------- */
/*                           Copyright 2025 NXP                               */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fsl_common.h"
#include "fwk_platform_sensors.h"

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
static temp_ready_event_callback_t temperature_ready_callback = (temp_ready_event_callback_t)NULL;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

bool PLATFORM_IsAdcInitialized(void)
{
    /* Not implemented because no ADC available on MCXW23 */
    return true;
}

void PLATFORM_InitAdc(void)
{
    /* Not implemented because no ADC available on MCXW23 */
}

void PLATFORM_DeinitAdc(void)
{
    /* Not implemented because no ADC available on MCXW23 */
}

void PLATFORM_ReinitAdc(void)
{
    /* Not implemented because no ADC available on MCXW23 */
}

void PLATFORM_StartBatteryMonitor(void)
{
    /* Not implemented because no ADC available on MCXW23 */
}

void PLATFORM_GetBatteryLevel(uint8_t *battery_level)
{
    *battery_level = PLATFORM_SENSOR_UNKNOWN_BATTERY_LVL;
}

void PLATFORM_StartTemperatureMonitor(void)
{
    /* Not implemented because no ADC available on MCXW23 */
    if (temperature_ready_callback != NULL)
    {
        temperature_ready_callback();
    }
}

void PLATFORM_GetTemperatureValue(int32_t *temperature_value)
{
    /* Dummy Value (25°C)- Real measurement not implemented yet */
    *temperature_value = 250; /* value in tenths of degree */
}

void PLATFORM_RegisterTemperatureReadyEventCb(temp_ready_event_callback_t cb)
{
    temperature_ready_callback = cb;
}
