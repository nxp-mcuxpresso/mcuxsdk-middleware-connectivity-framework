/* -------------------------------------------------------------------------- */
/*                           Copyright 2021-2025 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fsl_common.h"
#include "fwk_platform_sensors.h"

#if !defined(FPGA_TARGET) || (FPGA_TARGET == 0)
#include "fwk_platform_ics.h"
#include "fwk_platform.h"
#include "board_utility.h"
#include "fwk_debug.h"
#include "fsl_lpadc.h"
#include "fsl_os_abstraction.h"
#include "fwk_workq.h"
#include "fwk_config.h"

/* -------------------------------------------------------------------------- */
/*                              Exported functions                            */
/* -------------------------------------------------------------------------- */
extern void ADC0_IRQHandler(void);

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/*!
 * Send temperature value to the NBU.
 */
static void PLATFORM_SendTemperatureValue(int32_t l_temperature_value);
static void PLATFORM_TemperatureReadyWorkHandler(fwk_work_t *work);

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
typedef enum _temperature_req_status
{
    TEMPERATURE_REQ_IDLE,
    TEMPERATURE_REQ_ONGOING,
} temperature_req_status;

/*! Deviation from which the new temperature is sent to the CM3*/
#ifndef PLATFORM_TEMP_SENT_NBU_THRESHOLD
/* Value in tenths of degree Celsius */
#define PLATFORM_TEMP_SENT_NBU_THRESHOLD 10
#endif

#ifndef PLATFORM_ADC_IRQ_PRIO
#ifdef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define PLATFORM_ADC_IRQ_PRIO configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1
#else
#define PLATFORM_ADC_IRQ_PRIO 3
#endif /* USE_RTOS */
#endif /* PLATFORM_ADC_IRQ_PRIO */

static int32_t          last_sent_temperature_value = PLATFORM_SENSOR_UNKNOWN_TEMPERATURE;
static volatile int32_t new_temperature_value       = PLATFORM_SENSOR_UNKNOWN_TEMPERATURE;

static fwk_work_t temperature_ready_work = {
    .handler = PLATFORM_TemperatureReadyWorkHandler,
};

static temp_ready_event_callback_t temperature_ready_callback = (temp_ready_event_callback_t)NULL;

static volatile temperature_req_status temperature_status = TEMPERATURE_REQ_IDLE;
/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
void ADC0_IRQHandler(void)
{
    float float_temperature_value = BOARD_GetTemperature();

    if (float_temperature_value != -273.15f)
    {
        new_temperature_value = 10 * (int32_t)float_temperature_value;

        if (WORKQ_Submit(&temperature_ready_work) < 0)
        {
            assert(0);
        }
    }

    temperature_status = TEMPERATURE_REQ_IDLE;

    SDK_ISR_EXIT_BARRIER;
}

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

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

    /* ADC ISR will be using a work to execute registered
     * cb and send the temperature measurement to the NBU
     */
    if (WORKQ_InitSysWorkQ() < 0)
    {
        assert(0);
    }

    LPADC_EnableInterrupts(ADC0, (uint32_t)kLPADC_FIFO1WatermarkInterruptEnable);
    NVIC_SetPriority(ADC0_IRQn, PLATFORM_ADC_IRQ_PRIO);
    (void)EnableIRQ(ADC0_IRQn);
}

void PLATFORM_DeinitAdc(void)
{
    BOARD_DeinitAdc();
}

void PLATFORM_ReinitAdc(void)
{
    BOARD_ReinitAdc();

    LPADC_EnableInterrupts(ADC0, (uint32_t)kLPADC_FIFO1WatermarkInterruptEnable);
    NVIC_SetPriority(ADC0_IRQn, PLATFORM_ADC_IRQ_PRIO);
    (void)EnableIRQ(ADC0_IRQn);
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
    temperature_status = TEMPERATURE_REQ_ONGOING;
    BOARD_AdcSwTrigger(LPADC_TEMPEATURE_SENSOR_CHANNEL);
}

void PLATFORM_GetTemperatureValue(int32_t *temperature_value)
{
    while (temperature_status == TEMPERATURE_REQ_ONGOING)
    {
        /* Wait until measurement is ready */
    }
    *temperature_value = new_temperature_value;
}

void PLATFORM_RegisterTemperatureReadyEventCb(temp_ready_event_callback_t cb)
{
#if defined(gPlatformIcsUseWorkqueueRxProcessing_d) && (gPlatformIcsUseWorkqueueRxProcessing_d > 0)
    temperature_ready_callback = cb;
#else
    /* Temperature ready event is only supported when gPlatformIcsUseWorkqueueRxProcessing_d is enabled */
    cb = NULL;
#endif
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

static void PLATFORM_TemperatureReadyWorkHandler(fwk_work_t *work)
{
    (void)work;

    int32_t temperature_to_send;
    int16_t xtal_cal_temp;

    if (temperature_ready_callback != NULL)
    {
        temperature_ready_callback(new_temperature_value);
    }

    /* new_temperature_value is in tenth of degrees C, converting to degrees C */
    xtal_cal_temp = (int16_t)(new_temperature_value / 10);

    /* When a new temperature is available, attempt to calibrate the XTAL32M.
     * The calibration will be effective if the temperature compensation LUT
     * was registered previously with PLATFORM_RegisterXtal32MTempCompLut().
     * If not, the calibration will be ignored. */
    if (PLATFORM_CalibrateXtal32M(xtal_cal_temp) < 0)
    {
        assert(0);
    }

    temperature_to_send = new_temperature_value;
    /* No need to send the temperature to the NBU if the core is not started */
    if (PLATFORM_IsNbuStarted() != 0)
    {
        if ((temperature_to_send > (last_sent_temperature_value + PLATFORM_TEMP_SENT_NBU_THRESHOLD)) ||
            (temperature_to_send < (last_sent_temperature_value - PLATFORM_TEMP_SENT_NBU_THRESHOLD)))
        {
            /* Send temperature to the NBU */
            PLATFORM_SendTemperatureValue(temperature_to_send);
            last_sent_temperature_value = temperature_to_send;
        }
    }
}
#else

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

void PLATFORM_ReinitAdc(void)
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

void PLATFORM_RegisterTemperatureReadyEventCb(temp_ready_event_callback_t cb)
{
    (void)cb;
}

#endif
