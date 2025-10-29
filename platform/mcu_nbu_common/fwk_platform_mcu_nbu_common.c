/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fwk_platform_mcu_nbu_common.h"

#if !gPlatformTstmr32Bit_d
#include "fsl_tstmr.h"
#endif

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

uint64_t PLATFORM_TSTMR_ReadTimeStamp(TSTMR_Type *base)
{
#if gPlatformTstmr32Bit_d
    /* LOW register can be read separately, however each read triggers the HW latch of the HIGH register */
    return (uint64_t)base->L;
#else
    return TSTMR_ReadTimeStamp(base);
#endif
}

/*!
 * \brief Compute number of ticks between 2 timestamps expressed in number of TSTMR ticks
 *
 * \param [in] timestamp0 start timestamp.
 * \param [in] timestamp1 end timestamp.
 *
 * \return uint64_t number of TSTMR ticks
 *
 */
uint64_t PLATFORM_GetTstmrDeltaTicks(uint64_t timestamp0, uint64_t timestamp1)
{
    uint64_t delta_ticks;

    timestamp0 &= PLATFORM_TSTMR_MAX_VAL; /* sanitize arguments */
    timestamp1 &= PLATFORM_TSTMR_MAX_VAL;

    if (timestamp1 >= timestamp0)
    {
        delta_ticks = timestamp1 - timestamp0;
    }
    else
    {
        /* In case the 56-bit counter has wrapped */
        delta_ticks = PLATFORM_TSTMR_MAX_VAL - timestamp0 + timestamp1 + 1ULL;
    }
    return delta_ticks;
}
