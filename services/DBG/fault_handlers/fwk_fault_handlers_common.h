/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fwk_config.h"

/* -------------------------------------------------------------------------- */
/*                               Public macros                                */
/* -------------------------------------------------------------------------- */
/* Enable coredump debug feature in fault handler
 * When enabled, the fault handler will capture and send core dump information
 * to the host for debugging purposes.
 */
#ifndef gFaultHandlerCoredumpEnabled_d
#define gFaultHandlerCoredumpEnabled_d 0
#endif

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Hard Fault exception handler
 *
 * Extracts register values, fault status registers, and diagnostic information
 * to help decode the cause of the fault.
 *
 * @param[in] hardfault_args Pointer to the stacked registers at the time of the fault
 *
 * @note Implementation may differ between host and controller targets.
 * @note This function does not return - enters infinite loop after dumping diagnostics.
 */
void HardFaultHandler(unsigned long *hardfault_args);
