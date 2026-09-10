/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FWK_FAULT_HANDLERS_UTILS_H_
#define FWK_FAULT_HANDLERS_UTILS_H_

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*                               Public macros                                */
/* -------------------------------------------------------------------------- */
#define _TEXT_START              m_text_start
#define _TEXT_END                m_text_end
#define _STACK_TOP               m_cstack_end
#define DBG_NO_ACTIVE_DEVICE_IRQ 0xFFFFFFFFU

/* Size of the FWK_FaultPrintfImpl stack line buffer, one formatted line at a
 * time. Longest known line is the ThreadX callstack banner
 * (93 bytes with 32-bit addresses). Overridable by the consuming project; a
 * smaller value truncates the affected lines. */
#ifndef FWK_FAULT_OUTPUT_LINE_SIZE
#define FWK_FAULT_OUTPUT_LINE_SIZE 128U
#endif

#if defined(BOARD_DBG_FAULT_SINK_ENABLE) && (BOARD_DBG_FAULT_SINK_ENABLE > 0)
/* App-core (non-NBU): route the fault report through the registered sink when
 * present; if no sink is registered the report is dropped (no PRINTF fallback). */
#define FWK_FaultPrintf(...) FWK_FaultPrintfImpl(__VA_ARGS__)
#else
/* The fault-report emitter compiles to nothing. No buffer,
 * no vsnprintf reference and no sink pointer are emitted into the image (NBU case). */
#define FWK_FaultPrintf(...)
#endif

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */
extern unsigned int m_text_start[];
extern unsigned int m_text_end[];
extern unsigned int m_cstack_end[];

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

#if defined(BOARD_DBG_FAULT_SINK_ENABLE) && (BOARD_DBG_FAULT_SINK_ENABLE > 0)
/**
 * \brief Formats one fault-report line and hands it to the registered sink
 *
 * \param[in] fmt printf-style format string, followed by its arguments
 */
void FWK_FaultPrintfImpl(const char *fmt, ...);
#endif /* BOARD_DBG_FAULT_SINK_ENABLE */

/**
 * \brief Parses a stack and dumps addresses that are possibly part of the call stack
 *
 * This function scans through a stack memory range and prints addresses that fall
 * within the text (code) section, which are likely to be part of the call stack.
 * It helps with debugging by showing the sequence of function calls that led to
 * the current execution point.
 *
 * \param[in] stack_ptr Starting address of the stack to analyze (lower address)
 * \param[in] stack_top Ending address of the stack to analyze (higher address)
 */
void sys_dump_callstack(unsigned int stack_ptr, unsigned int stack_top);

/**
 * \brief Dumps the exception call stack from the current exception context
 *
 * This function retrieves the Main Stack Pointer (MSP) from the current exception
 * context and uses it to analyze the call stack from the exception point to the
 * top of the stack. It's specifically designed to be called from exception handlers
 * to help debug what sequence of function calls led to the exception.
 */
void sys_dump_exception_callstack(void);

/**
 * \brief Dumps the current interrupt status showing pending and active IRQs
 *
 * This function iterates through all available IRQ numbers and checks their status
 * using NVIC functions. It prints information about any pending or active interrupts,
 * which is useful for debugging interrupt-related issues or understanding the system
 * state during fault conditions.
 *
 * \return Active device specific irq or DBG_NO_ACTIVE_DEVICE_IRQ if none is found
 */
unsigned int sys_dump_interrupt_status(void);

#endif /* FWK_FAULT_HANDLERS_UTILS_H_ */
