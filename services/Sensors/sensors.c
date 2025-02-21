/*
 * Copyright 2021-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************************
 * Include
 ************************************************************************************/

#include "sensors.h"

#include "fwk_platform.h"
#include "fwk_platform_sensors.h"

#include "fsl_device_registers.h"
#include "fsl_os_abstraction.h"
#include "fsl_common.h"
#include "fwk_platform_ics.h"

/************************************************************************************
*************************************************************************************
* Private type definitions and macros
*************************************************************************************
************************************************************************************/
#if USE_RTOS && 1 // Currently only tested with Mutex enabled in Sensors
#define gSensorsUseMutex_c TRUE
#else
#define gSensorsUseMutex_c FALSE
#endif /*USE_RTOS*/

#if gSensorsUseMutex_c
#define ADC_MUTEX_LOCK()   (void)OSA_MutexLock((osa_mutex_handle_t)mADCMutexId, osaWaitForever_c)
#define ADC_MUTEX_UNLOCK() (void)OSA_MutexUnlock((osa_mutex_handle_t)mADCMutexId)
#else
#define ADC_MUTEX_LOCK()
#define ADC_MUTEX_UNLOCK()
#endif /* gSensorsUseMutex_c */

#if gSensorsUseMutex_c
static OSA_MUTEX_HANDLE_DEFINE(mADCMutexId);
#endif /* gSensorsUseMutex_c */

#define VALUE_NOT_AVAILABLE_8  0xFFu
#define VALUE_NOT_AVAILABLE_32 0xFFFFFFFFu

#ifndef gSensorsLpConstraint_c
#define gSensorsLpConstraint_c 0 /* Set lowpower constraint to WFI - See PWR_LowpowerMode_t from PWR.h */
#endif

#ifndef gSensorsAdcCalibrationDurationInMs_c
#define gSensorsAdcCalibrationDurationInMs_c 0U
#endif

typedef enum _temperature_measurement_s
{
    TMP_MEASUREMENT_IDLE,
    TMP_MEASUREMENT_ONGOING,
} temperature_measurement_s;

/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
*********************************************************************************** */

static uint8_t                            LastBatteryLevel   = VALUE_NOT_AVAILABLE_8;
static int32_t                            LastTemperature    = (int32_t)VALUE_NOT_AVAILABLE_32;
static volatile temperature_measurement_s temperature_status = TMP_MEASUREMENT_IDLE;

/*!
 * @brief pointer to Callback function structure used to allow / disallow lowpower during Sensors activity
 */
static const Sensors_LowpowerCriticalCBs_t *pfSensorsLowpowerCriticalCallbacks = NULL;

/*! *********************************************************************************
*************************************************************************************
* Private function
*************************************************************************************
*********************************************************************************** */

static int32_t Sensors_SetLpConstraint(int32_t power_mode)
{
    int32_t status = -1;
    if ((pfSensorsLowpowerCriticalCallbacks != NULL) &&
        (pfSensorsLowpowerCriticalCallbacks->SensorsEnterLowpowerCriticalFunc != NULL))
    {
        status = pfSensorsLowpowerCriticalCallbacks->SensorsEnterLowpowerCriticalFunc(power_mode);
    }
    return status;
}
static int32_t Sensors_ReleaseLpConstraint(int32_t power_mode)
{
    int32_t status = -1;
    ;
    if ((pfSensorsLowpowerCriticalCallbacks != NULL) &&
        (pfSensorsLowpowerCriticalCallbacks->SensorsExitLowpowerCriticalFunc != NULL))
    {
        status = pfSensorsLowpowerCriticalCallbacks->SensorsExitLowpowerCriticalFunc(power_mode);
    }
    return status;
}

static void Sensors_TemperatureReqCb(uint32_t temperature_meas_interval_ms)
{
    /* periodic measurement not supported yet */
    assert(temperature_meas_interval_ms == 0);

    SENSORS_TriggerTemperatureMeasurement();
}

static void Sensors_TemperatureReadyCb(int32_t temperature_value)
{
    (void)SENSORS_RefreshTemperatureValue();
}

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

/*!
 * @brief Init Sensors driver.
 *
 */
void SENSORS_Init(void)
{
    PLATFORM_InitAdc();

#if gSensorsUseMutex_c
    /*! Initialize the ADC Mutex here. */
    if (KOSA_StatusSuccess != OSA_MutexCreate((osa_mutex_handle_t)mADCMutexId))
    {
        assert(0);
    }
#endif
    PLATFORM_RegisterNbuTemperatureRequestEventCb(&Sensors_TemperatureReqCb);
    PLATFORM_RegisterTemperatureReadyEventCb(&Sensors_TemperatureReadyCb);
}

