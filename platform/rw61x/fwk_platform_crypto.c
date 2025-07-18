/* -------------------------------------------------------------------------- */
/*                           Copyright 2024 NXP                               */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include <stdarg.h>
#include "fwk_platform_crypto.h"
#if !defined(gSecLibUsePsa_d) || (gSecLibUsePsa_d == 0)
#include "els_pkc_mbedtls.h"
#else
#include "mcux_psa_els_pkc_common_init.h"
#endif
/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
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

int PLATFORM_InitCrypto(void)
{
    (void)CRYPTO_InitHardware();
    return 0;
}

int PLATFORM_TerminateCrypto(void)
{
    return 0;
}

int PLATFORM_ResetCrypto(void)
{
    PLATFORM_InitCrypto();
    return 0;
}
