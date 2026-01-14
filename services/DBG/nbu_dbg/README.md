NBU Debug Module

## Overview

The NBU Debug module provides debugging capabilities for NBU (Narrow Band Unit) fault detection and analysis. It allows the host to monitor NBU status, detect faults/fatal assert, and extract debug information when crashes occur, and handle warning notifications.

## Platform Support

> **Platform Support**: NBU Debug module is currently only supported on **KW47-MCXW72** platform.

## Module Structure

### Debug Structure

- **File**: `framework/services/DBG/nbu_dbg/common/`
- **Description**: Debug structure with extended debugging capabilities
- **Features**: 
  - Comprehensive register dump
  - Execution context information (handler mode vs thread mode)
  - Thread information capture
  - Support for both BLE LL debug data (15.4 planned)

### Host Interface

#### Host Debug Implementation
- **Files**: 
  - `framework/services/DBG/nbu_dbg/host_interface/fwk_nbu_dbg.c`
  - `framework/services/DBG/nbu_dbg/host_interface/fwk_nbu_dbg.h`
- **Description**: Host-side implementation for NBU debug functionality
- **Responsibilities**:
  - NBU fault detection
  - Debug structure extraction
  - System error callback management
  - NBU warning detection and notification with ID tracking
  - HCI logging callback registration

### NBU Interface

#### NBU Debug Implementation
- **File**: `framework/services/DBG/nbu_dbg/nbu_interface/fwk_nbu_dbg.c`
- **Description**: NBU-side implementation for debug data collection
- **Responsibilities**:
  - Debug structure region definition
  - Warning notification support

### Platform Support

#### Platform Debug Implementation - NBU side

- **File**: `framework/platform/kw47_mcxw72_nbu/fwk_platform_dbg.c`
- **Description**: Platform-specific debug functionality
- **Features**:
  - NBU fault status indication

#### Platform Debug Implementation - Host side

- **File**: `framework/platform/kw47_mcxw72/fwk_platform_dbg.c`
- **Description**: Platform-specific debug functionality
- **Features**:
  - NBU fault status detection
  - NBU warning status detection

## Key Features

### Fault Detection
- Single call to check NBU status

### Warning Detection
- NBU warning status monitoring
- Warning count reporting
- Circular buffer tracking of last 7 warnings

### Assert Detection and Analysis
- Fatal assert detection from NBU
- File name and line number capture

### HCI Vendor Event Support
- Debug structure transmission over HCI vendor events
- RAM log transmission capability
- Configurable event generation

### HCI Logging
- HCI packet logging callback support
- Captures both TX and RX HCI packets
- Platform-level integration

### Debug Information Extraction
- Complete register state capture
- Exception context preservation
- Protocol-specific debug data collection

### Execution Context Analysis
- Handler mode vs thread mode detection
- IRQ number identification for handler mode faults
- Thread information capture for thread mode faults

## Debug Data Population

### Fault Handler Requirements
In order to have meaningful content in the debug structure, the NBU must use fault handlers that populate the crash context at the moment of the fault. The fault handlers are responsible for:
- Capturing processor register state
- Recording exception information
- Preserving execution context (handler/thread mode)
- Indicate the fault to the Host

### BLE Debug Data
Unlike the crash context which is captured at fault time, BLE debug data behaves differently:
- **Runtime Population**: BLE debug data is changed and added gradually during runtime
- **Dynamic Content**: The BLE debug section reflects the current state of the BLE LL

### Assert Context
When a fatal assert occurs on the NBU:
- **File Information**: The filename where the assert occurred is captured (up to 74 characters)
- **Line Number**: The exact line number is recorded
- **Magic Pattern**: A special exception ID (0x00A55E27) identifies assert conditions
- **Context Preservation**: Assert information shares memory space with register dump to optimize memory usage

### Warning Context
When warnings occur on the NBU:
- **Warning ID**: Each warning has a unique identifier for root cause analysis
- **Circular Buffer**: Last 7 warnings are tracked in a circular buffer
- **Warning Index**: Current position in the circular buffer is maintained

## API Overview

### Main Functions

```c
// Check if NBU fault, assert, or warning condition has occurred
// Note: Not thread-safe, must be called from a single context only (e.g., IDLE task)
// Prevents duplicate notifications - each condition reported only once
void NBUDBG_StateCheck(void);

// Register NBU system debug callback - Called upon NBUDBG_StateCheck if fault/assert/warning is detected
void NBUDBG_RegisterNbuDebugNotificationCb(nbu_dbg_system_cb_t cb);

// Extract debug information from NBU and optionally send via HCI vendor events
int NBUDBG_StructDump(nbu_debug_struct_t *debug_struct);

// Configure HCI vendor event transmission for debug information
void NBUDBG_ConfigureHciVendorEvent(uint32_t config_mask);

// Register HCI logging callback for packet monitoring
// The callback will be invoked for all HCI packets (TX and RX)
void NBUDBG_RegisterHciLogCallback(platform_hci_log_cb_t cb);
```