/*!
 * @brief Deinit ADC driver.
 *
 */
void SENSORS_Deinit(void)
{
    ADC_MUTEX_LOCK();
    PLATFORM_DeinitAdc();

#if gSensorsUseMutex_c
    /*! Destroy the ADC Mutex here. */
    if (KOSA_StatusSuccess != OSA_MutexDestroy((osa_mutex_handle_t)mADCMutexId))
    {
        assert(0);
    }
#endif
    PLATFORM_RegisterNbuTemperatureRequestEventCb(NULL);
    PLATFORM_RegisterTemperatureReadyEventCb(NULL);
    ADC_MUTEX_UNLOCK();
}

/*!
 * \brief  This function performs initialization of the callbacks structure used to
 *       disable lowpower when ADC is active
 *
 * \param[in]  pfCallback Pointer to the function structure used to allow/disable lowpower .
 *
 */
void Sensors_SetLowpowerCriticalCb(const Sensors_LowpowerCriticalCBs_t *pfCallback)
{
    pfSensorsLowpowerCriticalCallbacks = pfCallback;
}

/*!
 * @brief Trig the ADC on the temperature.
 *
 *
 */
void SENSORS_TriggerTemperatureMeasurement(void)
{
    if (temperature_status == TMP_MEASUREMENT_IDLE)
    {
        /* Lock the mutex now, it will be unlocked when the measure in SENSORS_RefreshTemperatureValue() */
        ADC_MUTEX_LOCK();
        (void)Sensors_SetLpConstraint(gSensorsLpConstraint_c);
        temperature_status = TMP_MEASUREMENT_ONGOING;
        if (false == PLATFORM_IsAdcInitialized())
        {
            PLATFORM_InitAdc();
            OSA_TimeDelay(gSensorsAdcCalibrationDurationInMs_c);
        }
        PLATFORM_StartTemperatureMonitor();
    }
}

/*!
 * @brief Refresh temperature value in RAM.
 *
 * @retval int32_t Temperature value
 */
int32_t SENSORS_RefreshTemperatureValue(void)
{
    int32_t temperature;

    /* We are using recursive mutexes so the mutex holder can lock several time the same mutex. This mutex is first
     * locked in SENSORS_TriggerTemperatureMeasurement(), locking it here a second time ensures the refresh is done by
     * the current mutex holder.
     * This mutex must be unlocked twice to be fully available to other tasks. */
    ADC_MUTEX_LOCK();

    if (temperature_status == TMP_MEASUREMENT_ONGOING)
    {
        PLATFORM_GetTemperatureValue(&temperature);
        temperature_status = TMP_MEASUREMENT_IDLE;
        /* This unlock corresponds to the lock done in SENSORS_TriggerTemperatureMeasurement() */
        ADC_MUTEX_UNLOCK();
        (void)Sensors_ReleaseLpConstraint(gSensorsLpConstraint_c);
        LastTemperature = temperature;
    }
    else
    {
        temperature = LastTemperature;
    }

    /* Unlock the mutex again because the mutex can be taken up to 2 times by the same holder */
    ADC_MUTEX_UNLOCK();
    return temperature;
}

/*!
 * @brief Get temperature value from RAM.
 *
 * @retval int32_t Temperature value
 */
int32_t SENSORS_GetTemperature(void)
{
    return LastTemperature;
}

/*!
 * @brief Trig the ADC on the battery.
 *
 *
 */
void SENSORS_TriggerBatteryMeasurement(void)
{
    ADC_MUTEX_LOCK();
    (void)Sensors_SetLpConstraint(gSensorsLpConstraint_c);
    if (false == PLATFORM_IsAdcInitialized())
    {
        PLATFORM_InitAdc();
        OSA_TimeDelay(gSensorsAdcCalibrationDurationInMs_c);
    }
    PLATFORM_StartBatteryMonitor();
}

/*!
 * @brief Refresh battery level in RAM.
 *
 * @retval uint8_t Battery level
 */
uint8_t SENSORS_RefreshBatteryLevel(void)
{
    uint8_t BatteryLevel;
    PLATFORM_GetBatteryLevel(&BatteryLevel);
    (void)Sensors_ReleaseLpConstraint(gSensorsLpConstraint_c);
    ADC_MUTEX_UNLOCK();

    LastBatteryLevel = BatteryLevel;
    return BatteryLevel;
}

/*!
 * @brief Get battery level from RAM.
 *
 * @retval int32_t Temperature value
 */
uint8_t SENSORS_GetBatteryLevel(void)
{
    assert(LastBatteryLevel != VALUE_NOT_AVAILABLE_8);
    return LastBatteryLevel;
}
