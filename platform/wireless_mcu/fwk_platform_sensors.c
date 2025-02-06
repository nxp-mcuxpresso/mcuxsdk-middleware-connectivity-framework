/* -------------------------------------------------------------------------- */
/*                           Copyright 2021-2025 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fsl_common.h"
#include "fwk_platform_sensors.h"
#include "fwk_platform_ics.h"
#include "fwk_platform.h"
#include "board_utility.h"
#include "fwk_debug.h"
#include "fsl_lpadc.h"
#include "fsl_os_abstraction.h"

/* -------------------------------------------------------------------------- */
/*                              Exported functions                            */
/* -------------------------------------------------------------------------- */
extern void ADC0_IRQHandler(void);

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/*! Deviation from which the new temperature is sent to the CM3*/
#define PLATFORM_TEMP_SENT_CM3_THRESHOLD 10
#ifndef PLATFORM_ADC_IRQ_PRIO
#ifdef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define PLATFORM_ADC_IRQ_PRIO configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1
#else
#define PLATFORM_ADC_IRQ_PRIO 3
#endif /* USE_RTOS */
#endif /* PLATFORM_ADC_IRQ_PRIO */

static int32_t last_temperature_value = PLATFORM_SENSOR_UNKNOWN_TEMPERATURE;
static int32_t new_temperature_value  = PLATFORM_SENSOR_UNKNOWN_TEMPERATURE;

static struct
{
    volatile bool isTempMeasureOngoing;
    volatile bool isTempMeasureDone;
} temperature_req_status;

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/*!
 * Send temperature value to the CM3.
 */
static void PLATFORM_SendTemperatureValue(int32_t l_temperature_value);

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
void ADC0_IRQHandler(void)
{
    float float_temperature_value = BOARD_GetTemperature();

    if (float_temperature_value != -273.15f)
    {
        new_temperature_value = 10 * (int32_t)float_temperature_value;
    }

    temperature_req_status.isTempMeasureDone = true;

    SDK_ISR_EXIT_BARRIER;
}

bool PLATFORM_IsAdcInitialized(void)
{
    return BOARD_IsAdcInitialized();
}

void PLATFORM_InitAdc(void)
{
    BOARD_InitAdc();
    /* Temperature measurement is directed to FIFO1. Here we enable FIFO1
     * Watermark interrupt to be able to receive the temperature measurement in the ISR.
     * Battery measurement will be using ADC FIFO0. No interrupt is generated
     * in that case.
     */
    LPADC_EnableInterrupts(ADC0, (uint32_t)kLPADC_FIFO1WatermarkInterruptEnable);
    NVIC_SetPriority(ADC0_IRQn, PLATFORM_ADC_IRQ_PRIO);
    (void)EnableIRQ(ADC0_IRQn);
}

void PLATFORM_DeinitAdc(void)
{
    BOARD_DeinitAdc();
}

void PLATFORM_StartBatteryMonitor(void)
{
    BOARD_AdcSwTrigger(LPADC_BATTERY_MONITOR_CHANNEL);
}

void PLATFORM_GetBatteryLevel(uint8_t *battery_level)
{
    *battery_level = (uint8_t)BOARD_GetBatteryLevel();
}

void PLATFORM_StartTemperatureMonitor(void)
{
    BOARD_AdcSwTrigger(LPADC_TEMPEATURE_SENSOR_CHANNEL);
    temperature_req_status.isTempMeasureOngoing = true;
}

void PLATFORM_GetTemperatureValue(int32_t *temperature_value)
{
    do
    {
        /* Ensure to not be in an infinite while loop if nothing has been trigger */
        if (!temperature_req_status.isTempMeasureOngoing)
        {
            *temperature_value = new_temperature_value;
            break;
        }
        while (!temperature_req_status.isTempMeasureDone)
        {
            /* Wait until measurement is ready */
        }
        *temperature_value = new_temperature_value;
        /* Clear Status for futur use */
        temperature_req_status.isTempMeasureDone    = false;
        temperature_req_status.isTempMeasureOngoing = false;

        /* No need to send the temperature to the NBU if the core is not started */
        if (PLATFORM_IsNbuStarted() != 0)
        {
            if ((*temperature_value > (last_temperature_value + PLATFORM_TEMP_SENT_CM3_THRESHOLD)) ||
                (*temperature_value < (last_temperature_value - PLATFORM_TEMP_SENT_CM3_THRESHOLD)))
            {
                /* Send temperature to the NBU */
                PLATFORM_SendTemperatureValue(*temperature_value);
                last_temperature_value = *temperature_value;
            }
        }
    } while (false);
}

void PLATFORM_SaveAdcContext(void)
{
    BOARD_SaveAdcContext();
}

void PLATFORM_RestoreAdcContext(void)
{
    BOARD_RestoreAdcContext();
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
static void PLATFORM_SendTemperatureValue(int32_t l_temperature_value)
{
    int status;
    PWR_DBG_LOG("temperature sent:%d", l_temperature_value);
    status = PLATFORM_FwkSrvSendPacket(gFwkTemperatureIndication_c, (void *)&l_temperature_value,
                                       (uint16_t)sizeof(l_temperature_value));
    assert(status == 0);
    (void)status;
}
