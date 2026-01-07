/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fsl_common.h"
#include "fwk_fault_handlers_common.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

#if ((__CORTEX_M == 33) || (__CORTEX_M == 3))
void HardFault_Handler(void)
{
    __asm volatile(
        "movs r0, #4\t\n"
        "mov  r1, lr\t\n"
        "tst  r0, r1\t\n" /* Check EXC_RETURN[2] */
        "beq 1f\t\n"
        "mrs r0, psp\t\n"
        "ldr r1,=HardFaultHandler\t\n" /* Specific implementation for host and nbu */
        "bx r1\t\n"
        "1:mrs r0,msp\t\n"
        "ldr r1,=HardFaultHandler\t\n"
        "bx r1\t\n");
}

/* NMI handler is reimplemented by gcov port layer in the SDK */
#if !defined(GCOV_DO_COVERAGE)
void NMI_Handler(void)
{
    __asm volatile("  b HardFault_Handler \n");
}
#endif

void MemManage_Handler(void)
{
    __asm volatile("  b HardFault_Handler \n");
}

void BusFault_Handler(void)
{
    __asm volatile(" b HardFault_Handler \n");
}

void UsageFault_Handler(void)
{
    __asm volatile(" b HardFault_Handler \n");
}

void DebugMon_Handler(void)
{
#if defined(gFaultHandlerCoredumpEnabled_d) && (gFaultHandlerCoredumpEnabled_d > 0U)
    extern void __tx_DBGHandler(void);
    __tx_DBGHandler();
#endif
}

#endif

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
