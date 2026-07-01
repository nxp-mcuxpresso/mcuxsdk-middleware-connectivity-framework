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
#if defined(SDK_OS_FREE_RTOS) || defined(FSL_RTOS_THREADX)
#include "fwk_fault_handlers_rtos_port.h"
#endif

#if defined(CONFIG_DEBUG_COREDUMP)
#include "zephyr_headers/arch/arm/thread.h"
/*
 * fault_capture() lives in the coredump component (error_stack_frame.c) and has
 * no public header. It builds the Cortex-M exception stack frame from the
 * provided MSP/PSP/EXC_RETURN, sets z_arm_coredump_fault_sp, decodes the fault
 * reason and finally calls coredump(). This is the same entry point the
 * standalone coredump_fault example reaches through its assembly trampoline.
 */
extern void fault_capture(uint32_t msp, uint32_t psp, uint32_t exc_return, _callee_saved_t *callee_regs);
#endif

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

    /* Capture EXC_RETURN. On exception entry the hardware loads LR with the
     * EXC_RETURN value; HardFault_Handler (fwk_fault_handlers_common.c) reaches
     * this function with a tail branch ("bx", not "bl"), so LR is NOT clobbered
     * by the hand-off and still holds the fault-time EXC_RETURN here. This read
     * is the first C statement in the handler (before any bl/function call), so
     * EXEC_RETURN is the fault-time value, not a live post-fault LR. EXC_RETURN[2]
     * (the "& 0x4" tests below) therefore reliably tells Thread vs Handler mode,
     * and the same value is forwarded to fault_capture() for the coredump. */
    __asm volatile(" mov %0, lr" : "=r"(EXEC_RETURN));

    stacked_r0  = ((unsigned int)hardfault_args[0]);
    stacked_r1  = ((unsigned int)hardfault_args[1]);
    stacked_r2  = ((unsigned int)hardfault_args[2]);
    stacked_r3  = ((unsigned int)hardfault_args[3]);
    stacked_r12 = ((unsigned int)hardfault_args[4]);
    stacked_lr  = ((unsigned int)hardfault_args[5]);
    stacked_pc  = ((unsigned int)hardfault_args[6]);
    stacked_psr = ((unsigned int)hardfault_args[7]);

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

    /* Dump the cortex register first in case of issues in the logging dump */
    PRINTF("\r\n============\r\n");
    PRINTF("HardFault from pc    = 0x%08x (%d)\r\n", stacked_pc, stacked_pc);

    PRINTF("r0    = 0x%08x\r\n", stacked_r0);
    PRINTF("r1    = 0x%08x\r\n", stacked_r1);
    PRINTF("r2    = 0x%08x\r\n", stacked_r2);
    PRINTF("r3    = 0x%08x\r\n", stacked_r3);

    PRINTF("r4    = 0x%08x\r\n", stacked_r4);
    PRINTF("r5    = 0x%08x\r\n", stacked_r5);
    PRINTF("r6    = 0x%08x\r\n", stacked_r6);
    PRINTF("r7    = 0x%08x\r\n", stacked_r7);
    PRINTF("r8    = 0x%08x\r\n", stacked_r8);
    PRINTF("r9    = 0x%08x\r\n", stacked_r9);
    PRINTF("r10   = 0x%08x\r\n", stacked_r10);
    PRINTF("r11   = 0x%08x\r\n", stacked_r11);

    PRINTF("r12   = 0x%08x\r\n", stacked_r12);
    PRINTF("lr    = 0x%08x (%d)\r\n", stacked_lr, stacked_lr);
    PRINTF("sp    = 0x%08x\r\n", stacked_sp);
    PRINTF("psr   = 0x%08x\r\n", stacked_psr);

#if (__CORTEX_M == 33) || (__CORTEX_M == 3)
    PRINTF("_CFSR = 0x%08x (Configurable Fault Status Register: UFSR|BFSR|MMSR)\r\n", SCB->CFSR);
    PRINTF("_HFSR = 0x%08x (Hard Fault Status Register)\r\n", SCB->HFSR);
    PRINTF("_DFSR = 0x%08x (Debug Fault Status Register)\r\n", SCB->DFSR);
    PRINTF("_AFSR = 0x%08x (Auxiliary Fault Status Register)\r\n", SCB->AFSR);
    PRINTF("_SHCSR= 0x%08x (System Handler Control and State Register)\r\n", (unsigned int)SCB->SHCSR);

    if ((SCB->CFSR & SCB_CFSR_MMARVALID_Msk) != 0U)
    {
        PRINTF("_MMAR = 0x%08x (MemManage Fault Address Register)\r\n", SCB->MMFAR);
    }
    if ((SCB->CFSR & SCB_CFSR_BFARVALID_Msk) != 0U)
    {
        PRINTF("_BFAR = 0x%08x (Bus Fault Address Register)\r\n", SCB->BFAR);
    }
    if ((SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk) != 0U)
    {
        PRINTF("!! Division by zero !!\r\n");
    }
    if ((SCB->CFSR & SCB_CFSR_UNALIGNED_Msk) != 0U)
    {
        PRINTF("!! Unaligned access !!\r\n");
    }
