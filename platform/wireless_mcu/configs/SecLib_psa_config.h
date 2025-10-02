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

#include "fwk_platform_definitions.h"
#include "mcux_mbedtls_accelerator_config.h"
#include "fsl_sss_config_elemu.h"

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
#define MBEDTLS_PSA_ACCEL_ALG_ECDH                       1
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_AES                   1
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1
#define MBEDTLS_PSA_ACCEL_ECC_MONTGOMERY_255             1
#define MBEDTLS_PSA_ACCEL_ECC_SECP_R1_256                1

#define PSA_WANT_ALG_SHA_256        1
#define PSA_WANT_ALG_SHA_512        1
#define PSA_WANT_ALG_ECB_NO_PADDING 1
#define PSA_WANT_ALG_CCM            1
#define PSA_WANT_ALG_GCM            1
#define PSA_WANT_ALG_CMAC           1
#define PSA_WANT_ALG_ECDH           1

#define PSA_WANT_KEY_TYPE_AES                   1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE 1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_EXPORT   1

#define PSA_WANT_ECC_MONTGOMERY_255 1
#define PSA_WANT_ECC_SECP_R1_256    1

#if defined(PSA_CRYPTO_DRIVER_ELE_S2XX) && !defined(ELE_FEATURE_MAC_MULTIPART)
#undef MBEDTLS_PSA_ACCEL_ALG_CMAC /* psa crypto drivers for s200 do not support partial operations with CMAC update on \
                                     KW45 */
#endif

#endif /* PSA_CRYPTO_CONFIG_H */