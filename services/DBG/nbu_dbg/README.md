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
  - NBU warning detection and notification

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
  - NBU warning status detection

## Key Features

### Fault Detection
- Single call to check NBU status

### Warning Detection
- NBU warning status monitoring
- Warning count reporting

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
// Check if NBU fault or warning has occurred
void NBUDBG_StateCheck(void);

// Register NBU system debug callback - Cb will be called upon NBUDBG_StateCheck usage if fault/new warning is detected
void NBUDBG_RegisterNbuDebugNotificationCb(nbu_dbg_system_cb_t cb);

// Extract debug information from NBU
int NBUDBG_StructDump(nbu_debug_struct_t *debug_struct);
```

## Usage Examples

### Basic NBU Debug Setup

```c
#include "fwk_nbu_dbg.h"

// NBU debug notification callback
static void BOARD_NbuDebugNotifyCb(const nbu_dbg_context_t *nbu_event)
{
    nbu_debug_struct_t debug_info;
    regs_status_t *regs;
    int status;

    status = NBUDBG_StructDump(&debug_info);
    if (status != 0)
    {
        PRINTF("ERROR: Failed to retrieve NBU debug information\n");
        return;
    }

    if (nbu_event->nbu_warning_count > 0U)
    {
        PRINTF("New NBU Warnings detected: %u warnings\n", nbu_event->nbu_warning_count);
    }

    if (nbu_event->nbu_error_count > 0U)
    {
        /* Fault/ Fatal assert on NBU side */
        regs = &debug_info.reg_dump;

        PRINTF("\n=== NBU Fault Analysis ===\n");
        PRINTF("Exception Information:\n");
        PRINTF("  Exception ID: 0x%08X\n", regs->exception_id);
        PRINTF("  NBU SHA1    : 0x%08X\n", regs->nbu_sha1);

        PRINTF("\nProcessor State:\n");
        PRINTF("  PC  (Program Counter): 0x%08X\n", regs->pc);
        PRINTF("  LR  (Link Register)  : 0x%08X\n", regs->lr);
        PRINTF("  SP  (Stack Pointer)  : 0x%08X\n", regs->sp);
        PRINTF("  PSR (Program Status) : 0x%08X\n", regs->psr);
        /*** Additional debug analysis can be performed here ***/
        /*** System recovery actions ***/
        /*** Consider NBU reset, system restart, or safe mode entry ***/
    }
}

// Initialize NBU debug monitoring
int BOARD_DbgNbuInit(void)
{
    NBUDBG_RegisterNbuDebugNotificationCb(BOARD_NbuDebugNotifyCb);
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
