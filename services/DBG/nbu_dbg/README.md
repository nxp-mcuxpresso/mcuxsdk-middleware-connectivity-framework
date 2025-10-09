NBU Debug Module

## Overview

The NBU Debug module provides debugging capabilities for NBU (Narrow Band Unit) fault detection and analysis. It allows the host to monitor NBU status, detect faults/fatal assert, and extract debug information when crashes occur.

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

### NBU Interface

#### NBU Debug Implementation
- **File**: `framework/services/DBG/nbu_dbg/nbu_interface/fwk_nbu_dbg.c`
- **Description**: NBU-side implementation for debug data collection
- **Responsibilities**:
  - Debug structure region definition

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

## Key Features

### Fault Detection
- Single call to check NBU status

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

## API Overview

### Main Functions

```c
// Check if NBU fault has occurred
void NBUDBG_StateCheck(void);

// Register system error callback - Cb will be called upon NBUDBG_StateCheck usage if the fault is detected
void NBUDBG_RegisterSystemErrorCb(nbu_dbg_system_err_cb_t cb);

// Extract debug information from NBU
int NBUDBG_StructDump(nbu_debug_struct_t *debug_struct);
```

## Usage Examples

### Basic NBU Debug Setup

```c
#include "fwk_nbu_dbg.h"

// NBU fault analysis callback
static void BOARD_NbuSystemNotifyCb(nbu_dbg_event_id_t event_id)
{
    nbu_debug_struct_t debug_info;
    int status;

    // Extract debug information from NBU
    status = NBUDBG_StructDump(&debug_info);
    if (status != 0)
    {
        PRINTF("ERROR: Failed to retrieve NBU debug information\n");
        return;
    }
   /*** Detailed debug information logging ***/
}

// Initialize NBU debug monitoring
int BOARD_DbgNbuInit(void)
{
    NBUDBG_RegisterSystemErrorCb(BOARD_NbuSystemNotifyCb);
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
- Requires NBU fault handlers to be enabled
