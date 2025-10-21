/*!
 * Copyright 2025 NXP
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

#ifdef __cplusplus
}
#endif

#endif /* _PLATFORM_DBG_H_ */
