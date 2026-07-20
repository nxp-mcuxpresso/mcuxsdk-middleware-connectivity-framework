/* -------------------------------------------------------------------------- */
/*                           Copyright 2025-2026 NXP                          */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_platform.h"
#include "fwk_platform_ics.h"

#if defined(gPlatformNbuDebugGpioDAccessEnabled_d) && (gPlatformNbuDebugGpioDAccessEnabled_d > 0)
#include "fsl_trdc.h"
#endif

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Private functions                              */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int PLATFORM_GetRadioIdleDuration32K(void)
{
    /* On this platform, Radio NBU is located on separated power domain so no impact on main core */
    return PLATFORM_RADIO_IDLE_FOREVER;
}

/*!
 * \brief Get 32kHz timestamp from 32kHz counter
 *
 * \return uint64_t number ticks since start of counter.
 *
 */
uint64_t PLATFORM_Get32KTimeStamp(void)
{
    /* 32kHz TSTMR not instantiated on KW45/MCXW71 */
    return 0ULL;
}

/*!
 * \brief Compute number of microseconds between 2 timestamps expressed in number of TSTM 32kHz ticks
 *
 * \param [in] timestamp0 start timestamp from which duration is assessed.
 * \param [in] timestamp1 end timestamp till which duration is assessed.
 *
 * \return uint64_t number of microseconds
 *
 */
uint64_t PLATFORM_Get32KTimeStampDeltaUs(uint64_t timestamp0, uint64_t timestamp1)
{
    NOT_USED(timestamp0);
    NOT_USED(timestamp1);
    /* 32kHz TSTMR not instantiated on KW45/MCXW71 */
    return 0ULL;
}

/*
 * Tell NBU the address in RAM where to write its next activity timestamp.
 * Not applicable on KW45/MCXW71.
 */
int PLATFORM_SetNbuSharedCtxAddress(void)
{
    return -1;
}

int PLATFORM_InitNbuSpecific(void)
{
#if defined(gPlatformNbuDebugGpioDAccessEnabled_d) && (gPlatformNbuDebugGpioDAccessEnabled_d == 1)
    /* Init TRDC for NBU - Allow NBU to access GPIOD*/
    trdc_non_processor_domain_assignment_t domainAssignment;
    TRDC_SetDacGlobalValid(TRDC);
    TRDC_GetDefaultNonProcessorDomainAssignment(&domainAssignment);
    domainAssignment.privilegeAttr = (uint8_t)kTRDC_ForcePrivilege;
    TRDC_SetNonProcessorDomainAssignment(TRDC, (uint8_t)kTRDC_MasterRadioNBU, &domainAssignment);
#endif
    return 0;
}
