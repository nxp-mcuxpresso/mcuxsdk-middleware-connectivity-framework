/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWK_CONFIG_H_
#define _FWK_CONFIG_H_

#include "fwk_platform_definitions.h"

/*! *********************************************************************************
 *   RNG Module Configuration
 ********************************************************************************** */
/*! Set priority of TRNG interrupt (must be lower than link layer interrupts).
 *   See system_MCXW23*.* files for more details on the system requirements regarding interrupt priorities. */
#define gRngIsrPrio_c ((NVIC_DEFAULT_PRIORITY) << (8U - (uint8_t)__NVIC_PRIO_BITS))

#endif /* _FWK_CONFIG_H_ */
