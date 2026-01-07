/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fsl_common.h"
#include "EmbeddedTypes.h"
#include "fwk_debug.h"
#include "fwk_fault_handlers_utils.h"
#include "fwk_fault_handlers_common.h"
#include "fwk_fault_handlers_rtos_port.h"
#include "fwk_debug_struct.h"
#include "fwk_platform_dbg.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
static int sys_debug_panic_triggered = 0;

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

typedef enum
{
    FAULT_REASON_UNKNOWN         = 0x00,
    FAULT_REASON_HARD_FAULT      = 0x01,
    FAULT_REASON_MEMMANAGE_FAULT = 0x02,
    FAULT_REASON_BUS_FAULT       = 0x03,
    FAULT_REASON_USAGE_FAULT     = 0x04,
} fault_reason_t;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */
/**
 * HardFaultHandler:
 * This is called from the HardFault_HandlerAsm with a pointer the Fault stack
 * as the parameter. We can then read the values from the stack and place them
 * into local variables for ease of reading.
 * We then read the various Fault Status and Address Registers to help decode
 * cause of the fault.
 */
void HardFaultHandler(unsigned long *hardfault_args)
{
    volatile unsigned int stacked_r0;
    volatile unsigned int stacked_r1;
    volatile unsigned int stacked_r2;
    volatile unsigned int stacked_r3;
    volatile unsigned int stacked_r4;
    volatile unsigned int stacked_r5;
    volatile unsigned int stacked_r6;
    volatile unsigned int stacked_r7;
    volatile unsigned int stacked_r8;
    volatile unsigned int stacked_r9;
    volatile unsigned int stacked_r10;
    volatile unsigned int stacked_r11;
    volatile unsigned int stacked_r12;
    volatile unsigned int stacked_lr;
    volatile unsigned int stacked_pc;
    volatile unsigned int stacked_sp;
    volatile unsigned int stacked_psr;
    volatile unsigned int EXEC_RETURN;
    unsigned int          stack_frame_size;
    unsigned int          active_device_irq;
    dbg_thread_info       current_thread;
    fault_reason_t        exception_reason;

    exception_reason = FAULT_REASON_UNKNOWN;

    __asm volatile(" mov %0, lr" : "=r"(EXEC_RETURN));
    stacked_psr = ((unsigned int)hardfault_args[7]);

    if (!NBUDBG_IS_NBU_ASSERT())
    {
        stacked_r0  = ((unsigned int)hardfault_args[0]);
        stacked_r1  = ((unsigned int)hardfault_args[1]);
        stacked_r2  = ((unsigned int)hardfault_args[2]);
        stacked_r3  = ((unsigned int)hardfault_args[3]);
        stacked_r12 = ((unsigned int)hardfault_args[4]);
        stacked_lr  = ((unsigned int)hardfault_args[5]);
        stacked_pc  = ((unsigned int)hardfault_args[6]);

        __asm volatile(" mov %0, r4" : "=r"(stacked_r4));
        __asm volatile(" mov %0, r5" : "=r"(stacked_r5));
        __asm volatile(" mov %0, r6" : "=r"(stacked_r6));
        __asm volatile(" mov %0, r7" : "=r"(stacked_r7));
        __asm volatile(" mov %0, r8" : "=r"(stacked_r8));
        __asm volatile(" mov %0, r9" : "=r"(stacked_r9));
        __asm volatile(" mov %0, r10" : "=r"(stacked_r10));
        __asm volatile(" mov %0, r11" : "=r"(stacked_r11));

        /* Calculate the correct stack pointer based on FPU usage
         * Basic stack frame: 8 words (32 bytes)
         * Extended stack frame (with FPU): 26 words (104 bytes)
         * EXC_RETURN[4] == 0 means FPU state was saved (extended frame)
         */
        if ((EXEC_RETURN & 0x10U) == 0U)
        {
            stack_frame_size = 26U; /* Extended frame with FPU registers */
        }
        else
        {
            stack_frame_size = 8U; /* Basic frame without FPU registers */
        }
        /* need to remove the stacked words from sp to get the initial SP value when the fault occurred */
        stacked_sp = (unsigned int)hardfault_args + (stack_frame_size * 4U);

        NBUDBG_SET_REG(pc, stacked_pc);
        NBUDBG_SET_REG(lr, stacked_lr);
        NBUDBG_SET_REG(sp, stacked_sp);
        NBUDBG_SET_REG(psr, stacked_psr);
        NBUDBG_SET_REG(r0, stacked_r0);
        NBUDBG_SET_REG(r1, stacked_r1);
        NBUDBG_SET_REG(r2, stacked_r2);
        NBUDBG_SET_REG(r3, stacked_r3);
        NBUDBG_SET_REG(r4, stacked_r4);
        NBUDBG_SET_REG(r5, stacked_r5);
        NBUDBG_SET_REG(r6, stacked_r6);
        NBUDBG_SET_REG(r7, stacked_r7);
        NBUDBG_SET_REG(r8, stacked_r8);
        NBUDBG_SET_REG(r9, stacked_r9);
        NBUDBG_SET_REG(r10, stacked_r10);
        NBUDBG_SET_REG(r11, stacked_r11);
        NBUDBG_SET_REG(r12, stacked_r12);

#if (__CORTEX_M == 33) || (__CORTEX_M == 3)
        uint32_t ipsr = __get_IPSR();
        if (ipsr >= 3U && ipsr <= 6U)                       /* Either Hardfault, MemFault, BusFault, UsageFault */
        {
            exception_reason = (fault_reason_t)(ipsr - 2U); /* Map to fault_reason_t */
        }

        NBUDBG_SET_EXCEPTION_ID(ipsr);
        NBUDBG_SET_REG(cfsr, SCB->CFSR);

        if ((SCB->CFSR & SCB_CFSR_MMARVALID_Msk) != 0U)
        {
            NBUDBG_SET_XFAR(mmfar, SCB->MMFAR);
        }
        if ((SCB->CFSR & SCB_CFSR_BFARVALID_Msk) != 0U)
        {
            NBUDBG_SET_XFAR(bfar, SCB->BFAR);
        }
#endif
    }
    else
    {
        /* This is a NBU assert, regs log are not needed in that case  */
        /* Debug structure is expected to be used by the NBU assert hook to log assert info */
    }

    /* Avoid recursive panic/fault */
    if (sys_debug_panic_triggered < 1)
    {
        sys_debug_panic_triggered++;

        if (EXEC_RETURN & 0x4)
        {
            if (sys_get_current_task_info(&current_thread) == 0)
            {
                NBUDBG_SetThreadContext(current_thread.thread_entry_addr, current_thread.thread_name);
            }
        }
        else
        {
            active_device_irq = sys_dump_interrupt_status();
            if ((uint32_t)active_device_irq != DBG_NO_ACTIVE_DEVICE_IRQ)
            {
                NBUDBG_SET_HANDLER_MODE_IRQ((uint32_t)active_device_irq);
            }
        }
    }

    /* NBU failure indication to host core */
    PLATFORM_Nbu2HostFaultIndication();

#if defined(gFaultHandlerCoredumpEnabled_d) && (gFaultHandlerCoredumpEnabled_d > 0U)
    extern void bt_dbg_gen_exception_handler(fault_reason_t reason, uint32_t SP, uint32_t xPSR);
    bt_dbg_gen_exception_handler(exception_reason, (uint32_t)hardfault_args, (uint32_t)stacked_psr);
#else
    (void)exception_reason;
#endif

    while (true)
    {
    }
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