#if defined(__ARM_ARCH_8M_MAIN__) && (__ARM_ARCH_8M_MAIN__ == 1)
    if ((SCB->CFSR & SCB_CFSR_STKOF_Msk) != 0U)
    {
        PRINTF("!! Stack overflow !!\r\n");
    }
#endif

    PRINTF("Exception_id = 0x%08x \r\n", __get_IPSR());
#endif

    PRINTF("EXEC_RETURN  = 0x%08x (%d current lr)\r\n", EXEC_RETURN, EXEC_RETURN);
    PRINTF("comming from %s\r\n", (EXEC_RETURN & 0x4) ? "Thread mode (Process stack)" : "Handler mode (Main Stack)");

    /* If gLoggingActive_d is set, Dump the log now */
    DBG_LOG_DUMP();

    /* Avoid recursive panic/fault */
    if (sys_debug_panic_triggered < 1)
    {
        sys_debug_panic_triggered++;

        if (EXEC_RETURN & 0x4)
        {
            /* Thread mode */
#if defined(SDK_OS_FREE_RTOS) || defined(FSL_RTOS_THREADX)
            sys_dump_task_stats();
#endif
        }
        else
        {
            /* Print diagnostics */
            (void)sys_dump_interrupt_status();
            /* Handler mode */
            sys_dump_exception_callstack();
        }
        PRINTF("\r\n");
        PRINTF(
            "Command for parsing a stack:\r\n>arm-none-eabi-addr2line.exe -e <your_project>.elf "
            "<paste_stack_line_above>\r\nadd option -f to show the function and line\r\n");
    }

#if defined(CONFIG_DEBUG_COREDUMP)
    /* Capture the fault as a Zephyr coredump. fault_capture() rebuilds the
     * exception stack frame from the fault-time stack pointer, sets
     * z_arm_coredump_fault_sp, decodes the fault reason and calls coredump(),
     * which streams the dump through the selected backend (over the application
     * console for the board_debug backend). Done after the human-readable
     * register dump so the two outputs do not interleave.
     *
     * hardfault_args already points at the fault-time stack frame (the asm
     * trampoline selected MSP or PSP based on EXC_RETURN[2]). Pass it as the
     * stack get_esf() will dereference for the active mode; the other pointer is
     * not used to locate the frame.
     *
     * Signature (error_stack_frame.c):
     *   void fault_capture(uint32_t msp, uint32_t psp, uint32_t exc_return,
     *                      _callee_saved_t *callee_regs);
     * Argument 1 is always the MSP, argument 2 is always the PSP; fault_capture()
     * picks the frame belonging to the active mode from EXC_RETURN[2], so the
     * fault-time frame pointer (hardfault_args) must go into the slot matching
     * the mode we are in.
     *
     * callee_regs is passed NULL on purpose: the callee-saved registers are not
     * captured here. This is safe -- fault_capture() never dereferences the
     * pointer. With CONFIG_EXTRA_EXCEPTION_INFO it is ARG_UNUSED; otherwise it is
     * only stored (esf.extra_info.callee = callee_regs), so NULL cannot trigger a
     * secondary fault during the dump.
     */
    if ((EXEC_RETURN & 0x4U) != 0U)
    {
        /* Thread mode: fault frame is on the process stack, so hardfault_args is
         * the PSP-based frame and goes into the psp (2nd) argument. */
        fault_capture(__get_MSP(), (uint32_t)hardfault_args, EXEC_RETURN, NULL);
    }
    else
    {
        /* Handler mode: fault frame is on the main stack, so hardfault_args is
         * the MSP-based frame and goes into the msp (1st) argument. */
        fault_capture((uint32_t)hardfault_args, __get_PSP(), EXEC_RETURN, NULL);
    }
#endif

    while (true)
    {
    }
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
