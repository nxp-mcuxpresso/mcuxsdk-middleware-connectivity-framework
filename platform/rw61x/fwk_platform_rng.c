/* -------------------------------------------------------------------------- */
/*                             Copyright 2024 NXP                             */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_platform_rng.h"

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

int PLATFORM_RequestRngSeed(void)
{
    /* Not implemented */
    return 1;
}
int PLATFORM_SendRngSeed(uint8_t *seed, uint16_t seed_size)
{
    /* Not implemented */
    (void)seed_size;
    return 1;
}
