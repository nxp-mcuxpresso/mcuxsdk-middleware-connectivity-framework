/*!
 * Copyright 2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file SecLib_psa_config.h
 * \brief Config file used as MBEDTLS_CONFIG_FILE for SecLib PSA port
 *
 */

#ifndef PSA_CRYPTO_CONFIG_H
#define PSA_CRYPTO_CONFIG_H

#include "mcux_mbedtls_accelerator_config.h"

#define MBEDTLS_PSA_CRYPTO_C            1
#define MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG 1
#define MBEDTLS_PSA_CRYPTO_CONFIG       1

#define MBEDTLS_PLATFORM_C      1
#define MBEDTLS_PLATFORM_MEMORY 1

#ifdef MBEDTLS_PLATFORM_MEMORY
#include "fsl_component_mem_manager.h"
#endif

#define MBEDTLS_PLATFORM_CALLOC_MACRO (void *)MEM_CallocAlt
#define MBEDTLS_PLATFORM_FREE_MACRO   MEM_BufferFree

#define MBEDTLS_PSA_ACCEL_ALG_SHA_256                    1
#define MBEDTLS_PSA_ACCEL_ALG_SHA_512                    1
#define MBEDTLS_PSA_ACCEL_ALG_ECB_NO_PADDING             1
#define MBEDTLS_PSA_ACCEL_ALG_CCM                        1
#define MBEDTLS_PSA_ACCEL_ALG_GCM                        1
#define MBEDTLS_PSA_ACCEL_ALG_CMAC                       1
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_AES                   1
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1

#define PSA_WANT_ALG_SHA_256        1
#define PSA_WANT_ALG_SHA_512        1
#define PSA_WANT_ALG_ECB_NO_PADDING 1
#define PSA_WANT_ALG_CCM            1
#define PSA_WANT_ALG_GCM            1
#define PSA_WANT_ALG_CMAC           1

#define PSA_WANT_KEY_TYPE_AES                   1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_EXPORT   1
#define PSA_WANT_KEY_TYPE_HMAC                  1

#define PSA_WANT_ECC_MONTGOMERY_255 1
#define PSA_WANT_ECC_SECP_R1_256    1

/*
 * The following symbols extend and deprecate the legacy
 * PSA_WANT_KEY_TYPE_xxx_KEY_PAIR ones. They include the usage of that key in
 * the name's suffix. "_USE" is the most generic and it can be used to describe
 * a generic suport, whereas other ones add more features on top of that and
 * they are more specific.
 */

#endif /* PSA_CRYPTO_CONFIG_H */