# Sensors

## Overview

The Sensors module provides an API to communicate with the ADC. Two values can be obtained by this module :
- Temperature value
- Battery level

The temperature is given in tenths of degrees Celsius and the battery in percentage.

This module is multi-caller, the ADC is protected by a mutex on the resource and by prevententing lowpower (only WFI) during its processing. Platform specific code can be find in fwk_platform_sensors.c/h.

## Features

### Manual Measurement
Calling "SENSORS_GetTemperature()" or "SENSORS_GetBatteryLevel()" function will give the last measured value that was copied to RAM. If you want to refresh this value you must first call the trigger function (SENSORS_TriggerTemperatureMeasurement() / SENSORS_TriggerBatteryMeasurement()) that configures the ADC to perform temperature or battery measurement, then a call to the refresh function (SENSORS_RefreshTemperatureValue() / SENSORS_RefreshBatteryLevel()) will read the returned value from the ADC and save it in RAM. You can run code between these two functions because the ADC won't give you the value until it's fully ready.

> **Important**: When a measurement is ongoing, new measurement requests will be **dropped**. Measurement triggers are only accepted when the measurement state is **IDLE**. This applies to both temperature and battery measurements to prevent ADC resource conflicts.

### Measurement State Management

The Sensors module uses a state machine to manage ADC resource access:

#### States:
- **MEASUREMENT_IDLE**: No measurement in progress, ready to accept new requests
- **TMP_MEASUREMENT_ONGOING**: Temperature measurement in progress
- **BAT_MEASUREMENT_ONGOING**: Battery measurement in progress

#### Behavior:
- **Request Acceptance**: New measurement requests are only processed when state is `MEASUREMENT_IDLE`
- **Request Dropping**: If a measurement request is made while another measurement is ongoing, the new request is silently dropped
- **State Transitions**: State changes from `IDLE` → `ONGOING` when measurement starts, and back to `IDLE` when measurement completes
- **Recovery Mechanism**: For periodic measurements, if a trigger fails due to ongoing measurement, a 1ms retry timer is started

## NBU Integration Features

> **Platform Support**: NBU integration features are only supported on **wireless_mcu** platforms. These features are **not available** on RT, mcxw23, and rw61x platforms.

### Periodic Temperature Measurement from NBU

The Sensors module supports automatic periodic temperature measurements triggered by NBU requests. This feature allows the system to automatically measure temperature at regular intervals without manual intervention. This temperature data is used for XTAL32M calibration and Channel Sounding calibration.

#### How it works:
1. **NBU Request**: The NBU can request periodic temperature measurements by calling the registered callback with a measurement interval
2. **Timer-based Scheduling**: A timer manager is used to schedule periodic measurements based on the requested interval
3. **Workqueue Processing**: Temperature measurement triggers are processed through the system workqueue to avoid blocking operations
4. **Automatic Refresh**: Once a measurement is complete, the next measurement is automatically scheduled

#### Key Components:
- **Temperature Request Callback**: `Sensors_TemperatureReqCb()` - Handles NBU requests for periodic measurements
- **Timer Callback**: `Sensors_TempMeasTimerCallback()` - Triggers measurements on timer expiration
- **Work Handler**: `Sensors_PeriodicTempWorkHandler()` - Processes temperature measurement requests in workqueue context
- **Ready Callback**: `Sensors_TemperatureReadyCb()` - Called when temperature measurement is complete

#### Usage:
- **One-shot measurement**: NBU requests with interval = 0
- **Periodic measurement**: NBU requests with interval > 0 (in milliseconds)
- The periodic measurement continues until a new request changes the interval or stops it

### Temperature Filtering and NBU Communication

#### Modified Moving Average Filter
On wireless_mcu platforms, temperature readings are processed through a Modified Moving Average (MMA) filter to reduce noise and prevent unnecessary NBU notifications. This filter helps to:

- **Limit XTAL32M calibration changes**: Prevents excessive calibration adjustments that could degrade RF performances
- **Optimize power consumption**: Reduces NBU wake-up events by limiting notifications to significant temperature changes
- **Reduce noise**: Smooths out temperature reading fluctuations for more stable system behavior

The XTAL32M calibration process uses a LUT (Look-Up Table) that contains pre-characterized calibration values mapping temperature ranges to optimal XTAL32M frequency adjustments, enabling precise crystal oscillator tuning based on thermal conditions.

The LUT is registered using `PLATFORM_RegisterXtal32MTempCompLut()` and this is currently done in `examples/_common/project_segments/wireless/wireless_mcu/app_common/hardware_init.c` and requires `gBoardUseXtal32MTempComp` to be enabled.

