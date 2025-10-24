/* -------------------------------------------------------------------------- */
/*                           Copyright 2025 NXP                               */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fsl_device_registers.h"
#include "fwk_platform_dbg.h"
#include "fwk_debug_struct.h"
#include "fsl_assert.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
/* Bit definitions */
#define NBU2HOST_ERROR_INDICATION (1U << 0) /* Bit 0: Fault occurred */
#define NBU2HOST_WARNING_SHIFT    (2U)      /* Bits 2-4: Warning count field */
#define NBU2HOST_WARNING_MASK     (0x1CU)   /* 3-bit mask for warning count */
#define NBU2HOST_WARNING_MAX      (NBU2HOST_WARNING_MASK >> NBU2HOST_WARNING_SHIFT)
#define NBU2HOST_WARNING(x)       (((uint32_t)(((uint32_t)(x)) << NBU2HOST_WARNING_SHIFT)) & NBU2HOST_WARNING_MASK)

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
static uint8_t warning_count = 0U;

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int PLATFORM_Nbu2HostFaultIndication(void)
{
    XCVR_MISC->RADIO2HOST |= NBU2HOST_ERROR_INDICATION;

    return 0;
}

int PLATFORM_Nbu2HostWarningIndication(void)
{
    uint32_t xcvr_radio_to_host;

    /* Handle 3-bit overflow by wrapping around */
    if (warning_count >= (uint8_t)NBU2HOST_WARNING_MAX)
    {
        warning_count = 0U;
    }

    warning_count++;

    /* Clear warning count bits and set new warning count value */
    xcvr_radio_to_host = XCVR_MISC->RADIO2HOST;
    xcvr_radio_to_host &= ~((uint32_t)NBU2HOST_WARNING_MASK);
    xcvr_radio_to_host |= NBU2HOST_WARNING(warning_count);
    XCVR_MISC->RADIO2HOST = xcvr_radio_to_host;

    return 0;
}

void PLATFORM_NbuRaiseFault(void)
{
    /* This is a fatal assert, trig a fault to save debug context
     * and inform the host using the fault handler.
     */
    *((volatile uint32_t *)0xFFA55E27) = 0xFFA55E27; // Invalid memory access
}

int fsl_assert_hook(const char *failedExpr, const char *file, int line)
{
    NBUDBG_SetAssertContext(failedExpr, file, line);
    PLATFORM_NbuRaiseFault();
    return 0;
}
