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

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Informs the Host that the NBU is encountering a fault.
 *
 * \return 0 if success
 */
int PLATFORM_Nbu2HostFaultIndication(void);

/**
 * \brief Informs the Host that the NBU has a warning.
 *
 * \return 0 on success
 */
int PLATFORM_Nbu2HostWarningIndication(void);

/**
 * \brief Raise a fault.
 *        Can be used in case of fatal assert to capture the debug context with the fault handler.
 */
void PLATFORM_NbuRaiseFault(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLATFORM_DBG_H_ */