**Configuration**: Filter size is controlled by `PLATFORM_TEMPERATURE_FILTER_SIZE` (default: 4 samples)

#### NBU Temperature Threshold
Temperature values are only sent to the NBU when they exceed a configurable threshold:

**Threshold** is Controlled by `PLATFORM_TEMP_SENT_NBU_THRESHOLD` (default: 10 tenths of degrees Celsius = 1°C)

#### Temperature Processing Flow:
1. **Raw Measurement**: ADC interrupt captures temperature reading
2. **Sensors Module Informed** Sensors referesh the temperature value
2. **Filtering**: Raw value is processed through MMA filter
3. **Threshold Check**: Filtered value is compared against last sent value ± threshold
4. **NBU Communication**: If threshold is exceeded:
   - XTAL32M calibration is performed (if LUT is available)
   - Temperature value is sent to NBU
   - Last sent value is updated

```
Temperature Request
           ↓
ADC Interrupt - Measurement is ready
           ↓
Raw Temperature Reading
           ↓
Sensors Module Informed - Temperature refresh
           ↓
MMA Filter Processing
           ↓
Threshold Check: |filtered - last_sent| > threshold?
      ↓                    ↓
    YES                   NO
      ↓                    
LUT Available?
   ↓        ↓
 YES       NO
   ↓        ↓
XTAL32M     ↓
Calibration ↓
   ↓        ↓
   └────────┘
      ↓
Send Temperature to NBU
      ↓
Update Last Value Sent to NBU
```

#### Platform Configuration Macros:
```c
// Temperature filter size (number of samples for averaging)
#define PLATFORM_TEMPERATURE_FILTER_SIZE 4U

// Threshold for sending temperature to NBU (in tenths of degrees Celsius)
#define PLATFORM_TEMP_SENT_NBU_THRESHOLD 10  // 1°C
```

## API Usage Examples

### Manual Temperature Measurement
```c
int32_t temperature1, temperature2;
SENSORS_TriggerTemperatureMeasurement();
...
temperature1 = (SENSORS_RefreshTemperatureValue())/10;
temperature2 = (SENSORS_GetTemperature())/10;
```
In this case temperature1 is equal to temperature2, both are in degree celsius.

### Battery Level Measurement
```c
uint8_t batteryLevel;
SENSORS_TriggerBatteryMeasurement();
...
batteryLevel = SENSORS_RefreshBatteryLevel();
// batteryLevel is now in percentage (0-100%)
```

### Handling Concurrent Requests
```c
// First request - will be accepted
SENSORS_TriggerTemperatureMeasurement();

// Second request while first is ongoing - will be dropped silently
SENSORS_TriggerBatteryMeasurement();  // This will be ignored

// Wait for first measurement to complete
int32_t temp = SENSORS_RefreshTemperatureValue();

// Now battery measurement can be triggered
SENSORS_TriggerBatteryMeasurement();  // This will be accepted
```

### Low Power Optimization - Wake-up Temperature Measurement

```c
// In low power exit callback (interrupts masked, scheduler disabled)
/* Trig temperature measurement on wake-up.
 * If a temperature change is detected,
 * the new value will be sent to the NBU for appropriate XTAL32M trimming.
 * During channel sounding measurements, this temperature is also used for RTT compensation.
 * Since this function runs with interrupts masked and the scheduler disabled,
 * the unsafe variant must be used to avoid mutex acquisition.
 */
SENSORS_TriggerTemperatureMeasurementUnsafe();
```
This approach optimizes power consumption and provides additional benefits:
- **XTAL32M Trimming**: Temperature changes are sent to NBU for crystal oscillator calibration only if they exceed the configured threshold after filtering
- **RTT Compensation**: This temperature data is used by the NBU for RTT compensation during channel sounding measurements
- **Safe Execution**: Uses the unsafe variant to avoid mutex acquisition in interrupt context with scheduler disabled
In case of periodic measurement:
- **Power Optimization**: Triggers temperature measurements when the system is already awake, avoiding dedicated timer wake-ups
- **Timer Reset**: Resets the periodic measurement timer to prevent unnecessary wake-ups

> **Note**: This optimization is implemented by default in wireless platform examples when `gAppUseSensors_d` is enabled. The temperature measurement is automatically triggered in the low power exit callback (`BOARD_ExitLowPowerCb()`) in `examples/_common/project_segments/wireless/wireless_mcu/board_lp.c`.

## Constant macro definitions

Name :

```
#define VALUE_NOT_AVAILABLE_8 0xFFu
#define VALUE_NOT_AVAILABLE_32 0xFFFFFFFFu
```

Description :

Defines the error value that can be compared to the value obtain on the ADC.