## Usage Examples

Complete NBU debug integration example code can be found in the following files:
- `mcuxsdk/examples/_common/project_segments/wireless/wireless_mcu/debug/board_nbu_dbg.c`
- `mcuxsdk/examples/_common/project_segments/wireless/wireless_mcu/debug/board_nbu_dbg.h`

### Basic NBU Debug Setup

```c
#include "fwk_nbu_dbg.h"

// NBU debug notification callback
static void BOARD_NbuDebugNotifyCb(const nbu_dbg_context_t *nbu_event)
{
    nbu_debug_struct_t debug_info;
    nbu_dbg_info_t *nbu_dbg_info;
    reg_info_t *regs;
    int status;

    do
    {
        if (nbu_event->nbu_warning_count > 0U)
        {
            PRINTF("WARNING: %u New NBU Warnings detected\n", nbu_event->nbu_warning_count);
        }

        status = NBUDBG_StructDump(&debug_info);
        if (status != 0)
        {
            PRINTF("ERROR: Failed to retrieve NBU debug information\n");
            break;
        }

        PRINTF("NBU Debug version: 0x%04X\n", debug_info.version);
        if (debug_info.version != (uint16_t)NBUDBG_VERSION)
        {
            PRINTF("!! Host Debug version 0x%04X != NBU debug version 0x%04X !!\n", (uint16_t)NBUDBG_VERSION, debug_info.version);
            PRINTF("!! The following analysis may be incorrect !!\n");
        }

        if (nbu_event->nbu_warning_count > 0U)
        {
            PRINTF("=== Warning Circular Table ===\n");
            for(uint8_t i = 0U; i < NBUDBG_MAX_NB_WARNINGS; i++)
            {
                if (i == debug_info.nbu_dbg_info.warning_index)
                {
                    PRINTF("->");
                }
                PRINTF("%u\n", debug_info.nbu_dbg_info.warnings[i]);
            }
        }

        if ((nbu_event->nbu_error_count > 0U))
        {
            nbu_dbg_info = &debug_info.nbu_dbg_info;
            regs = &debug_info.nbu_dbg_info.reg_info;
            PRINTF("\n=== NBU Fault/Assert Analysis ===\n\n");
            if (nbu_dbg_info->exception_id == NBUDBG_EXCEPTION_ID_FOR_ASSERT_MAGIC)
            {
                /* Assert on NBU side */
                PRINTF("NBU Assert Detected\n");
                PRINTF("  Line: %u\n", nbu_dbg_info->assert_info.line);
                PRINTF("  File name: %s\n", nbu_dbg_info->assert_info.file_name);
            }
            else
            {
                /* Fault on NBU side */
                PRINTF("NBU Fault Detected\n");
                PRINTF("Exception Information:\n");
                PRINTF("  Exception ID: 0x%08X\n", nbu_dbg_info->exception_id);
                PRINTF("  NBU SHA1    : 0x%08X\n", nbu_dbg_info->nbu_sha1);

                PRINTF("\nProcessor State:\n");
                PRINTF("  PC  (Program Counter): 0x%08X\n", regs->pc);
                PRINTF("  LR  (Link Register)  : 0x%08X\n", regs->lr);
                PRINTF("  SP  (Stack Pointer)  : 0x%08X\n", regs->sp);
                PRINTF("  PSR (Program Status) : 0x%08X\n", regs->psr);
                /*** Additional debug analysis can be performed here ***/
            }
            /*** System recovery actions ***/
            /*** Consider NBU reset, system restart, or safe mode entry ***/
        }
    }
}

// Initialize NBU debug monitoring
int BOARD_DbgNbuInit(void)
{
    NBUDBG_RegisterNbuDebugNotificationCb(BOARD_NbuDebugNotifyCb);

    // Configure to send debug structure via HCI vendor events
    NBUDBG_ConfigureHciVendorEvent(NBUDBG_HCI_EVENT_DEBUG_STRUCT);
    // Or enable both debug structure and RAM log transmission:
    // NBUDBG_ConfigureHciVendorEvent(NBUDBG_HCI_EVENT_ALL);

    // Optional: Register HCI logging callback
    // NBUDBG_RegisterHciLogCallback(BOARD_HciLogCallback);
    return 0;
}

// Process NBU debug events (call periodically)
void BOARD_DbgNbuProcess(void)
{
    NBUDBG_StateCheck();
}
```

## Limitations

- Currently only supported on KW47-MCXW72 platform
- Requires NBU fault handlers to be enabled for fault detection

## Debug Structure Versioning

The debug structure includes a version field to ensure compatibility: Applications should validate the version field before processing debug information to prevent misinterpretation of structure layouts across different NBU firmware versions.
