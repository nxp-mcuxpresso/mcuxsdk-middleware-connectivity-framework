/*!
 * Copyright 2025-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PLATFORM_DBG_H_
#define _PLATFORM_DBG_H_

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/*!
 * \brief  Checks whether the NBU is faulting.
 *
 * \return true if NBU faults, false otherwise.
 */
bool PLATFORM_IsNbuFaultSet(void);

/**
 * \brief Check if NBU warning is set and get the warning count
 * \param pCount Pointer to store the warning count (can be NULL if count not needed)
 *
 * \return true if warning is set (count > 0), false otherwise
 */
bool PLATFORM_IsNbuWarningSet(uint8_t *pCount);

/*!
 * \brief Force the NBU to raise a fault on demand (debug only).
 *
 * Sends an MCMGR remote application event to the NBU. The NBU handler raises a
 * fault so its fault handler can capture the debug context and stream a
 * coredump to the host. Useful to obtain a coredump when the NBU is suspected
 * to be stalled.
 *
 * \note Requires MCMGR_REMOTE_APP_EVENT_COUNT >= 2. If this is not met the
 *       function does nothing and returns an error.
 * \note This relies on the NBU still being able to service the MCMGR (IMU/MU)
 *       interrupt. A fully hard-locked NBU with interrupts disabled cannot be
 *       broken into this way.
 *
 * \return 0 on success, negative value on error or if the feature is unavailable.
 */
int PLATFORM_TryForceNbuFault(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLATFORM_DBG_H_ */
