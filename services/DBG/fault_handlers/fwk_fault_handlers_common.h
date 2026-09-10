/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FWK_FAULT_HANDLERS_COMMON_H_
#define FWK_FAULT_HANDLERS_COMMON_H_

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdint.h>
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

#if defined(BOARD_DBG_FAULT_SINK_ENABLE) && (BOARD_DBG_FAULT_SINK_ENABLE > 0)
/**
 * @brief Fault-handler output sink callback type.
 *
 * A board or application registers one of these to receive the fault
 * handler's human-readable diagnostic text so that it lands on the same
 * output port the coredump backend uses. The framework calls the sink once
 * per formatted line.
 *
 * The callback runs in fault (HardFault) context: the scheduler is frozen,
 * the handler is single-shot, non-reentrant and terminal. The implementation
 * must therefore be synchronous, must not allocate, must not block on an RTOS
 * primitive, and must write inline to its port.
 *
 * @param[in] buf Pointer to the formatted text bytes for one line (not NUL required).
 * @param[in] len Number of bytes to emit (may be 0).
 *
 * @return 0 or a positive value on success.
 */
typedef int (*fwk_fault_output_fn_t)(const char *buf, uint16_t len);

/**
 * @brief Register (or clear) the fault-handler output sink.
 *
 * With no sink registered the fault report is dropped.
 *
 * @param[in] fn Sink callback, or NULL to clear the sink.
 */
void FWK_FaultHandlerSetOutput(fwk_fault_output_fn_t fn);
#endif /* BOARD_DBG_FAULT_SINK_ENABLE */

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

#endif /* FWK_FAULT_HANDLERS_COMMON_H_ */
