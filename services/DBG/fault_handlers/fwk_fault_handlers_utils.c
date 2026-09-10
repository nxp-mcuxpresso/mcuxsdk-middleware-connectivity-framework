/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdbool.h>
#include "fsl_common.h"
#include "fsl_device_registers.h"
#include "EmbeddedTypes.h"
#include "fwk_fault_handlers_utils.h"
#include "fwk_fault_handlers_common.h"

#if defined(BOARD_DBG_FAULT_SINK_ENABLE) && (BOARD_DBG_FAULT_SINK_ENABLE > 0)
#include <stdarg.h>
#include <stdio.h>
#endif

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
/* Determine the highest IRQ number by subtracting system exceptions and adjusting for zero-based indexing */
#define IRQN_TYPE_MAX               NUMBER_OF_INT_VECTORS - 16 - 1
#define SYS_DEBUG_CALLSTACK_KEYWORD ">>>"

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
#if defined(BOARD_DBG_FAULT_SINK_ENABLE) && (BOARD_DBG_FAULT_SINK_ENABLE > 0)
/* Registered fault-report sink. NULL means "no sink": the fault report is
 * dropped. The sink is registered at boot by FWK_FaultHandlerSetOutput(); a
 * fault that fires before registration therefore produces no output. Only read
 * from fault context, so no locking is needed. */
static fwk_fault_output_fn_t s_fault_output_fn = NULL;
#endif

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

#if defined(BOARD_DBG_FAULT_SINK_ENABLE) && (BOARD_DBG_FAULT_SINK_ENABLE > 0)
void FWK_FaultHandlerSetOutput(fwk_fault_output_fn_t fn)
{
    s_fault_output_fn = fn;
}

/* Format one fault-report line and hand it to the registered sink (the coredump
 * port). When no sink is registered the report is dropped: no formatting is done
 * and nothing is emitted.
 */
void FWK_FaultPrintfImpl(const char *fmt, ...)
{
    char     line_buf[FWK_FAULT_OUTPUT_LINE_SIZE];
    va_list  ap;
    int      n;
    uint16_t len;

    if (s_fault_output_fn != NULL)
    {
        va_start(ap, fmt);
        n = vsnprintf(line_buf, sizeof(line_buf), fmt, ap);
        va_end(ap);

        if (n > 0)
        {
            if ((unsigned int)n < sizeof(line_buf))
            {
                len = (uint16_t)n;
            }
            else
            {
                /* Output was truncated to fit the buffer; emit what we have. */
                len = (uint16_t)(sizeof(line_buf) - 1U);
            }

            (void)s_fault_output_fn(line_buf, len);
        }
    }
}
#endif /* BOARD_DBG_FAULT_SINK_ENABLE */

/* Parses a stack and dump addresses that are possibly part of the call stack
 * (each address in the text range)
 */
void sys_dump_callstack(unsigned int stack_ptr, unsigned int stack_top)
{
    unsigned int *addr;

    FWK_FaultPrintf("CallStack from sp=0x%x to top=0x%x (approximate):\r\n", stack_ptr, stack_top);
    FWK_FaultPrintf(SYS_DEBUG_CALLSTACK_KEYWORD);

    /* Search for possible backtrace addresses in the stack */
    for (addr = (unsigned int *)stack_ptr; addr < (unsigned int *)stack_top; addr++)
    {
        unsigned int val = *addr;
        if ((val > (unsigned int)_TEXT_START) && (val < (unsigned int)_TEXT_END))
        {
            FWK_FaultPrintf("0x%x ", val);
        }
    }
    FWK_FaultPrintf("\r\n");
}

unsigned int sys_dump_interrupt_status(void)
{
    unsigned int IRQn;
    unsigned int device_active_irq   = DBG_NO_ACTIVE_DEVICE_IRQ;
    bool_t       irq_pending_running = FALSE;

    FWK_FaultPrintf("\r\n==========INTERRUPT STATUS==========\r\n");

    for (IRQn = 0U; IRQn <= IRQN_TYPE_MAX; IRQn++)
    {
        if (NVIC_GetPendingIRQ((IRQn_Type)IRQn))
        {
            FWK_FaultPrintf("IRQ pending: %d\r\n", IRQn);
            irq_pending_running = TRUE;
        }
        if (NVIC_GetActive((IRQn_Type)IRQn))
        {
            device_active_irq = IRQn;
            FWK_FaultPrintf("IRQ active : %d\r\n", IRQn);
            irq_pending_running = TRUE;
        }
    }

    if (!irq_pending_running)
    {
        FWK_FaultPrintf("No IRQ pending or running\r\n");
    }

    return device_active_irq;
}

void sys_dump_exception_callstack(void)
{
    volatile unsigned int exception_sp;
    __asm volatile(" mrs r0, msp");
    __asm volatile(" mov %0, r0" : "=r"(exception_sp));

    FWK_FaultPrintf("Exception Callstack from 0x%x to 0x%x\r\n", exception_sp, (unsigned int)_STACK_TOP);
    sys_dump_callstack(exception_sp, (unsigned int)_STACK_TOP);
    FWK_FaultPrintf("\r\n");
}
