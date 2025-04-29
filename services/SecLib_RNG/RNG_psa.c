/*! *********************************************************************************
 * Copyright 2025 NXP
 *
 * \file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */
#include "RNG_Interface.h"
#include "fwk_platform_crypto.h"
#include "entropy_poll.h"
#include "mbedtls/entropy.h"
#include "mbedtls/psa_util.h"

/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */

#define mPRNG_NoOfBits_c  (256U)
#define mPRNG_NoOfBytes_c (mPRNG_NoOfBits_c / 8U)

typedef struct rng_ctx_t
{
    bool_t mRngCtxInitialized;
} RNG_context_t;

/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */

static RNG_context_t rng_ctx = {
    .mRngCtxInitialized = FALSE,
};

/*! *********************************************************************************
*************************************************************************************
* Public prototypes
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
 * \brief  Initialize the RNG Software Module
 *         Please call SecLib_Init() before calling this function to make sure
 *         RNG hardware is correctly initialized.
 *
 * \return  Status of the RNG initialization procedure.
 *
 ********************************************************************************** */
int RNG_Init(void)
{
    int result = gRngInternalError_d;

    (void)PLATFORM_InitCrypto();

    do
    {
        if (rng_ctx.mRngCtxInitialized == TRUE)
        {
            result = gRngSuccess_d;
            break;
        }

        psa_status_t status = psa_crypto_init();
        if (status != PSA_SUCCESS)
        {
            break;
        }
        rng_ctx.mRngCtxInitialized = TRUE;

        result = gRngSuccess_d;
    } while (0 != 0);

    return result;
}

int RNG_ReInit(void)
{
    int result = gRngSuccess_d;
    if (PLATFORM_ResetCrypto() != 0)
    {
        result = gRngInternalError_d;
    }
    return result;
}

/*! *********************************************************************************
 * \brief  DeInitialize the RNG module.
 *         Resets the RNG context variables. Only used for test purposes.
 *
 * \return none
 *
 ********************************************************************************** */
void RNG_DeInit(void)
{
    rng_ctx.mRngCtxInitialized = FALSE;
}

/*! *********************************************************************************
 * \brief  Generates a 32-bit statistically random number if the hardware is enable
 *        else a PRNG number will be generated
 *         No random number will be generated if the RNG was not initialized
 *         or an error occurs.
 *
 * \param[out]  pRandomNo  Pointer to location where the value will be stored
 *
 ********************************************************************************** */
int RNG_GetTrueRandomNumber(uint32_t *pRandomNo)
{
    int status = gRngInternalError_d;
    do
    {
        if (pRandomNo == NULL)
        {
            status = gRngBadArguments_d;
            break;
        }

        if (rng_ctx.mRngCtxInitialized != TRUE)
        {
            status = gRngNotInitialized_d;
            break;
        }

        if (psa_generate_random((uint8_t *)pRandomNo, sizeof(uint32_t)) == PSA_SUCCESS)
        {
            status = gRngSuccess_d;
        }
    } while (0 != 0);
    return status;
}

/*! *********************************************************************************
 * \brief  Generates a bit pseudo-random number up to 256 bits. The PRNG algorithm used depend
 *         platform's cryptographic hardware and software capabilities.
 *
 * \param[out]  pOut  Pointer to the output buffer (max 32 bytes)
 * \param[in]   outBytes  The number of bytes to be copied (1-32)
 * \param[in]   pSeed  Ignored - please set to NULL
 *              This parameter is ignored because it is no longer needed.
 *              The PRNG is automatically seeded from the true random source.
 *              The length of the seed if present is 32 bytes.
 *
 * \return  The number of bytes copied OR
 *          -3 if the PRNG was not initialized OR
 *          -2 if 0 bytes were requested OR
 *          -1 if an error occurred
 *
 ********************************************************************************** */
int RNG_GetPseudoRandomData(uint8_t *pOut, uint8_t outBytes, uint8_t *pSeed)
{
    int ret;
    NOT_USED(pSeed);
    do
    {
        if (pOut == NULL || outBytes == 0u)
        {
            ret = gRngBadArguments_d;
            break;
        }

        if (rng_ctx.mRngCtxInitialized != TRUE)
        {
            ret = gRngNotInitialized_d;
            break;
        }
        else
        {
            if (outBytes > mPRNG_NoOfBytes_c)
            {
                outBytes = mPRNG_NoOfBytes_c;
            }

            if (psa_generate_random((uint8_t *)pOut, outBytes) == PSA_SUCCESS)
            {
                ret = outBytes;
            }
        }
    } while (false);
    return ret;
}

int RNG_SetSeed(void)
{
    return 0;
}

int RNG_NotifyReseedNeeded(void)
{
    return 0;
}

bool_t RNG_IsReseedNeeded(void)
{
    return FALSE;
}

int RNG_SetExternalSeed(uint8_t *external_seed)
{
    (void)external_seed;
    return 1; /* External seeding is not supported on mbedtls */
}

#ifdef PSA_CRYPTO_DRIVER_ELE_S2XX
psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t *context,
                                             uint8_t                               *output,
                                             size_t                                 output_size,
                                             size_t                                *output_length)
{
    size_t gen_length;
    int    status;
    do
    {
        status = mbedtls_hardware_poll((void *)context, output, output_size, &gen_length);
        if (status != PSA_SUCCESS)
        {
            break;
        }
        output_size -= gen_length;
        *output_length += gen_length;

    } while (output_size > 0U);

    return status;
}
#endif

/********************************** EOF ***************************************/
