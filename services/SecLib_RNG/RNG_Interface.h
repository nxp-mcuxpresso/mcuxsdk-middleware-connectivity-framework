/*! *********************************************************************************
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2024 NXP
 * All rights reserved.
 *
 * \file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef RNG_INTERFACE_H
#define RNG_INTERFACE_H

#include "EmbeddedTypes.h"
#include <stddef.h>

/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */
#define gRngSuccess_d        (0x00)
#define gRngInternalError_d  (-1)
#define gRngBadArguments_d   (-2)
#define gRngNotInitialized_d (-3)
#define gRngReseedPending_d  (-4)

#ifndef gRngMaxRequests_d
#define gRngMaxRequests_d (100000)
#endif

#ifndef gRngIsrPrio_c
#define gRngIsrPrio_c (0x80)
#endif

/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */

/*! Generic PRNG function pointer type definition. */
typedef int (*fpRngPrng_t)(void *data, unsigned char *output, size_t len);

/*! Generic RNG Entropy function pointer type definition. */
typedef int (*fpRngEntropy_t)(void *data, unsigned char *output, size_t len);

/*! *********************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Public function prototypes
*************************************************************************************
********************************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif
/*! *********************************************************************************
 * \brief  Initialize the RNG Software Module
 *         Please call SecLib_Init() before calling this function to make sure
 *         RNG hardware is correctly initialized.
 *
 * \return  Status of the RNG initialization procedure.
 *
 ********************************************************************************** */
int RNG_Init(void);

/*! *********************************************************************************
 * \brief  Reinitialize the RNG module post-wakeup.
 *         May do nothing, action is dependent on platform.
 *
 * \return  gRngSuccess_d if successful, gRngInternalError_d if operation fails.
 *
 * Note: Failure only possible for specific platforms.
 *
 ********************************************************************************** */
int RNG_ReInit(void);

/*! *********************************************************************************
 * \brief  DeInitialize the RNG module.
 *         Resets the RNG context variables. Only used for test purposes.
 *
 * \return none
 *
 ********************************************************************************** */
void RNG_DeInit(void);

/*! *********************************************************************************
 * \brief  Generates a 32-bit statistically random number
 *         No random number will be generated if the RNG was not initialized
 *         or an error occurs.
 *
 * \param[out]  pRandomNo  Pointer to location where the value will be stored
 *
 ********************************************************************************** */
int RNG_GetTrueRandomNumber(uint32_t *pRandomNo);

/*! *********************************************************************************
 * \brief  Generates an 256 bit (or 160 bit) pseudo-random number. The PRNG algorithm used
 *         depends on the platform's cryptographic hardware and software capabilities.
 *         Please check the implementation and/or the output of this function at runtime
 *         to see how many bytes does the PRNG produce (depending on the implementation).
 *
 * \param[out]  pOut  Pointer to the output buffer (max 32 bytes or max 20 bytes)
 * \param[in]   outBytes  The number of bytes to be copied (1-32 or 1-20 depending on the implementation)
 * \param[in]   pSeed  used to inject 'entropy' - may be NULL if not used.
 *              If NULL, The PRNG is automatically seeded from the true random source, otherwise the pSeed is combined
 *              in order to inject entropy. The length of the seed may be 32 bytes or 20 bytes (depending on the implementation).
 *
  * \return If positive ( > 0) the number of random bytes produced,
 *          If negative denotes an error:
 *              gRngBadArguments_d if pOut is NULL or outBytes is 0 OR
 *              gRngNotInitialized_d if RNG_Init was not priorly called OR
 *              gRngInternalError_d in case of driver error OR
 *              gRngReseedPending_d if new call happened whereas reseed request
 *              was pending already .

 ********************************************************************************** */
int RNG_GetPseudoRandomData(uint8_t *pOut, uint8_t outBytes, uint8_t *pSeed);

/*! *********************************************************************************
 * \brief  Returns a pointer to the general PRNG function
 *         Call RNG_SetPseudoRandomNoSeed() before calling this function.
 *
 * \return  Function pointer to the general PRNG function or NULL if it
 *          was not seeded.
 *
 ********************************************************************************** */
fpRngPrng_t RNG_GetPrngFunc(void);

/*! *********************************************************************************
 * \brief  Returns a pointer to the general PRNG context
 *         Call RNG_SetPseudoRandomNoSeed() before calling this function.
 *
 * \return  Function pointer to the general PRNG context or NULL if it
 *          was not initialized correctly.
 *
 ********************************************************************************** */
void *RNG_GetPrngContext(void);

/*! *********************************************************************************
 * \brief  Initialize seed for the PRNG algorithm with the TRNG available on the platform.
 *         If this function is called again, the PRNG will be reseeded. *
 *
 ********************************************************************************** */
int RNG_SetSeed(void);

/*! *********************************************************************************
 * \brief  Initialize seed for the PRNG algorithm with an external seed.
 *         If this function is called again, the PRNG will be reseeded.
 *
 *  \param[in]  external_seed  Pointer to 32 byte array used to set seed.
 *
 ********************************************************************************** */
int RNG_SetExternalSeed(uint8_t *external_seed);

/*! *********************************************************************************
 * \brief  Warn the module that a reseed is needed for the PRNG.

 * PRNG will still work but quality of the number generated will decrease RNG_SetSeed()
 * or RNG_SetExternalSeed() will need to be called asynchronously from another task.
 *
 ********************************************************************************** */
int RNG_NotifyReseedNeeded(void);

/*! *********************************************************************************
 * \brief  Tell if the PRNG needs to be reseeded.
 *
 * \return  PRNG needs to be reseeded : TRUE or FALSE
 *
 ********************************************************************************** */
bool_t RNG_IsReseedNeeded(void);

#ifdef __cplusplus
}
#endif
#endif /* RNG_INTERFACE_H */
