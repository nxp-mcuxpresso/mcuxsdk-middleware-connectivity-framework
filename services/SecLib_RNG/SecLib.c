/*! *********************************************************************************
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2018,2020-2025 NXP
 * All rights reserved.
 *
 * \file
 *
 * This is the source file for the security module.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */
#include "FunctionLib.h"
#include "SecLib.h"
#include "fsl_device_registers.h"
#include "fsl_os_abstraction.h"
#include "fsl_component_mem_manager.h"
#include "CryptoLibSW.h"

/* header file to be included after fsl_device_registers.h as it potentially overwrites some feature MACROs
    (FSL_FEATURE_SOC_LTC_COUNT) */
#include "fwk_config.h"

#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))

#ifndef FREESCALE_MMCAU
#define FREESCALE_MMCAU 1
#endif

#ifndef FREESCALE_MMCAU_SHA
#define FREESCALE_MMCAU_SHA 1
#endif

#include "cau_api.h"
#endif /* FSL_FEATURE_SOC_MMCAU_COUNT */

#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0))
#include "fsl_ltc.h"
#endif

#ifdef FSL_FEATURE_SOC_AES_HW
#ifdef CPU_QN908X
#include "aes_reg_access.h"
#include "fsl_aes.h"
#include "AesManager.h"
#endif /* CPU_QN908X */
#endif /* FSL_FEATURE_SOC_AES_HW */

/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */

/* AES constants */
#define AES128        128U
#define AES128_ROUNDS 10U

#define AES192        192U
#define AES192_ROUNDS 12U

#define AES256        256U
#define AES256_ROUNDS 14U

#if ((defined(USE_RTOS) && (USE_RTOS > 0)) &&                                       \
     ((defined FSL_FEATURE_SOC_LTC_COUNT && (FSL_FEATURE_SOC_LTC_COUNT > 0)) ||     \
      (defined FSL_FEATURE_SOC_MMCAU_COUNT && (FSL_FEATURE_SOC_MMCAU_COUNT > 0)) || \
      (defined FSL_FEATURE_SOC_AES_HW && (FSL_FEATURE_SOC_AES_HW > 0))))
#define gSecLibUseMutex_c TRUE
#else
#define gSecLibUseMutex_c FALSE
#endif

extern osa_status_t SecLibMutexCreate(void);
extern osa_status_t SecLibMutexLock(void);
extern osa_status_t SecLibMutexUnlock(void);

#if gSecLibUseMutex_c
#define SECLIB_MUTEX_LOCK()   (void)SecLibMutexLock()
#define SECLIB_MUTEX_UNLOCK() (void)SecLibMutexUnlock()
#else
#define SECLIB_MUTEX_LOCK()
#define SECLIB_MUTEX_UNLOCK()
#endif
/*
 * __DSP_PRESENT is defined in the device specific file, however avoid use of __DSP_PRESENT to avoid
 * a dependency with SDK.
 * It is likely to be present on all Core M33, Core M7 and Core M4 devices.
 * Nonetheless RW61x was designed without ARM DSP extension, in which case avoid defining
 * gSecLibUseDspExtension_d.
 * gSecLibUseDspExtension_d follows __DSP_PRESENT definition unless overridden to 0
 */

#ifndef gSecLibUseDspExtension_d
#define gSecLibUseDspExtension_d 0
#endif

#ifndef RAISE_ERROR
#define RAISE_ERROR(x, code) \
    {                        \
        (x) = (code);        \
        break;               \
    }
#endif

#define AES_BLOCK_ALIGN_MASK (0x0000000fUL)
/* Compute number of whole AES block bytes */
#define AES_WHOLE_BLOCK_BYTES(_LEN_) ((uint32_t)(_LEN_) & ~AES_BLOCK_ALIGN_MASK)
/* Compute number of residual bytes constituting a partial AES block */
#define AES_PARTIAL_BLOCK_BYTES(_LEN_) ((uint32_t)(_LEN_)&AES_BLOCK_ALIGN_MASK)

/*! *********************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
********************************************************************************** */

static void SHA1_hash_n(uint8_t *pData, uint32_t nBlk, uint32_t *pHash);

/*! *********************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
********************************************************************************** */
typedef union _uuint128_tag
{
    uint8_t  u8[16];
    uint64_t u64[2];
} uuint128_t;

#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES == 1))
uint8_t AES128ECB_Enc_Id;
uint8_t AES128ECB_Dec_Id;
uint8_t AES128ECBB_Enc_Id;
uint8_t AES128ECBB_Dec_Id;

uint8_t AES128CTR_Enc_Id;
uint8_t AES128CTR_Dec_Id;

uint8_t AES128CMAC_Id;
#endif

#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
typedef struct mmcauAesContext_tag
{
    uint8_t keyExpansion[44 * 4];
    uint8_t alignedIn[AES_BLOCK_SIZE];
    uint8_t alignedOut[AES_BLOCK_SIZE];
} mmcauAesContext_t;

/*! MMCAU AES Context Buffer for both AES Encrypt and Decrypt operations.*/
mmcauAesContext_t mmcauAesCtx;
#endif /* FSL_FEATURE_SOC_MMCAU_COUNT */

#if gSecLibUseMutex_c
/*! Mutex used to protect the AES Context when an RTOS is used. */
static OSA_MUTEX_HANDLE_DEFINE(mSecLibMutexId);
#endif /* USE_RTOS */

typedef struct sha1Context_tag
{
    uint32_t hash[SHA1_HASH_SIZE / sizeof(uint32_t)];
    uint8_t  buffer[SHA1_BLOCK_SIZE];
    uint32_t totalBytes;
    uint8_t  bytes;
} sha1Context_t;

typedef struct sha256Context_tag
{
    uint32_t hash[SHA256_HASH_SIZE / sizeof(uint32_t)];
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    uint32_t totalBytes;
    uint8_t  bytes;
} sha256Context_t;

typedef struct HMAC_SHA256_context_tag
{
    sha256Context_t shaCtx;
    uint8_t         pad[SHA256_BLOCK_SIZE];
} HMAC_SHA256_context_t;

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
/*! Callback used to offload Security steps onto application message queue. When it is not set the
 * multiplication is done using SecLib means */
extern secLibCallback_t pfSecLibMultCallback;

secLibCallback_t pfSecLibMultCallback = NULL;

#if (gSecLibUseBleDebugKeys_d == 1)
/*! Bluetooth LE debug keys as specified in section 2.3.5.6.1 vol. 3, part H of the Bluetooth Core specification version 5.4 */
static const ecp256KeyPair_t mBleDebugKeyPair = {
    .public_key.components_8bit.x = {0x20, 0xb0, 0x03, 0xd2, 0xf2, 0x97, 0xbe, 0x2c, 0x5e, 0x2c, 0x83,
                                     0xa7, 0xe9, 0xf9, 0xa5, 0xb9, 0xef, 0xf4, 0x91, 0x11, 0xac, 0xf4,
                                     0xfd, 0xdb, 0xcc, 0x03, 0x01, 0x48, 0x0e, 0x35, 0x9d, 0xe6},
    .public_key.components_8bit.y = {0xdc, 0x80, 0x9c, 0x49, 0x65, 0x2a, 0xeb, 0x6d, 0x63, 0x32, 0x9a,
                                     0xbf, 0x5a, 0x52, 0x15, 0x5c, 0x76, 0x63, 0x45, 0xc2, 0x8f, 0xed,
                                     0x30, 0x24, 0x74, 0x1c, 0x8e, 0xd0, 0x15, 0x89, 0xd2, 0x8b},
    .private_key.raw_8bit         = {0x3f, 0x49, 0xf6, 0xd4, 0xa3, 0xc5, 0x5f, 0x38, 0x74, 0xc9, 0xb3,
                                     0xe3, 0xd2, 0x10, 0x3f, 0x50, 0x4a, 0xff, 0x60, 0x7b, 0xeb, 0x40,
                                     0xb7, 0x99, 0x58, 0x99, 0xb8, 0xa6, 0xcd, 0x3c, 0x1a, 0xbd}};
#endif /* gSecLibUseBleDebugKeys_d */

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

static void SHA1_hash_n(uint8_t *pData, uint32_t nBlk, uint32_t *pHash);
static void SHA256_hash_n(const uint8_t *pData, uint32_t nBlk, uint32_t *pHash);
static void AES_128_CMAC_Generate_Subkey(const uint8_t *key, uint8_t *K1, uint8_t *K2);
static void SecLib_LeftShiftOneBit(uint8_t *input, uint8_t *output);
static void SecLib_Xor128(const uint8_t *a, const uint8_t *b, uint8_t *out);

static uint8_t SecLib_Padding(const uint8_t *lastb, uint8_t pad_block[AES_BLOCK_SIZE], uint8_t length);
static uint8_t SecLib_DePadding(const uint8_t pad_block[AES_BLOCK_SIZE]);

#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT == 1U))
#else
static void AES_128_IncrementCounter(uint8_t *ctr);
#endif

#ifdef FSL_FEATURE_SOC_AES_HW
static void AES_128_ECB_Enc_HW(AES_param_t *ECB_p);
static void AES_128_ECB_Dec_HW(AES_param_t *ECB_p);
static void AES_128_ECB_Block_Enc_HW(AES_param_t *ECBB_p);
static void AES_128_ECB_Block_Dec_HW(AES_param_t *ECBB_p);

static void AES_128_CTR_Enc_HW(AES_param_t *CTR_p);
static void AES_128_CTR_Dec_HW(AES_param_t *CTR_p);

static void AES_128_CMAC_HW(AES_param_t *CMAC_p);
#endif

/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************** */

#if gSecLibUseDspExtension_d
static bool ECP256_LePointValid(const ecp256Point_t *P)
{
    ecp256Point_t tmp;
    ECP256_PointCopy_and_change_endianness(tmp.raw, P->raw);
    return ECP256_PointValid(&tmp);
}
#else

extern bool_t EcP256_IsPointOnCurve(const uint32_t *X, const uint32_t *Y);

static bool ECP256_LePointValid(const ecp256Point_t *P)
{
    return EcP256_IsPointOnCurve((const uint32_t *)&P->components_32bit.x[0],
                                 (const uint32_t *)&P->components_32bit.y[0]);
}
#endif

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

osa_status_t SecLibMutexCreate(void)
{
    osa_status_t st = KOSA_StatusSuccess;
#if gSecLibUseMutex_c
    static bool seclib_mutex_created = false;
    if (!seclib_mutex_created)
    {
        /*! Initialize the SecLib Mutex here. If not already done by RNG module */
        st = OSA_MutexCreate((osa_mutex_handle_t)mSecLibMutexId);

        if (KOSA_StatusSuccess != st)
        {
            assert(0);
        }
        else
        {
            seclib_mutex_created = true;
        }
    }
#endif
    return st;
}

osa_status_t SecLibMutexLock(void)
{
#if gSecLibUseMutex_c
    return OSA_MutexLock((osa_mutex_handle_t)mSecLibMutexId, osaWaitForever_c);
#else
    return KOSA_StatusSuccess;
#endif
}

osa_status_t SecLibMutexUnlock(void)
{
#if gSecLibUseMutex_c
    return OSA_MutexUnlock((osa_mutex_handle_t)mSecLibMutexId);
#else
    return KOSA_StatusSuccess;
#endif
}

/*! *********************************************************************************
 * \brief  This function performs initialization of the cryptographic HW acceleration.
 *
 ********************************************************************************** */
void SecLib_Init(void)
{
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;
#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0))
        LTC_Init(LTC0);
#elif (defined FSL_FEATURE_SOC_AES_HW && (FSL_FEATURE_SOC_AES_HW > 0))
#ifdef CPU_QN908X
#if USE_TASK_FOR_HW_AES
        AESM_Initialize();
#endif /* CPU_QN908X */
#endif /* FSL_FEATURE_SOC_AES_HW   */
#endif /* FSL_FEATURE_SOC_LTC_COUNT */

#if gSecLibUseMutex_c
        /*! Initialize the MMCAU AES Context Buffer Mutex here. */
        (void)SecLibMutexCreate();
#endif
    }
}

/*! *********************************************************************************
 * \brief  This function performs initialization of the cryptographic HW acceleration.
 *
 ********************************************************************************** */
void SecLib_ReInit(void)
{
    /* Nothing to do for Software implementation */
}

/*! *********************************************************************************
 * \brief  This function will allow reinitizialize the cryptographic HW acceleration
 * next time we need it, typically after lowpower mode.
 *
 ********************************************************************************** */
void SecLib_DeInit(void)
{
    /* Nothing to do for Software implementation */
}

/*! *********************************************************************************
 * \brief  This function performs initialization of the callback used to offload
 * elliptic curve multiplication.
 *
 * \param[in]  pfCallback Pointer to the function used to handle multiplication.
 *
 ********************************************************************************** */
void SecLib_SetExternalMultiplicationCb(secLibCallback_t pfCallback)
{
#if (defined(gSecLibUseDspExtension_d) && (gSecLibUseDspExtension_d > 0))
    /* In case the SecLib is using dsp extension the API from the Ultrafast library will be used, no need to offload
     * elliptic curve multiplication */
    return;
#else
    pfSecLibMultCallback = pfCallback;
#endif
}

/*! *********************************************************************************
 * \brief  This function performs calls the multiplication Callback.
 *
 * \param[in]  pMsg Pointer to the data used in multiplication.
 *
 ********************************************************************************** */
void SecLib_ExecMultiplicationCb(computeDhKeyParam_t *pMsg)
{
    if (pfSecLibMultCallback != NULL)
    {
        pfSecLibMultCallback(pMsg);
    }
}

/*! *********************************************************************************
 * \brief  This function performs AES-128 encryption on a 16-byte block.
 *
 * \param[in]  pInput Pointer to the location of the 16-byte plain text block.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte ciphered output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
void AES_128_Encrypt(const uint8_t *pInput, const uint8_t *pKey, uint8_t *pOutput)
{
#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    mmcauAesContext_t *pCtx = &mmcauAesCtx;
    uint8_t           *pIn;
    uint8_t           *pOut;
#endif

    SECLIB_MUTEX_LOCK();

#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    /* Check if pKey is 4 bytes aligned */
    if ((uint32_t)pKey & 0x00000003u)
    {
        FLib_MemCpy(pCtx->alignedIn, (uint8_t *)pKey, AES_BLOCK_SIZE);
        pIn = pCtx->alignedIn;
    }
    else
    {
        pIn = (uint8_t *)pKey;
    }

    /* Expand Key */
    mmcau_aes_set_key(pIn, AES128, pCtx->keyExpansion);

    /* Check if pData is 4 bytes aligned */
    if ((uint32_t)pInput & 0x00000003u)
    {
        FLib_MemCpy(pCtx->alignedIn, (uint8_t *)pInput, AES_BLOCK_SIZE);
        pIn = pCtx->alignedIn;
    }
    else
    {
        pIn = (uint8_t *)pInput;
    }

    /* Check if pReturnData is 4 bytes aligned */
    if ((uint32_t)pOutput & 0x00000003u)
    {
        pOut = pCtx->alignedOut;
    }
    else
    {
        pOut = pOutput;
    }

    /* Encrypt data */
    mmcau_aes_encrypt(pIn, pCtx->keyExpansion, AES128_ROUNDS, pOut);

    if (pOut == pCtx->alignedOut)
    {
        FLib_MemCpy(pOutput, pCtx->alignedOut, AES_BLOCK_SIZE);
    }

#elif (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0))
    (void)LTC_AES_EncryptEcb(LTC0, pInput, pOutput, AES_BLOCK_SIZE, pKey, AES_BLOCK_SIZE);

#elif (defined FSL_FEATURE_SOC_AES_HW && (FSL_FEATURE_SOC_AES_HW > 0))
    aes_enc_status_t hw_ase_status_flag;

    do
    {
        while (*(uint8_t *)(0x04000168u + 76u) == true)
        {
            OSA_TaskYield();
        }
        __disable_irq();
        hw_ase_status_flag = AES_128_Encrypt_HW(pInput, pKey, pOutput);
        __enable_irq();
    } while (hw_ase_status_flag == HW_AES_Previous_Enc_on_going);

#else
    sw_Aes128(pInput, pKey, 1, pOutput);
#endif

    SECLIB_MUTEX_UNLOCK();
}

/*! *********************************************************************************
 * \brief  This function performs AES-128 decryption on a 16-byte block.
 *
 * \param[in]  pInput Pointer to the location of the 16-byte ciphered text block.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte plain text output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
void AES_128_Decrypt(const uint8_t *pInput, const uint8_t *pKey, uint8_t *pOutput)
{
#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    mmcauAesContext_t *pCtx = &mmcauAesCtx;
    uint8_t           *pIn;
    uint8_t           *pOut;

    SECLIB_MUTEX_LOCK();
    /* Check if pKey is 4 bytes aligned */
    if ((uint32_t)pKey & 0x00000003u)
    {
        FLib_MemCpy(pCtx->alignedIn, (uint8_t *)pKey, AES_BLOCK_SIZE);
        pIn = pCtx->alignedIn;
    }
    else
    {
        pIn = (uint8_t *)pKey;
    }

    /* Expand Key */
    mmcau_aes_set_key(pIn, AES128, pCtx->keyExpansion);

    /* Check if pData is 4 bytes aligned */
    if ((uint32_t)pInput & 0x00000003u)
    {
        FLib_MemCpy(pCtx->alignedIn, (uint8_t *)pInput, AES_BLOCK_SIZE);
        pIn = pCtx->alignedIn;
    }
    else
    {
        pIn = (uint8_t *)pInput;
    }

    /* Check if pReturnData is 4 bytes aligned */
    if ((uint32_t)pOutput & 0x00000003u)
    {
        pOut = pCtx->alignedOut;
    }
    else
    {
        pOut = pOutput;
    }

    /* Decrypt data */
    mmcau_aes_decrypt(pIn, pCtx->keyExpansion, AES128_ROUNDS, pOut);

    if (pOut == pCtx->alignedOut)
    {
        FLib_MemCpy(pOutput, pCtx->alignedOut, AES_BLOCK_SIZE);
    }
    SECLIB_MUTEX_UNLOCK();
#elif (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0))
    SECLIB_MUTEX_LOCK();
    (void)LTC_AES_DecryptEcb(LTC0, pInput, pOutput, AES_BLOCK_SIZE, pKey, AES_BLOCK_SIZE, kLTC_EncryptKey);
    SECLIB_MUTEX_UNLOCK();

#elif (defined FSL_FEATURE_SOC_AES_HW && (FSL_FEATURE_SOC_AES_HW > 0))

    aes_enc_status_t hw_ase_status_flag;
    SECLIB_MUTEX_LOCK();
    do
    {
        while (*(uint8_t *)(0x04000168u + 76u) == true)
        {
            OSA_TaskYield();
        }
        __disable_irq();
        hw_ase_status_flag = AES_128_Decrypt_HW(pInput, pKey, pOutput);
        __enable_irq();

    } while (hw_ase_status_flag == HW_AES_Previous_Enc_on_going);

    SECLIB_MUTEX_UNLOCK();
#else
    sw_Aes128(pInput, pKey, 0, pOutput);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-ECB encryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
void AES_128_ECB_Encrypt(const uint8_t *pInput, uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput)
{
#ifdef FSL_FEATURE_SOC_AES_HW /* HW AES */
    AES_param_t pAES;

    pAES.CTR_counter = NULL;
    pAES.Key         = pKey;
    pAES.Len         = inputLen;
    pAES.pCipher     = pOutput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pInput;
    pAES.Blocks      = 0;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128ECB_Enc_Id, gAESMGR_ECB_Enc_c);
    AESM_SetParam(AES128ECB_Enc_Id, pAES, AES_128_ECB_Enc_HW);
    AESM_Start(AES128ECB_Enc_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_ECB_Enc_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */
#else  /* SW AES */
    uint8_t tempBuffIn[AES_BLOCK_SIZE]  = {0};
    uint8_t tempBuffOut[AES_BLOCK_SIZE] = {0};

    /* If remaining data bigger than one AES block size */
    while (inputLen > AES_BLOCK_SIZE)
    {
        AES_128_Encrypt(pInput, pKey, pOutput);
        pInput += AES_BLOCK_SIZE;
        pOutput += AES_BLOCK_SIZE;
        inputLen -= AES_BLOCK_SIZE;
    }

    /* If remaining data is smaller then one AES block size */
    FLib_MemCpy(tempBuffIn, pInput, inputLen);
    AES_128_Encrypt(tempBuffIn, pKey, tempBuffOut);
    FLib_MemCpy(pOutput, tempBuffOut, inputLen);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-ECB decryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
#ifdef FSL_FEATURE_SOC_AES_HW
void AES_128_ECB_Decrypt(const uint8_t *pInput, uint32_t inputLen, uint8_t *pKey, uint8_t *pOutput)
{
    AES_param_t pAES;

    pAES.CTR_counter = NULL;
    pAES.Key         = pKey;
    pAES.Len         = inputLen;
    pAES.pCipher     = pInput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pOutput;
    pAES.Blocks      = 0;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128ECB_Dec_Id, gAESMGR_ECB_Dec_c);
    AESM_SetParam(AES128ECB_Dec_Id, pAES, AES_128_ECB_Dec_HW);
    AESM_Start(AES128ECB_Dec_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_ECB_Dec_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */
}
#endif /* FSL_FEATURE_SOC_AES_HW */

/*! *********************************************************************************
 * \brief  This function performs AES-128-ECB encryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  numBlocks Input message number of 16-byte blocks.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
void AES_128_ECB_Block_Encrypt(const uint8_t *pInput, uint32_t numBlocks, const uint8_t *pKey, uint8_t *pOutput)
{
#ifdef FSL_FEATURE_SOC_AES_HW /* HW AES */
    AES_param_t pAES;

    pAES.CTR_counter = NULL;
    pAES.Key         = (uint8_t *)pKey;
    pAES.Len         = numBlocks * 16;
    pAES.pCipher     = pOutput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pInput;
    pAES.Blocks      = numBlocks;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128ECBB_Enc_Id, gAESMGR_ECB_Block_Enc_c);
    AESM_SetParam(AES128ECBB_Enc_Id, pAES, AES_128_ECB_Block_Enc_HW);
    AESM_Start(AES128ECBB_Enc_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_ECB_Block_Enc_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */

#else  /* SW AES */
    while (numBlocks > 0u)
    {
        AES_128_Encrypt(pInput, pKey, pOutput);
        numBlocks--;
        pInput += AES_BLOCK_SIZE;
        pOutput += AES_BLOCK_SIZE;
    }
#endif /* FSL_FEATURE_SOC_AES_HW */
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-ECB decryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  numBlocks Input message number of 16-byte blocks.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
#ifdef FSL_FEATURE_SOC_AES_HW
void AES_128_ECB_Block_Decrypt(uint8_t *pInput, uint32_t numBlocks, const uint8_t *pKey, uint8_t *pOutput)
{
    AES_param_t pAES;

    pAES.CTR_counter = NULL;
    pAES.Key         = (uint8_t *)pKey;
    pAES.Len         = numBlocks * 16;
    pAES.pCipher     = pInput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pOutput;
    pAES.Blocks      = numBlocks;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128ECBB_Dec_Id, gAESMGR_ECB_Block_Dec_c);
    AESM_SetParam(AES128ECBB_Dec_Id, pAES, AES_128_ECB_Block_Dec_HW);
    AESM_Start(AES128ECBB_Dec_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_ECB_Block_Dec_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */
}
#endif /* FSL_FEATURE_SOC_AES_HW */

/*! *********************************************************************************
 * \brief  This function performs AES-128-CBC encryption on a message block.
 *
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes - must be a multiple of AES_BLOCK_SIZE
 *
 * \param[in, out]  pInitVector Pointer to the location of the 128-bit initialization vector.
 *                 On exit the IV content is updated with ciphered output to be injected as next block IV.
 *                 Because IV is modifiable, it cannot be RO (const).
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 * \return : gSecSuccess_c if no error,
 *           gSecBadArgument_c in case of bad arguments,
 *           gSecError_c in case of internal error.
 *
 ********************************************************************************** */
secResultType_t AES_128_CBC_Encrypt(
    const uint8_t *pInput, uint32_t inputLen, uint8_t *pInitVector, const uint8_t *pKey, uint8_t *pOutput)
{
    secResultType_t ret;

    do
    {
        if ((pInput == NULL) || (pInitVector == NULL) || (pKey == NULL) || (pOutput == NULL) ||
            /* If the input length is not a non zero multiple of AES 128 block size,  return */
            (inputLen < AES_BLOCK_SIZE) || (AES_PARTIAL_BLOCK_BYTES(inputLen) != 0U))
        {
            ret = gSecBadArgument_c;
            break;
        }

        /* LTC is capable of performing CBC operation natively */
#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0))
        status_t st;
        SECLIB_MUTEX_LOCK();
        st = LTC_AES_EncryptCbc(LTC0, pInput, pOutput, inputLen, pInitVector, pKey, AES_128_KEY_BYTE_LEN);
        SECLIB_MUTEX_UNLOCK();
        if (st != kStatus_Success)
        {
            ret = gSecError_c;
            break;
        }
        /* Update IV with last ciphered block to be injected at next call */
        /* Note that inputLen is greater than or equal to AES_BLOCK_SIZE, otherwise would have exited
           with gSecBadArgument_c, so difference cannot be negative */
        FLib_MemCpy(pInitVector, &pOutput[inputLen - AES_BLOCK_SIZE], AES_BLOCK_SIZE);
#else
        uint8_t tempBuffIn[AES_BLOCK_SIZE] = {0};

        FLib_MemCpy(tempBuffIn, pInitVector, AES_BLOCK_SIZE);
        /* If remaining data is bigger than one AES block size */
        while (inputLen > 0u)
        {
            SecLib_XorN(tempBuffIn, pInput, AES_BLOCK_SIZE);
            AES_128_Encrypt(tempBuffIn, pKey, pOutput);
            FLib_MemCpy(tempBuffIn, pOutput, AES_BLOCK_SIZE);
            pInput += AES_BLOCK_SIZE;
            pOutput += AES_BLOCK_SIZE;
            inputLen -= AES_BLOCK_SIZE;
        }
        FLib_MemCpy(pInitVector, tempBuffIn, AES_BLOCK_SIZE);
#endif
        ret = gSecSuccess_c;
    } while (false);

    return ret;
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CBC decryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input ciphered message.
 *
 * \param[in]  inputLen Input message length in bytes - must be a multiple of AES_BLOCK_SIZE.
 *
 * \param[in, out]  pInitVector Pointer to the location of the 128-bit initialization vector.
 *                 On exit the IV content is updated with ciphered output to be injected as next block IV.
 *                 Because IV is modifiable, it cannot be RO (const).
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the plain text output.
 *
 * \return : gSecSuccess_c if no error,
 *           gSecBadArgument_c in case of bad arguments,
 *           gSecError_c in case of internal error.
 *
 ********************************************************************************** */
secResultType_t AES_128_CBC_Decrypt(
    const uint8_t *pInput, uint32_t inputLen, uint8_t *pInitVector, const uint8_t *pKey, uint8_t *pOutput)
{
    secResultType_t ret;

    do
    {
        if ((pInput == NULL) || (pInitVector == NULL) || (pKey == NULL) || (pOutput == NULL) ||
            /* If the input length is not a non zero multiple of AES 128 block size,  return */
            (inputLen < AES_BLOCK_SIZE) || (AES_PARTIAL_BLOCK_BYTES(inputLen) != 0U))
        {
            ret = gSecBadArgument_c;
            break;
        }

#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0)) && \
    (defined(LTC_KEY_REGISTER_READABLE) && (LTC_KEY_REGISTER_READABLE == 1))
        status_t st;
        SECLIB_MUTEX_LOCK();
        st = LTC_AES_DecryptCbc(LTC0, pInput, pOutput, inputLen, pInitVector, pKey, AES_128_KEY_BYTE_LEN,
                                kLTC_DecryptKey);
        SECLIB_MUTEX_UNLOCK();
        if (st != kStatus_Success)
        {
            ret = gSecError_c;
            break;
        }
        /* Update IV with last ciphered block to be injected at next call */
        /* Note that inputLen is greater than or equal to AES_BLOCK_SIZE, otherwise would have exited
           with gSecBadArgument_c, so difference cannot be negative */
        FLib_MemCpy(pInitVector, &pInput[inputLen - AES_BLOCK_SIZE], AES_BLOCK_SIZE);
#else
        uint8_t temp[AES_BLOCK_SIZE] = {0u};

        while (inputLen > 0u)
        {
            FLib_MemCpy(temp, pInput, AES_BLOCK_SIZE);
            AES_128_Decrypt(pInput, pKey, pOutput);
            SecLib_XorN(pOutput, pInitVector, AES_BLOCK_SIZE);

            FLib_MemCpy(pInitVector, temp, AES_BLOCK_SIZE);

            pInput += AES_BLOCK_SIZE;
            pOutput += AES_BLOCK_SIZE;
            inputLen -= AES_BLOCK_SIZE;
        }
#endif
        ret = gSecSuccess_c;

    } while (false);

    return ret;
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CBC encryption on a message block after
 *         padding until AES block completion.
 *
 * Padding scheme is ISO/IEC 7816-4: one 80h byte (1 bit), followed by as many 00h as
 * required to fill a 128 bit block. Note that if the message length is a multiple of
 * AES block size already, another block is appended to the original message.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes - no specific constraint.
 *
 *  IMPORTANT: User must make sure output buffer has at least inputLen + 16 bytes size.
 *  This constraint does not apply to input buffer (any longer).
 *
 * \param[in, out]  pInitVector Pointer to the location of the 128-bit initialization vector.
 *                 On exit the IV content is updated with ciphered output to be injected as next block IV.
 *                 Because it is modifiable it cannot be RO (const).
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 * \return size of output message after padding is appended.
 *
 ********************************************************************************** */
uint32_t AES_128_CBC_Encrypt_And_Pad(
    uint8_t *pInput, uint32_t inputLen, uint8_t *pInitVector, const uint8_t *pKey, uint8_t *pOutput)
{
    uint32_t roundedLen = 0u;

    do
    {
        uint8_t last_blk_msg_sz;
        uint8_t last_block[AES_BLOCK_SIZE]; /* Buffer used to generate last block containing padding */
        /* compute new length */
        roundedLen      = AES_WHOLE_BLOCK_BYTES(inputLen);
        last_blk_msg_sz = (uint8_t)(inputLen - roundedLen);
        /* Perform AES-CBC operation on whole AES blocks */
        if (AES_128_CBC_Encrypt(pInput, roundedLen, pInitVector, pKey, pOutput) != gSecSuccess_c)
        {
            roundedLen = 0u;
            break;
        }
        pInput += roundedLen;
        pOutput += roundedLen;
        /* There may be a remainder modulus 16 : copy it to last_block on stack */
        /* then add padding so as to fill the last_block array */
        if (SecLib_Padding(pInput, last_block, last_blk_msg_sz) == 0u)
        {
            roundedLen = 0u;
            break;
        }
        if (AES_128_CBC_Encrypt(last_block, AES_BLOCK_SIZE, pInitVector, pKey, pOutput) != gSecSuccess_c)
        {
            roundedLen = 0u;
            break;
        }
        roundedLen += AES_BLOCK_SIZE;
    } while (false);

    return roundedLen;
}

/*! *********************************************************************************
 * \brief  This function performs AES_128_CBC_Decrypt_And_Depad decryption on a message.
 *
 * \param[in]  pInput Pointer to the location of the input ciphered message.
 *
 * \param[in]  inputLen Input message length in bytes must be a multiple of AES block size
 *
 * \param[in]  pInitVector Pointer to the location of the 128-bit initialization vector.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out] pOutput Pointer to the location to store the plain text output.
 *
 * \return size of output buffer (after depadding the 0x80 [0x00 .. ]. padding sequence)
 *
 ********************************************************************************** */
uint32_t AES_128_CBC_Decrypt_And_Depad(
    const uint8_t *pInput, uint32_t inputLen, uint8_t *pInitVector, const uint8_t *pKey, uint8_t *pOutput)
{
    uint32_t newLen = 0uL;

    if (inputLen > 0u)
    {
        if (AES_128_CBC_Decrypt(pInput, inputLen, pInitVector, pKey, pOutput) == gSecSuccess_c)
        {
            uint8_t padding_len;
            /* If we are here inputLen is a non 0 multiple of AES_BLOCK_SIZE, otherwise AES_128_CBC_Decrypt would have
            returned an error.
            Yet the test below is to prevent a false MISRA error detection.
            */
            if ((inputLen >= AES_BLOCK_SIZE) && (AES_PARTIAL_BLOCK_BYTES(inputLen) == 0u))
            {
                uint8_t *p_last_block = &pOutput[inputLen - AES_BLOCK_SIZE];
                padding_len           = SecLib_DePadding(p_last_block);
                if ((padding_len > 0u) && (padding_len <= AES_BLOCK_SIZE))
                {
                    /* Safe: inputLen is a multiple of AES_BLOCK_SIZE and >= AES_BLOCK_SIZE,
                    padding_len is in [1..AES_BLOCK_SIZE], so subtraction cannot underflow */
                    newLen = inputLen - (uint32_t)padding_len;
                }
            }
        }
    }
    /* coverity [return_overflow:FALSE] see above */
    return newLen;
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CTR encryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes.
 *
 * \param[in]  pCounter Pointer to the location of the 128-bit counter.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 ********************************************************************************** */
void AES_128_CTR(const uint8_t *pInput, uint32_t inputLen, uint8_t *pCounter, const uint8_t *pKey, uint8_t *pOutput)
{
#ifdef FSL_FEATURE_SOC_AES_HW /* HW AES */
    AES_param_t pAES;

    pAES.CTR_counter = pCounter;
    pAES.Key         = pKey;
    pAES.Len         = inputLen;
    pAES.pCipher     = pOutput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pInput;
    pAES.Blocks      = 0;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128CTR_Enc_Id, gAESMGR_CTR_Enc_c);
    AESM_SetParam(AES128CTR_Enc_Id, pAES, AES_128_CTR_Enc_HW);
    AESM_Start(AES128CTR_Enc_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_CTR_Enc_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */

#else  /* SW AES */

#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT > 0))
    SECLIB_MUTEX_LOCK();
    (void)LTC_AES_EncryptCtr(LTC0, pInput, pOutput, inputLen, pCounter, pKey, AES_BLOCK_SIZE, (void *)NULL,
                             (void *)NULL);
    SECLIB_MUTEX_UNLOCK();

#else
    uint8_t tempBuffIn[AES_BLOCK_SIZE] = {0};
    uint8_t encrCtr[AES_BLOCK_SIZE]    = {0};

    /* If remaining data bigger than one AES block size */
    while (inputLen > AES_BLOCK_SIZE)
    {
        FLib_MemCpy(tempBuffIn, pInput, AES_BLOCK_SIZE);
        AES_128_Encrypt(pCounter, pKey, encrCtr);
        SecLib_XorN(tempBuffIn, encrCtr, AES_BLOCK_SIZE);
        FLib_MemCpy(pOutput, tempBuffIn, AES_BLOCK_SIZE);
        pInput += AES_BLOCK_SIZE;
        pOutput += AES_BLOCK_SIZE;
        inputLen -= AES_BLOCK_SIZE;
        AES_128_IncrementCounter(pCounter);
    }

    /* If remaining data is smaller then one AES block size  */
    FLib_MemCpy(tempBuffIn, pInput, inputLen);
    AES_128_Encrypt(pCounter, pKey, encrCtr);
    SecLib_XorN(tempBuffIn, encrCtr, AES_BLOCK_SIZE);
    FLib_MemCpy(pOutput, tempBuffIn, inputLen);
    AES_128_IncrementCounter(pCounter);
#endif /* FSL_FEATURE_SOC_LTC_COUNT */
#endif /* FSL_FEATURE_SOC_AES_HW */
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CTR decryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes.
 *
 * \param[in]  pCounter Pointer to the location of the 128-bit counter.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 ********************************************************************************** */
#ifdef FSL_FEATURE_SOC_AES_HW
void AES_128_CTR_Decrypt(
    const uint8_t *pInput, uint32_t inputLen, uint8_t *pCounter, const uint8_t *pKey, uint8_t *pOutput)
{
    AES_param_t pAES;

    pAES.CTR_counter = pCounter;
    pAES.Key         = pKey;
    pAES.Len         = inputLen;
    pAES.pCipher     = pInput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pOutput;
    pAES.Blocks      = 0;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128CTR_Dec_Id, gAESMGR_CTR_Dec_c);
    AESM_SetParam(AES128CTR_Dec_Id, pAES, AES_128_CTR_Dec_HW);
    AESM_Start(AES128CTR_Dec_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_CTR_Dec_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */
}
#endif /* FSL_FEATURE_SOC_AES_HW */

#if gSecLibAesOfbEnable_d
/*! *********************************************************************************
 * \brief  This function performs AES-128-OFB encryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Input message length in bytes.
 *
 * \param[in]  pInitVector Pointer to the location of the 128-bit initialization vector.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the ciphered output.
 *
 ********************************************************************************** */
void AES_128_OFB(
    const uint8_t *pInput, uint32_t inputLen, const uint8_t *pInitVector, const uint8_t *pKey, uint8_t *pOutput)
{
    uint8_t tempBuffIn[AES_BLOCK_SIZE]  = {0};
    uint8_t tempBuffOut[AES_BLOCK_SIZE] = {0};

    if (pInitVector != (void *)NULL)
    {
        FLib_MemCpy(tempBuffIn, pInitVector, AES_BLOCK_SIZE);
    }

    /* If remaining data is bigger than one AES block size */
    while (inputLen > AES_BLOCK_SIZE)
    {
        AES_128_Encrypt(tempBuffIn, pKey, tempBuffOut);
        FLib_MemCpy(tempBuffIn, tempBuffOut, AES_BLOCK_SIZE);
        SecLib_XorN(tempBuffOut, pInput, AES_BLOCK_SIZE);
        FLib_MemCpy(pOutput, tempBuffOut, AES_BLOCK_SIZE);
        pInput += AES_BLOCK_SIZE;
        pOutput += AES_BLOCK_SIZE;
        inputLen -= AES_BLOCK_SIZE;
    }

    /* If remaining data is smaller then one AES block size  */
    AES_128_Encrypt(tempBuffIn, pKey, tempBuffOut);
    SecLib_XorN(tempBuffOut, pInput, (uint8_t)(inputLen & 0xffu));
    FLib_MemCpy(pOutput, tempBuffOut, inputLen);
}
#endif /* gSecLibAesOfbEnable_d */

/*! *********************************************************************************
 * \brief  This function performs AES-128-CMAC on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Length of the input message in bytes. The input data must be provided MSB first.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key. The key must be provided MSB first.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte authentication code. The code will be generated MSB
 *first.
 *
 * \remarks This is public open source code! Terms of use must be checked before use!
 *
 ********************************************************************************** */
void AES_128_CMAC(const uint8_t *pInput, const uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput)
{
#ifdef FSL_FEATURE_SOC_AES_HW /* HW AES */
    AES_param_t pAES;

    pAES.CTR_counter = NULL;
    pAES.Key         = pKey;
    pAES.Len         = inputLen;
    pAES.pCipher     = pOutput;
    pAES.pInitVector = NULL;
    pAES.pPlain      = pInput;
    pAES.Blocks      = 0;
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_InitType(&AES128CMAC_Id, gAESMGR_CMAC_Enc_c);
    AESM_SetParam(AES128CMAC_Id, pAES, AES_128_CMAC_HW);
    AESM_Start(AES128CMAC_Id);
#else
    SECLIB_MUTEX_LOCK();
    AES_128_CMAC_HW(&pAES);
    SECLIB_MUTEX_UNLOCK();
#endif /* USE_TASK_FOR_HW_AES */

#else  /* SW AES */

    uint8_t X[16];
    uint8_t Y[16];
    uint8_t M_last[16] = {0};
    uint8_t padded[16] = {0};

    uint8_t K1[16] = {0};
    uint8_t K2[16] = {0};

    uint16_t n;
    uint32_t i;
    uint8_t  flag;
    uint8_t  residual_len;

    AES_128_CMAC_Generate_Subkey(pKey, K1, K2);

    n            = (uint16_t)((inputLen + (AES_BLOCK_SIZE - 1u)) / AES_BLOCK_SIZE); /* n is number of rounds */
    residual_len = (uint8_t)AES_PARTIAL_BLOCK_BYTES(inputLen);

    if (n == 0u)
    {
        n    = 1u;
        flag = 0u;
    }
    else
    {
        if (residual_len == 0u)
        { /* last block is a complete block */
            flag = 1u;
        }
        else
        { /* last block is not complete block */
            flag = 0u;
        }
    }

    /* Process the last block  - the last part the MSB first input data */
    if (flag > 0u)
    { /* last block is complete block */
        SecLib_Xor128(&pInput[AES_BLOCK_SIZE * (n - 1u)], K1, M_last);
    }
    else
    {
        (void)SecLib_Padding(&pInput[AES_BLOCK_SIZE * (n - 1u)], padded, residual_len);
        SecLib_Xor128(padded, K2, M_last);
    }

    for (i = 0u; i < 16u; i++)
    {
        X[i] = 0u;
    }

    for (i = 0u; i < (uint32_t)n - 1u; i++)
    {
        SecLib_Xor128(X, &pInput[AES_BLOCK_SIZE * i], Y); /* Y := Mi (+) X  */
        AES_128_Encrypt(Y, pKey, X);                      /* X := AES-128(KEY, Y) */
    }

    SecLib_Xor128(X, M_last, Y);
    AES_128_Encrypt(Y, pKey, X);

    for (i = 0u; i < 16u; i++)
    {
        pOutput[i] = X[i];
    }
#endif /* FSL_FEATURE_SOC_AES_HW */
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CMAC on a message block accepting input data
 *         which is in LSB first format and computing the authentication code starting from the end of the data.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Length of the input message in bytes. The input data must be provided LSB first.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key. The key must be provided MSB first.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte authentication code. The code will be generated MSB
 *first.
 *
 ********************************************************************************** */
void AES_128_CMAC_LsbFirstInput(const uint8_t *pInput, uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput)
{
    uint8_t X[16];
    uint8_t Y[16];
    uint8_t M_last[16]        = {0};
    uint8_t padded[16]        = {0};
    uint8_t reversedBlock[16] = {0};

    uint8_t K1[16] = {0};
    uint8_t K2[16] = {0};

    uint16_t n;
    uint32_t i;
    uint8_t  flag;
    uint8_t  residual_len;

    AES_128_CMAC_Generate_Subkey(pKey, K1, K2);

    n            = (uint16_t)(((inputLen + (AES_BLOCK_SIZE - 1u))) / AES_BLOCK_SIZE); /* n is number of rounds */
    residual_len = (uint8_t)AES_PARTIAL_BLOCK_BYTES(inputLen);

    if (n == 0u)
    {
        n    = 1u;
        flag = 0u;
    }
    else
    {
        if (residual_len == 0u) /* last block is a complete block */
        {
            flag = 1u;
        }
        else /* last block is not complete block */
        {
            flag = 0u;
        }
    }

    /* Process the last block  - the first part the LSB first input data */
    if (flag > 0u) /* last block is complete block */
    {
        FLib_MemCpyReverseOrder(reversedBlock, &pInput[0], AES_BLOCK_SIZE);
        SecLib_Xor128(reversedBlock, K1, M_last);
    }
    else
    {
        FLib_MemCpyReverseOrder(reversedBlock, &pInput[0], residual_len);
        (void)SecLib_Padding(reversedBlock, padded, residual_len);
        SecLib_Xor128(padded, K2, M_last);
    }

    for (i = 0u; i < 16u; i++)
    {
        X[i] = 0u;
    }

    for (i = 0u; i < (uint32_t)n - 1u; i++)
    {
        FLib_MemCpyReverseOrder(reversedBlock, &pInput[inputLen - AES_BLOCK_SIZE * (i + 1u)], AES_BLOCK_SIZE);
        SecLib_Xor128(X, reversedBlock, Y); /* Y := Mi (+) X  */
        AES_128_Encrypt(Y, pKey, X);        /* X := AES-128(KEY, Y) */
    }

    SecLib_Xor128(X, M_last, Y);
    AES_128_Encrypt(Y, pKey, X);

    for (i = 0u; i < 16u; i++)
    {
        pOutput[i] = X[i];
    }
}

/*! *********************************************************************************
 * \brief  This function performs AES 128 CMAC Pseudo-Random Function (AES-CMAC-PRF-128),
 *         according to rfc4615, on a message block.
 *
 * \details The AES-CMAC-PRF-128 algorithm behaves similar to teh AES CMAC 128 algorithm
 *          but removes 128 bit key size restriction.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Length of the input message in bytes.
 *
 * \param[in]  pVarKey Pointer to the location of the variable length key.
 *
 * \param[in]  varKeyLen Length of the input key in bytes
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte pseudo random variable.
 *
 ********************************************************************************** */
void AES_CMAC_PRF_128(
    const uint8_t *pInput, uint32_t inputLen, const uint8_t *pVarKey, uint32_t varKeyLen, uint8_t *pOutput)
{
    uint8_t        K[16];              /*!< Temporary key location to be used if the key length is not 16 bytes. */
    const uint8_t *pCmacKey = pVarKey; /*!<  Pointer to the key used by the CMAC operation which generates the
                                        *    output. */
    if (varKeyLen > 0u)
    {
        if (varKeyLen != 16u)
        {
            uint8_t K0[16] = {0x00u, 0x00,  0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                              0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
            /*! Perform AES 128 CMAC on the variable key if it has a length which
             *  is different from 16 bytes using a 128 bit key with all zeroes and
             *  set the CMAC key to point to the result. */
            AES_128_CMAC(pVarKey, varKeyLen, K0, K);
            pCmacKey = K;
        }

        /*! Perform the CMAC operation which generates the output using the local
         *  key pointer whcih will be set to the initial key or the generated one. */
        AES_128_CMAC(pInput, inputLen, pCmacKey, pOutput);
    }
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-EAX encryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Length of the input message in bytes.
 *
 * \param[in]  pNonce Pointer to the location of the nonce.
 *
 * \param[in]  nonceLen Nonce length in bytes.
 *
 * \param[in]  pHeader Pointer to the location of header.
 *
 * \param[in]  headerLen Header length in bytes.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte authentication code.
 *
 * \param[out]  pTag Pointer to the location to store the 128-bit tag.
 *
 ********************************************************************************** */
secResultType_t AES_128_EAX_Encrypt(const uint8_t *pInput,
                                    uint32_t       inputLen,
                                    const uint8_t *pNonce,
                                    uint32_t       nonceLen,
                                    const uint8_t *pHeader,
                                    uint8_t        headerLen,
                                    const uint8_t *pKey,
                                    uint8_t       *pOutput,
                                    uint8_t       *pTag)
{
    uint8_t        *buf;
    uint32_t        buf_len;
    uint8_t         nonce_mac[AES_BLOCK_SIZE] = {0};
    uint8_t         hdr_mac[AES_BLOCK_SIZE]   = {0};
    uint8_t         data_mac[AES_BLOCK_SIZE]  = {0};
    uint8_t         tempBuff[AES_BLOCK_SIZE]  = {0};
    uint32_t        i;
    secResultType_t status = gSecAllocError_c;

    if (nonceLen > inputLen)
    {
        buf_len = nonceLen;
    }
    else
    {
        buf_len = inputLen;
    }

    if (headerLen > buf_len)
    {
        buf_len = headerLen;
    }

    buf_len += 16u;

    buf = MEM_BufferAlloc(buf_len);

    if (buf != (void *)NULL)
    {
        FLib_MemSet(buf, 0u, 15u);

        buf[15] = 0u;
        FLib_MemCpy((buf + 16u), pNonce, nonceLen);
        AES_128_CMAC(buf, 16u + nonceLen, pKey, nonce_mac);

        buf[15] = 1u;
        FLib_MemCpy((buf + 16u), pHeader, headerLen);
        AES_128_CMAC(buf, 16u + (uint32_t)headerLen, pKey, hdr_mac);

        /* keep the original value of nonce_mac, because AES_128_CTR will increment it */
        FLib_MemCpy(tempBuff, nonce_mac, nonceLen);

        AES_128_CTR(pInput, inputLen, tempBuff, pKey, pOutput);

        buf[15] = 2u;
        FLib_MemCpy((buf + 16u), pOutput, inputLen);
        AES_128_CMAC(buf, 16u + inputLen, pKey, data_mac);

        for (i = 0u; i < AES_BLOCK_SIZE; i++)
        {
            pTag[i] = nonce_mac[i] ^ data_mac[i] ^ hdr_mac[i];
        }

        (void)MEM_BufferFree(buf);
        status = gSecSuccess_c;
    }
    return status;
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-EAX decryption on a message block.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Length of the input message in bytes.
 *
 * \param[in]  pNonce Pointer to the location of the nonce.
 *
 * \param[in]  nonceLen Nonce length in bytes.
 *
 * \param[in]  pHeader Pointer to the location of header.
 *
 * \param[in]  headerLen Header length in bytes.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte authentication code.
 *
 * \param[out]  pTag Pointer to the location to store the 128-bit tag.
 *
 ********************************************************************************** */
secResultType_t AES_128_EAX_Decrypt(uint8_t *pInput,
                                    uint32_t inputLen,
                                    uint8_t *pNonce,
                                    uint32_t nonceLen,
                                    uint8_t *pHeader,
                                    uint8_t  headerLen,
                                    uint8_t *pKey,
                                    uint8_t *pOutput,
                                    uint8_t *pTag)
{
    uint8_t        *buf;
    uint32_t        buf_len;
    uint8_t         nonce_mac[AES_BLOCK_SIZE] = {0};
    uint8_t         hdr_mac[AES_BLOCK_SIZE]   = {0};
    uint8_t         data_mac[AES_BLOCK_SIZE]  = {0};
    secResultType_t status                    = gSecAllocError_c;
    uint32_t        i;

    if (nonceLen > inputLen)
    {
        buf_len = nonceLen;
    }
    else
    {
        buf_len = inputLen;
    }

    if (headerLen > buf_len)
    {
        buf_len = headerLen;
    }

    buf_len += 16u;

    buf = MEM_BufferAlloc(buf_len);

    if (buf != (void *)NULL)
    {
        FLib_MemSet(buf, 0u, 15u);

        buf[15] = 0u;
        FLib_MemCpy((buf + 16u), pNonce, nonceLen);
        AES_128_CMAC(buf, 16u + nonceLen, pKey, nonce_mac);

        buf[15] = 1u;
        FLib_MemCpy((buf + 16u), pHeader, headerLen);
        AES_128_CMAC(buf, 16u + (uint32_t)headerLen, pKey, hdr_mac);

        buf[15] = 2u;
        FLib_MemCpy((buf + 16u), pInput, inputLen);
        AES_128_CMAC(buf, 16u + inputLen, pKey, data_mac);

        (void)MEM_BufferFree(buf);

        status = gSecSuccess_c;

        for (i = 0u; i < AES_BLOCK_SIZE; i++)
        {
            if (pTag[i] != (nonce_mac[i] ^ data_mac[i] ^ hdr_mac[i]))
            {
                status = gSecError_c;
                break;
            }
        }

        if (gSecSuccess_c == status)
        {
            AES_128_CTR(pInput, inputLen, nonce_mac, pKey, pOutput);
        }
    }

    return status;
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CCM on a message block.
 *
 * \param[in]  pInput       Pointer to the location of the input message (plaintext or cyphertext).
 *
 * \param[in]  inputLen     Length of the input plaintext in bytes when encrypting.
 *                          Length of the input cypertext without the MAC length when decrypting.
 *
 * \param[in]  pAuthData    Pointer to the additional authentication data.
 *
 * \param[in]  authDataLen  Length of additional authentication data.
 *
 * \param[in]  pNonce       Pointer to the Nonce.
 *
 * \param[in]  nonceSize    The size of the nonce (7-13).
 *
 * \param[in]  pKey         Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput     Pointer to the location to store the plaintext data when decrypting.
 *                          Pointer to the location to store the cyphertext data when encrypting.
 *
 * \param[out]  pCbcMac     Pointer to the location to store the Message Authentication Code (MAC) when encrypting.
 *                          Pointer to the location where the received MAC can be found when decrypting.
 *
 * \param[out]  macSize     The size of the MAC.
 *
 * \param[out]  flags       Select encrypt/decrypt operations (gSecLib_CCM_Encrypt_c, gSecLib_CCM_Decrypt_c)
 *
 * \return 0 if encryption/decryption was successfull; otherwise, error code for failed encryption/decryption
 *
 * \remarks At decryption, MIC fail is also signaled by returning a non-zero value.
 *
 ********************************************************************************** */
uint8_t AES_128_CCM(const uint8_t *pInput,
                    uint16_t       inputLen,
                    const uint8_t *pAuthData,
                    uint16_t       authDataLen,
                    const uint8_t *pNonce,
                    uint8_t        nonceSize,
                    const uint8_t *pKey,
                    uint8_t       *pOutput,
                    uint8_t       *pCbcMac,
                    uint8_t        macSize,
                    uint32_t       flags)
{
    uint8_t status;

    SECLIB_MUTEX_LOCK();

#if (defined(FSL_FEATURE_SOC_LTC_COUNT) && (FSL_FEATURE_SOC_LTC_COUNT == 1))
    if ((flags & gSecLib_CCM_Decrypt_c) == gSecLib_CCM_Decrypt_c)
    {
        status = (uint8_t)(LTC_AES_DecryptTagCcm(LTC0, pInput, pOutput, (uint32_t)inputLen, pNonce, (uint32_t)nonceSize,
                                                 pAuthData, (uint32_t)authDataLen, pKey, AES_BLOCK_SIZE, pCbcMac,
                                                 (uint32_t)macSize));
    }
    else
    {
        status = (uint8_t)(LTC_AES_EncryptTagCcm(LTC0, pInput, pOutput, (uint32_t)inputLen, pNonce, (uint32_t)nonceSize,
                                                 pAuthData, (uint32_t)authDataLen, pKey, AES_BLOCK_SIZE, pCbcMac,
                                                 (uint32_t)macSize));
    }

#else
    status = sw_AES128_CCM(pInput, inputLen, pAuthData, authDataLen, pNonce, nonceSize, pKey, pOutput, pCbcMac, macSize,
                           flags);
#endif

    SECLIB_MUTEX_UNLOCK();

    return status;
}

/*! *********************************************************************************
 * \brief  This function calculates XOR of individual byte pairs in two uint8_t arrays.
 *         pDst[i] := pDst[i] ^ pSrc[i] for i=0 to n-1
 *
 * \param[in, out]  pDst First byte array operand for XOR and destination byte array
 *
 * \param[in]  pSrc Second byte array operand for XOR
 *
 * \param[in]  n  Length of the byte arrays which will be XORed
 *
 ********************************************************************************** */
void SecLib_XorN(uint8_t *pDst, const uint8_t *pSrc, uint8_t n)
{
    while (n > 0u)
    {
        *pDst = *pDst ^ *pSrc;
        pDst  = pDst + 1u;
        pSrc  = pSrc + 1u;
        n--;
    }
}

/*! *********************************************************************************
 * \brief  This function allocates a memory buffer for a SHA1 context structure
 *
 * \return    Address of the SHA1 context buffer
 *            Deallocate using SHA1_FreeCtx()
 *
 ********************************************************************************** */
void *SHA1_AllocCtx(void)
{
    void *sha1Ctx = MEM_BufferAlloc(sizeof(sha1Context_t));

    return sha1Ctx;
}

/*! *********************************************************************************
 * \brief  This function deallocates the memory buffer for the SHA1 context structure
 *
 * \param [in]    pContext    Address of the SHA1 context buffer
 *
 ********************************************************************************** */
void SHA1_FreeCtx(void *pContext)
{
    (void)MEM_BufferFree(pContext);
}

/*! *********************************************************************************
 * \brief  This function clones a SHA1 context.
 *         Make sure the size of the allocated destination context buffer is appropriate.
 *
 * \param [in]    pDestCtx    Address of the destination SHA1 context
 * \param [in]    pSourceCtx  Address of the source SHA1 context
 *
 ********************************************************************************** */
void SHA1_CloneCtx(void *pDestCtx, void *pSourceCtx)
{
    FLib_MemCpy(pDestCtx, pSourceCtx, sizeof(sha1Context_t));
}

/*! *********************************************************************************
 * \brief  This function initializes the SHA1 context data
 *
 * \param [in]    pContext    Pointer to the SHA1 context data
 *                            Allocated using SHA1_AllocCtx()
 *
 ********************************************************************************** */
void SHA1_Init(void *pContext)
{
    sha1Context_t *context = (sha1Context_t *)pContext;

    context->bytes      = 0u;
    context->totalBytes = 0u;
#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    SECLIB_MUTEX_LOCK();
    (void)mmcau_sha1_initialize_output((const unsigned int *)context->hash);
    SECLIB_MUTEX_UNLOCK();
#else
    sw_sha1_initialize_output(context->hash);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs SHA1 on multiple bytes and updates the context data
 *
 * \param [in]    pContext    Pointer to the SHA1 context data
 *                            Allocated using SHA1_AllocCtx()
 * \param [in]    pData       Pointer to the input data
 * \param [in]    numBytes    Number of bytes to hash
 *
 ********************************************************************************** */
void SHA1_HashUpdate(void *pContext, uint8_t *pData, uint32_t numBytes)
{
    uint16_t       blocks;
    sha1Context_t *context = (sha1Context_t *)pContext;

    /* update total byte count */
    context->totalBytes += numBytes;
    /* Check if we have at least 1 SHA1 block */
    if (context->bytes + numBytes < SHA1_BLOCK_SIZE)
    {
        /* store bytes for later processing */
        FLib_MemCpy(&context->buffer[context->bytes], pData, numBytes);
        context->bytes += (uint8_t)(numBytes & 0xffu);
    }
    else
    {
        /* Check for bytes leftover from previous update */
        if (context->bytes > 0u)
        {
            uint8_t copyBytes = SHA1_BLOCK_SIZE - context->bytes;

            FLib_MemCpy(&context->buffer[context->bytes], pData, copyBytes);
            SHA1_hash_n(context->buffer, 1u, context->hash);
            pData += copyBytes;
            numBytes -= copyBytes;
            context->bytes = 0u;
        }
        /* Hash 64 bytes blocks */
        blocks = (uint16_t)(numBytes / SHA1_BLOCK_SIZE & 0xffffu);
        SHA1_hash_n(pData, blocks, context->hash);
        numBytes -= (uint32_t)blocks * SHA1_BLOCK_SIZE;
        pData += blocks * SHA1_BLOCK_SIZE;
        /* Check for remaining bytes */
        if (numBytes > 0u)
        {
            context->bytes = (uint8_t)(numBytes & 0xffu);
            FLib_MemCpy(context->buffer, pData, numBytes);
        }
    }
}

/*! *********************************************************************************
 * \brief  This function finalizes the SHA1 hash computation and clears the context data.
 *         The final hash value is stored at the provided output location.
 *
 * \param [in]       pContext    Pointer to the SHA1 context data
 *                               Allocated using SHA1_AllocCtx()
 * \param [out]      pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void SHA1_HashFinish(void *pContext, uint8_t *pOutput)
{
    uint32_t       i;
    uint32_t       temp;
    sha1Context_t *context = (sha1Context_t *)pContext;
    uint32_t       numBytes;

    /* update remaining bytes */
    numBytes = context->bytes;
    /* Add 1 bit (a 0x80 byte) after the message to begin padding */
    context->buffer[numBytes++] = 0x80u;
    /* Check for space to fit an 8 byte length field plus the 0x80 */
    if (context->bytes >= 56u)
    {
        /* Fill the rest of the chunk with zeros */
        FLib_MemSet(&context->buffer[numBytes], 0u, SHA1_BLOCK_SIZE - numBytes);
        SHA1_hash_n(context->buffer, 1u, context->hash);
        numBytes = 0u;
    }
    /* Fill the rest of the chunk with zeros */
    FLib_MemSet(&context->buffer[numBytes], 0u, SHA1_BLOCK_SIZE - numBytes);
    /* Append the total length of the message, in bits (bytes << 3u) */
    context->totalBytes <<= 3u;
    FLib_MemCpyReverseOrder(&context->buffer[60], &context->totalBytes, sizeof(uint32_t));
    SHA1_hash_n(context->buffer, 1u, context->hash);
    /* Convert to Big Endian */
    for (i = 0u; i < SHA1_HASH_SIZE / sizeof(uint32_t); i++)
    {
        temp = context->hash[i];
        FLib_MemCpyReverseOrder(&context->hash[i], &temp, sizeof(uint32_t));
    }

    /* Copy the generated hash to the indicated output location */
    FLib_MemCpy(pOutput, (uint8_t *)(context->hash), SHA1_HASH_SIZE);
}

/*! *********************************************************************************
 * \brief  This function performs all SHA1 steps on multiple bytes: initialize,
 *         update and finish.
 *         The final hash value is stored at the provided output location.
 *
 * \param [in]       pData       Pointer to the input data
 * \param [in]       numBytes    Number of bytes to hash
 * \param [out]      pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void SHA1_Hash(uint8_t *pData, uint32_t numBytes, uint8_t *pOutput)
{
    sha1Context_t context;

    SHA1_Init(&context);
    SHA1_HashUpdate(&context, pData, numBytes);
    SHA1_HashFinish(&context, pOutput);
}

/*! *********************************************************************************
 * \brief  This function allocates a memory buffer for a SHA256 context structure
 *
 * \return    Address of the SHA256 context buffer
 *            Deallocate using SHA256_FreeCtx()
 *
 ********************************************************************************** */
void *SHA256_AllocCtx(void)
{
    void *sha256Ctx = MEM_BufferAlloc(sizeof(sha256Context_t));

    return sha256Ctx;
}

/*! *********************************************************************************
* \brief  This function deallocates the memory buffer for the SHA256 context structure
*

* \param [in]    pContext    Address of the SHA256 context buffer
*
********************************************************************************** */
void SHA256_FreeCtx(void *pContext)
{
    (void)MEM_BufferFree(pContext);
}

/*! *********************************************************************************
 * \brief  This function clones SHA256 context.
 *         Make sure the size of the allocated destination context buffer is appropriate.
 *
 * \param [in]    pDestCtx    Address of the destination SHA256 context
 * \param [in]    pSourceCtx  Address of the source SHA256 context
 *
 ********************************************************************************** */
void SHA256_CloneCtx(void *pDestCtx, void *pSourceCtx)
{
    FLib_MemCpy(pDestCtx, pSourceCtx, sizeof(sha256Context_t));
}

/*! *********************************************************************************
 * \brief  This function initializes the SHA256 context data
 *
 * \param [in]    pContext    Pointer to the SHA256 context data
 *                            Allocated using SHA256_AllocCtx()
 *
 ********************************************************************************** */
void SHA256_Init(void *pContext)
{
    sha256Context_t *context = (sha256Context_t *)pContext;

    context->bytes      = 0u;
    context->totalBytes = 0u;
#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    SECLIB_MUTEX_LOCK();
    (void)mmcau_sha256_initialize_output((const unsigned int *)context->hash);
    SECLIB_MUTEX_UNLOCK();

#else
    sw_sha256_initialize_output(context->hash);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs SHA256 on multiple bytes and updates the context data
 *
 * \param [in]    pContext    Pointer to the SHA256 context data
 *                            Allocated using SHA256_AllocCtx()
 * \param [in]    pData       Pointer to the input data
 * \param [in]    numBytes    Number of bytes to hash
 *
 ********************************************************************************** */
void SHA256_HashUpdate(void *pContext, const uint8_t *pData, uint32_t numBytes)
{
    uint16_t         blocks;
    sha256Context_t *context = (sha256Context_t *)pContext;

    /* update total byte count */
    context->totalBytes += numBytes;
    /* Check if we have at least 1 SHA256 block */
    if (context->bytes + numBytes < SHA256_BLOCK_SIZE)
    {
        /* store bytes for later processing */
        FLib_MemCpy(&context->buffer[context->bytes], pData, numBytes);
        context->bytes += (uint8_t)(numBytes & 0xffu);
    }
    else
    {
        /* Check for bytes leftover from previous update */
        if (context->bytes > 0u)
        {
            uint8_t copyBytes = SHA256_BLOCK_SIZE - context->bytes;

            FLib_MemCpy(&context->buffer[context->bytes], pData, copyBytes);
            SHA256_hash_n(context->buffer, 1u, context->hash);
            pData += copyBytes;
            numBytes -= copyBytes;
            context->bytes = 0u;
        }
        /* Hash 64 bytes blocks */
        blocks = (uint16_t)(numBytes / SHA256_BLOCK_SIZE & 0xffffu);
        SHA256_hash_n(pData, blocks, context->hash);
        numBytes -= (uint32_t)blocks * SHA256_BLOCK_SIZE;
        pData += blocks * SHA256_BLOCK_SIZE;
        /* Check for remaining bytes */
        if (numBytes > 0u)
        {
            context->bytes = (uint8_t)(numBytes & 0xffu);
            FLib_MemCpy(context->buffer, pData, numBytes);
        }
    }
}

/*! *********************************************************************************
 * \brief  This function finalizes the SHA256 hash computation and clears the context data.
 *         The final hash value is stored at the provided output location.
 *
 * \param [in]       pContext    Pointer to the SHA256 context data
 *                               Allocated using SHA256_AllocCtx()
 * \param [out]      pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void SHA256_HashFinish(void *pContext, uint8_t *pOutput)
{
    uint32_t         i;
    uint32_t         temp;
    sha256Context_t *context = (sha256Context_t *)pContext;
    uint32_t         numBytes;

    /* update remaining bytes */
    numBytes = context->bytes;
    /* Add 1 bit (a 0x80 byte) after the message to begin padding */
    context->buffer[numBytes++] = 0x80u;
    /* Check for space to fit an 8 byte length field plus the 0x80 */
    if (context->bytes >= 56u)
    {
        /* Fill the rest of the chunk with zeros */
        FLib_MemSet(&context->buffer[numBytes], 0u, SHA256_BLOCK_SIZE - numBytes);
        SHA256_hash_n(context->buffer, 1u, context->hash);
        numBytes = 0u;
    }
    /* Fill the rest of the chunk with zeros */
    FLib_MemSet(&context->buffer[numBytes], 0, SHA256_BLOCK_SIZE - numBytes);
    /* Append the total length of the message(Big Endian), in bits (bytes << 3) */
    context->totalBytes <<= 3u;
    FLib_MemCpyReverseOrder(&context->buffer[60], &context->totalBytes, sizeof(uint32_t));
    SHA256_hash_n(context->buffer, 1u, context->hash);
    /* Convert to Big Endian */
    for (i = 0u; i < SHA256_HASH_SIZE / sizeof(uint32_t); i++)
    {
        temp = context->hash[i];
        FLib_MemCpyReverseOrder(&context->hash[i], &temp, sizeof(uint32_t));
    }

    /* Copy the generated hash to the indicated output location */
    FLib_MemCpy(pOutput, (uint8_t *)(context->hash), SHA256_HASH_SIZE);
}

/*! *********************************************************************************
 * \brief  This function performs all SHA256 steps on multiple bytes: initialize,
 *         update and finish.
 *         The final hash value is stored at the provided output location.
 *
 * \param [in]       pData       Pointer to the input data
 * \param [in]       numBytes    Number of bytes to hash
 * \param [out]      pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void SHA256_Hash(const uint8_t *pData, uint32_t numBytes, uint8_t *pOutput)
{
    sha256Context_t context;

    SHA256_Init(&context);
    SHA256_HashUpdate(&context, pData, numBytes);
    SHA256_HashFinish(&context, pOutput);
}

/*! *********************************************************************************
 * \brief  This function allocates a memory buffer for a HMAC SHA256 context structure
 *
 * \return    Address of the HMAC SHA256 context buffer
 *            Deallocate using HMAC_SHA256_FreeCtx()
 *
 ********************************************************************************** */
void *HMAC_SHA256_AllocCtx(void)
{
    void *hmacSha256Ctx = MEM_BufferAlloc(sizeof(HMAC_SHA256_context_t));

    return hmacSha256Ctx;
}

/*! *********************************************************************************
 * \brief  This function deallocates the memory buffer for the HMAC SHA256 context structure
 *
 * \param [in]    pContext    Address of the HMAC SHA256 context buffer
 *
 ********************************************************************************** */
void HMAC_SHA256_FreeCtx(void *pContext)
{
    (void)MEM_BufferFree(pContext);
}

/*! *********************************************************************************
 * \brief  This function performs the initialization of the HMAC SHA256 context data
 *
 * \param [in]    pContext    Pointer to the HMAC SHA256 context data
 *                            Allocated using HMAC_SHA256_AllocCtx()
 * \param [in]    pKey        Pointer to the HMAC key
 * \param [in]    keyLen      Length of the HMAC key in bytes
 *
 ********************************************************************************** */
void HMAC_SHA256_Init(void *pContext, const uint8_t *pKey, uint32_t keyLen)
{
    uint8_t                i;
    HMAC_SHA256_context_t *context                               = (HMAC_SHA256_context_t *)pContext;
    uint8_t                sha256HashKeyBuffer[SHA256_HASH_SIZE] = {0};

    if (keyLen > SHA256_BLOCK_SIZE)
    {
        SHA256_Hash(pKey, keyLen, sha256HashKeyBuffer);
        pKey   = sha256HashKeyBuffer;
        keyLen = SHA256_HASH_SIZE;
    }

    /* Create i_pad */
    for (i = 0u; i < keyLen; i++)
    {
        context->pad[i] = pKey[i] ^ gHmacIpad_c;
    }

    for (i = (uint8_t)(keyLen & 0xffu); i < SHA256_BLOCK_SIZE; i++)
    {
        context->pad[i] = gHmacIpad_c;
    }
    /* start hashing of the i_key_pad */
    SHA256_Init(&context->shaCtx);
    SHA256_HashUpdate(&context->shaCtx, context->pad, SHA256_BLOCK_SIZE);

    /* create o_pad by xor-ing pad[i] with 0x36 ^ 0x5C: */
    for (i = 0u; i < SHA256_BLOCK_SIZE; i++)
    {
        context->pad[i] ^= (gHmacIpad_c ^ gHmacOpad_c);
    }
}

/*! *********************************************************************************
 * \brief  This function performs HMAC update with the input data.
 *
 * \param [in]    pContext    Pointer to the HMAC SHA256 context data
 *                            Allocated using HMAC_SHA256_AllocCtx()
 * \param [in]    pData       Pointer to the input data
 * \param [in]    numBytes    Number of bytes to hash
 *
 ********************************************************************************** */
void HMAC_SHA256_Update(void *pContext, const uint8_t *pData, uint32_t numBytes)
{
    HMAC_SHA256_context_t *context = (HMAC_SHA256_context_t *)pContext;

    SHA256_HashUpdate(&context->shaCtx, pData, numBytes);
}

/*! *********************************************************************************
 * \brief  This function finalizes the HMAC SHA256 computation and clears the context data.
 *         The final hash value is stored at the provided output location.
 *
 * \param [in]       pContext    Pointer to the HMAC SHA256 context data
 *                               Allocated using HMAC_SHA256_AllocCtx()
 * \param [in,out]   pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void HMAC_SHA256_Finish(void *pContext, uint8_t *pOutput)
{
    uint8_t                hash1[SHA256_HASH_SIZE];
    HMAC_SHA256_context_t *context = (HMAC_SHA256_context_t *)pContext;

    /* finalize the hash of the i_key_pad and message */
    SHA256_HashFinish(&context->shaCtx, hash1);
    /* perform hash of the o_key_pad and hash1 */
    SHA256_Init(&context->shaCtx);
    SHA256_HashUpdate(&context->shaCtx, context->pad, SHA256_BLOCK_SIZE);
    SHA256_HashUpdate(&context->shaCtx, hash1, SHA256_HASH_SIZE);
    SHA256_HashFinish(&context->shaCtx, pOutput);
}

/*! *********************************************************************************
 * \brief  This function performs all HMAC SHA256 steps on multiple bytes: initialize,
 *         update, finish, and update context data.
 *         The final HMAC value is stored at the provided output location.
 *
 * \param [in]       pKey        Pointer to the HMAC key
 * \param [in]       keyLen      Length of the HMAC key in bytes
 * \param [in]       pData       Pointer to the input data
 * \param [in]       numBytes    Number of bytes to perform HMAC on
 * \param [in,out]   pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void HMAC_SHA256(const uint8_t *pKey, uint32_t keyLen, const uint8_t *pData, uint32_t numBytes, uint8_t *pOutput)
{
    HMAC_SHA256_context_t context;

    HMAC_SHA256_Init(&context, pKey, keyLen);
    HMAC_SHA256_Update(&context, pData, numBytes);
    HMAC_SHA256_Finish(&context, pOutput);
}

#if (defined(mDbgRevertKeys_d) && (mDbgRevertKeys_d > 0))
static ecdhPublicKey_t  mReversedPublicKey;
static ecdhPrivateKey_t mReversedPrivateKey;
#endif /* mDbgRevertKeys_d */

/* ECDH Sample Data Bluetooth specification V5.0 :
7.1.2.1 P-256 Data Set 1
Private A: 3f49f6d4 a3c55f38 74c9b3e3 d2103f50 4aff607b eb40b799 5899b8a6 cd3c1abd
Private B: 55188b3d 32f6bb9a 900afcfb eed4e72a 59cb9ac2 f19d7cfb 6b4fdd49 f47fc5fd
Public A(x): 20b003d2 f297be2c 5e2c83a7 e9f9a5b9 eff49111 acf4fddb cc030148 0e359de6
Public A(y): dc809c49 652aeb6d 63329abf 5a52155c 766345c2 8fed3024 741c8ed0 1589d28b
Public B(x): 1ea1f0f0 1faf1d96 09592284 f19e4c00 47b58afd 8615a69f 559077b2 2faaa190
Public B(y): 4c55f33e 429dad37 7356703a 9ab85160 472d1130 e28e3676 5f89aff9 15b1214a
DHKey: ec0234a3 57c8ad05 341010a6 0a397d9b 99796b13 b4f866f1 868d34f3 73bfa698
7.1.2.2 P-256 Data Set 2
Private A: 06a51669 3c9aa31a 6084545d 0c5db641 b48572b9 7203ddff b7ac73f7 d0457663
Private B: 529aa067 0d72cd64 97502ed4 73502b03 7e8803b5 c60829a5 a3caa219 505530ba
Public A(x): 2c31a47b 5779809e f44cb5ea af5c3e43 d5f8faad 4a8794cb 987e9b03 745c78dd
Public A(y): 91951218 3898dfbe cd52e240 8e43871f d0211091 17bd3ed4 eaf84377 43715d4f
Public B(x): f465e43f f23d3f1b 9dc7dfc0 4da87581 84dbc966 204796ec cf0d6cf5 e16500cc
Public B(y): 0201d048 bcbbd899 eeefc424 164e33c2 01c2b010 ca6b4d43 a8a155ca d8ecb279
DHKey: ab85843a 2f6d883f 62e5684b 38e30733 5fe6e194 5ecd1960 4105c6f2 3221eb69
*/

/************************************************************************************
 * \brief Generates a new ECDH P256 Private/Public key pair
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_GenerateKeys(ecdhPublicKey_t *pOutPublicKey, ecdhPrivateKey_t *pOutPrivateKey)
{
    secResultType_t result;

#if (gSecLibUseBleDebugKeys_d == 0)
#if !(defined gSecLibUseDspExtension_d && (gSecLibUseDspExtension_d == 1))
    void *pMultiplicationBuffer = MEM_BufferAlloc(gEcP256_MultiplicationBufferSize_c);
    if (NULL == pMultiplicationBuffer)
    {
        result = gSecAllocError_c;
    }
    else
    {
#if mDbgRevertKeys_d
        if (gSecEcp256Success_c !=
            ECP256_GenerateKeyPair(&mReversedPublicKey, &mReversedPrivateKey, pMultiplicationBuffer))
#else  /* !mDbgRevertKeys_d */
        if (gSecEcp256Success_c != ECP256_GenerateKeyPair(pOutPublicKey, pOutPrivateKey, pMultiplicationBuffer))
#endif /* mDbgRevertKeys_d */
        {
            result = gSecError_c;
        }
        else
        {
            result = gSecSuccess_c;
#if mDbgRevertKeys_d
            FLib_MemCpyReverseOrder(pOutPublicKey->components_8bit.x, mReversedPublicKey.components_8bit.x, 32);
            FLib_MemCpyReverseOrder(pOutPublicKey->components_8bit.y, mReversedPublicKey.components_8bit.y, 32);
            FLib_MemCpyReverseOrder(pOutPrivateKey->raw_8bit, mReversedPrivateKey.raw_8bit, 32);
#endif /* mDbgRevertKeys_d */
        }

        MEM_BufferFree(pMultiplicationBuffer);
    }
#else
    ecp256KeyPair_t KeyPair;
    if (gSecEcp256Success_c != ECP256_GenerateKeyPairUltraFast(&KeyPair.public_key, &KeyPair.private_key, NULL))
    {
        result = gSecError_c;
    }
    else
    {
        result = gSecSuccess_c;
        /* The NCCL output is BE and BLE expected LE */
        ECP256_PointCopy_and_change_endianness((uint8_t *)pOutPublicKey, (const uint8_t *)&KeyPair.public_key);
        ECP256_coordinate_copy_and_change_endianness((uint8_t *)pOutPrivateKey, (const uint8_t *)&KeyPair.private_key);
    }
#endif
#else  /* gSecLibUseBleDebugKeys_d */
    /* The NCCL output is BE and BLE expected LE */
    ECP256_PointCopy_and_change_endianness((uint8_t *)pOutPublicKey, (const uint8_t *)&mBleDebugKeyPair.public_key);
    ECP256_coordinate_copy_and_change_endianness((uint8_t *)pOutPrivateKey,
                                                 (const uint8_t *)&mBleDebugKeyPair.private_key);
    result = gSecSuccess_c;
#endif /* gSecLibUseBleDebugKeys_d */

    return result;
}

/************************************************************************************
 * \brief Generates a new ECDH P256 Private/Public key pair. This function starts the
 *        ECDH generate procedure. The pDhKeyData must be allocated and kept
 *        allocated for the time of the computation procedure.
 *        When the result is gSecResultPending_c the memory should be kept until the
 *        last step.
 *        In any other result messages the data shall be cleared after this call.
 *
 * \param[in]  pDhKeyData Pointer to the structure holding information about the
 *                        multiplication
 *
 * \return gSecSuccess_c, gSecResultPending_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_GenerateKeysSeg(computeDhKeyParam_t *pDhKeyData)
{
    secResultType_t result;

    /* The callback is NULL when there is no async ECDH */
    if (pfSecLibMultCallback == NULL)
    {
        result = ECDH_P256_GenerateKeys(&pDhKeyData->outPoint, &pDhKeyData->privateKey);
    }
    else
    {
        void *pMultiplicationBuffer = MEM_BufferAlloc(gEcP256_MultiplicationBufferSize_c);

        if (NULL == pMultiplicationBuffer)
        {
            result = gSecAllocError_c;
        }
        else
        {
            pDhKeyData->pWorkBuffer = pMultiplicationBuffer;
            if (gSecEcdhSuccess_c != Ecdh_GenerateNewKeysSeg(pDhKeyData))
            {
                result = gSecError_c;
            }
            else
            {
                result = gSecResultPending_c;
            }
        }
    }
    return result;
}

/************************************************************************************
 * \brief Function used to create the mac key and LTK using Bluetooth F5 algorithm
 *
 * \param  [out] pMacKey 128 bit MacKey output location (pointer)
 * \param  [out] pLtk    128 bit LTK output location (pointer)
 * \param  [in] pW       256 bit W (pointer) (DHKey)
 * \param  [in] pN1      128 bit N1 (pointer) (Na)
 * \param  [in] pN2      128 bit N2 (pointer) (Nb)
 * \param  [in] a1at     8 bit A1 address type, 0 = Public, 1 = Random
 * \param  [in] pA1      48 bit A1 (pointer) (A)
 * \param  [in] a2at     8 bit A2 address type, 0 = Public, 1 = Random
 * \param  [in] pA2      48 bit A2 (pointer) (B)
 *
 * \retval gSecSuccess_c operation succeeded
 * \retval gSecError_c operation failed
 ************************************************************************************/
secResultType_t SecLib_GenerateBluetoothF5Keys(uint8_t       *pMacKey,
                                               uint8_t       *pLtk,
                                               const uint8_t *pW,
                                               const uint8_t *pN1,
                                               const uint8_t *pN2,
                                               const uint8_t  a1at,
                                               const uint8_t *pA1,
                                               const uint8_t  a2at,
                                               const uint8_t *pA2)
{
    secResultType_t result     = gSecError_c;
    const uint8_t   f5KeyId[4] = {0x62, 0x74, 0x6c, 0x65}; /*!< Big Endian, "btle" */
    uint8_t         f5CmacBuffer[1 + 4 + 16 + 16 + 7 + 7 + 2];
    /* Counter[1] || keyId[4] || N1[16] || N2[16] || A1[7] || A2[7] || Length[2] = 53 */

    uint8_t       f5T[16]    = {0};
    const uint8_t f5Salt[16] = {0x6C, 0x88, 0x83, 0x91, 0xAA, 0xF5, 0xA5, 0x38,
                                0x60, 0x37, 0x0B, 0xDB, 0x5A, 0x60, 0x83, 0xBE}; /*!< Big endian */
    do
    {
        uint8_t tempOut[16];

        /*! Check for NULL output pointers and return with proper status if this is the case. */
        if ((NULL == pMacKey) || (NULL == pLtk) || (NULL == pN1) || (NULL == pN2) || (NULL == pA1) || (NULL == pA2))
        {
#if defined(gSmDebugEnabled_d) && (gSmDebugEnabled_d == 1U)
            SmDebug_Log(gSmDebugFileSmCrypto_c, __LINE__, smDebugLogTypeError_c, 0);
#endif /* gSmDebugEnabled_d */
            RAISE_ERROR(result, gSecError_c);
        }

        /*! Compute the f5 function key T using the predefined salt as key for AES-128-CAMC */
        AES_128_CMAC_LsbFirstInput((const uint8_t *)pW, 32, (const uint8_t *)f5Salt, f5T);

        /*! Build the most significant part of the f5 input data to compute the MacKey */
        f5CmacBuffer[0] = 0; /* Counter = 0 */
        FLib_MemCpy(&f5CmacBuffer[1], (const uint8_t *)f5KeyId, 4);
        FLib_MemCpyReverseOrder(&f5CmacBuffer[5], (const uint8_t *)pN1, 16);
        FLib_MemCpyReverseOrder(&f5CmacBuffer[21], (const uint8_t *)pN2, 16);
        f5CmacBuffer[37] = 0x01U & a1at;
        FLib_MemCpyReverseOrder(&f5CmacBuffer[38], (const uint8_t *)pA1, 6);
        f5CmacBuffer[44] = 0x01U & a2at;
        FLib_MemCpyReverseOrder(&f5CmacBuffer[45], (const uint8_t *)pA2, 6);
        f5CmacBuffer[51] = 0x01; /* Length msB big endian = 0x01, Length = 256 */
        f5CmacBuffer[52] = 0x00; /* Length lsB big endian = 0x00, Length = 256 */

        /*! Compute the MacKey into the temporary buffer. */
        AES_128_CMAC(f5CmacBuffer, sizeof(f5CmacBuffer), f5T, tempOut);

        /*! Copy the MacKey to the output location
         *  in reverse order. The CMAC result is generated MSB first. */
        FLib_MemCpyReverseOrder(pMacKey, (const uint8_t *)tempOut, 16);

        /*! Build the least significant part of the f5 input data to compute the MacKey.
         *  It is identical to the most significant part with the exception of the counter. */
        f5CmacBuffer[0] = 1; /* Counter = 1 */

        /*! Compute the LTK into the temporary buffer. */
        AES_128_CMAC(f5CmacBuffer, sizeof(f5CmacBuffer), f5T, tempOut);

        /*! Copy the LTK to the output location
         *  in reverse order. The CMAC result is generated MSB first. */
        FLib_MemCpyReverseOrder(pLtk, (const uint8_t *)tempOut, 16);

        result = gSecSuccess_c;

    } while (false);

    return result;
}

#if (defined(mDbgRevertKeys_d) && (mDbgRevertKeys_d > 0))
static ecdhDhKey_t mReversedEcdhKey;
#endif /* mDbgRevertKeys_d */

secResultType_t ECDH_P256_ComputeDhKey(const ecdhPrivateKey_t *pPrivateKey,
                                       const ecdhPublicKey_t  *pPeerPublicKey,
                                       ecdhDhKey_t            *pOutDhKey,
                                       const bool_t            keepBlobDhKey)
{
    secResultType_t result = gSecSuccess_c;
    secEcdhStatus_t ecdhStatus;
    do
    {
        if (!ECP256_LePointValid(pPeerPublicKey))
        {
            result = gSecInvalidPublicKey_c;
            break;
        }
#if !(defined gSecLibUseDspExtension_d && (gSecLibUseDspExtension_d == 1))

        void *pMultiplicationBuffer = MEM_BufferAlloc(gEcP256_MultiplicationBufferSize_c);
        if (NULL == pMultiplicationBuffer)
        {
            result = gSecAllocError_c;
            break;
        }

#if mDbgRevertKeys_d
        FLib_MemCpyReverseOrder(mReversedPublicKey.components_8bit.x, pPeerPublicKey->components_8bit.x, 32);
        FLib_MemCpyReverseOrder(mReversedPublicKey.components_8bit.y, pPeerPublicKey->components_8bit.y, 32);
        FLib_MemCpyReverseOrder(mReversedPrivateKey.raw_8bit, pPrivateKey->raw_8bit, 32);
#endif /* mDbgRevertKeys_d */

#if mDbgRevertKeys_d
        ecdhStatus =
            Ecdh_ComputeDhKey(&mReversedPrivateKey, &mReversedPublicKey, &mReversedEcdhKey, pMultiplicationBuffer);
#else  /* !mDbgRevertKeys_d */
        ecdhStatus = Ecdh_ComputeDhKey(pPrivateKey, pPeerPublicKey, pOutDhKey, pMultiplicationBuffer);
#endif /* mDbgRevertKeys_d */
        if (gSecEcdhInvalidPublicKey_c == ecdhStatus)
        {
            result = gSecInvalidPublicKey_c;
            break;
        }
        else if (gSecEcdhSuccess_c != ecdhStatus)
        {
            result = gSecError_c;
            break;
        }
        else
        {
#if mDbgRevertKeys_d
            FLib_MemCpyReverseOrder(pOutDhKey->components_8bit.x, mReversedEcdhKey.components_8bit.x, 32);
            FLib_MemCpyReverseOrder(pOutDhKey->components_8bit.y, mReversedEcdhKey.components_8bit.y, 32);
#endif /* mDbgRevertKeys_d */
        }

        MEM_BufferFree(pMultiplicationBuffer);

#else
        ecp256Point_t      peer_public_key;
        ecp256Coordinate_t self_private_key;
        ecp256Point_t      dh_secret;
        ECP256_PointCopy_and_change_endianness(&peer_public_key.raw[0], (const uint8_t *)pPeerPublicKey);
        ECP256_coordinate_copy_and_change_endianness(&self_private_key.raw_8bit[0], (const uint8_t *)pPrivateKey);
        ecdhStatus = Ecdh_ComputeDhKeyUltraFast(&self_private_key, &peer_public_key, &dh_secret, NULL);
        if (ecdhStatus == gSecEcdhSuccess_c)
        {
            ECP256_PointCopy_and_change_endianness(&pOutDhKey->raw[0], (const uint8_t *)&dh_secret);
        }
        else
        {
            result = gSecError_c;
        }
#endif
    } while (false);
    return result;
}

/************************************************************************************
 * \brief Free any data allocated in the input structure.
 *
 * \param[in]  pDhKeyData Pointer to the structure holding information about the
 *                        multiplication
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
void ECDH_P256_FreeDhKeyData(computeDhKeyParam_t *pDhKeyData)
{
    NOT_USED(pDhKeyData);
}

/************************************************************************************
 * \brief Checks whether a public key is valid (point is on the curve).
 *
 * \return TRUE if valid, FALSE if not
 *
 ************************************************************************************/
bool_t ECP256_IsKeyValid(const ecp256Point_t *pKey)
{
    bool_t ret = false;

    if (ECP256_LePointValid(pKey))
    {
        ret = true;
    }

    return ret;
}

/*! *********************************************************************************
 * \brief  This function implements the SMP ah cryptographic toolbox function which calculates the
 *         hash part of a Resolvable Private Address.
 *         The key is kept in plaintext.
 *
 * \param[out]  pHash  Pointer where the 24 bit hash value will be written.
 *                     24 bit hash field of a Resolvable Private Address (output)
 *
 * \param[in]  pKey  Pointer to the 128 bit key.
 *
 * \param[in]  pR   Pointer to the 24 bit random value (Prand).
 *                  The most significant bits of this field must be 0b01 for Resolvable Private Addresses.
 *
 * \retval  gSecSuccess_c  All operations were successful.
 * \retval  gSecError_c The call failed.
 *
 ********************************************************************************** */
secResultType_t SecLib_VerifyBluetoothAh(uint8_t *pHash, const uint8_t *pKey, const uint8_t *pR)
{
    secResultType_t result           = gSecError_c;
    uint8_t         tempAddrPart[16] = {0};
    uint8_t         tempOutHash[16];
    uint8_t         tempKey[16];
    do
    {
        /*! Check for NULL output pointers and return with proper status if this is the case. */
        if ((NULL == pHash) || (NULL == pKey) || (NULL == pR))
        {
            break;
        }
        /* Initialize the r' value in the temporary location. 3 bytes of ramdom value.
         *  Initialize it reversed for AES.
         */
        for (int i = 0; i < 3; i++)
        {
            tempAddrPart[15 - i] = pR[i];
        }
        /* Regular operation with plaintext key */
        /*! Reverse the Key and place it in a temporary location. */
        FLib_MemCpyReverseOrder(tempKey, (const uint8_t *)pKey, 16);

        /*! Compute the hash. */
        AES_128_Encrypt(tempAddrPart, tempKey, tempOutHash);

        /*! Copy the relevant bytes to the output. */
        pHash[0] = tempOutHash[15];
        pHash[1] = tempOutHash[14];
        pHash[2] = tempOutHash[13];

        result = gSecSuccess_c;

    } while (false);

    return result;
}

/************************************************************************************
 * \brief Computes the Diffie-Hellman Key for an ECDH P256 key pair. This function
 *        starts the ECDH key pair generate procedure. The pDhKeyData must be
 *        allocated and kept allocated for the time of the computation procedure.
 *        When the result is gSecResultPending_c the memory should be kept until the
 *        last step, when it can be safely freed.
 *        In any other result messages the data shall be cleared after this call.
 *
 * \param[in]  pDhKeyData Pointer to the structure holding information about the
 *                        multiplication
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_ComputeDhKeySeg(computeDhKeyParam_t *pDhKeyData)
{
    secResultType_t result;

    if (pfSecLibMultCallback == NULL)
    {
        result =
            ECDH_P256_ComputeDhKey(&pDhKeyData->privateKey, &pDhKeyData->peerPublicKey, &pDhKeyData->outPoint, FALSE);
    }
    else
    {
        secEcdhStatus_t ecdhStatus;

        void *pMultiplicationBuffer = MEM_BufferAlloc(gEcP256_MultiplicationBufferSize_c);
        if (NULL == pMultiplicationBuffer)
        {
            result = gSecAllocError_c;
        }
        else
        {
            ecdhStatus = Ecdh_ComputeDhKeySeg(pDhKeyData);

            if (gSecEcdhInvalidPublicKey_c == ecdhStatus)
            {
                result = gSecInvalidPublicKey_c;
            }
            else if (gSecEcdhSuccess_c != ecdhStatus)
            {
                result = gSecError_c;
            }
            else
            {
                result = gSecResultPending_c;
            }
        }
    }
    return result;
}

/************************************************************************************
 * \brief Handle one step of ECDH multiplication depending on the number of steps at
 *        a time according to gSecLibEcStepsAtATime. After the last step is completed
 *        the function returns TRUE and the upper layer is responsible for clearing
 *        pData.
 *
 * \param[in]  pData Pointer to the structure holding information about the
 *                   multiplication
 *
 * \return TRUE if the multiplication is completed
 *         FALSE when the function needs to be called again
 *
 ************************************************************************************/
bool_t SecLib_HandleMultiplyStep(computeDhKeyParam_t *pData)
{
    bool_t  result = FALSE;
    uint8_t steps  = ((255U + 1U) / gSecLibEcStepsAtATime);

    /* Intermediate step */
    if (pData->procStep < steps)
    {
        /* Compute step */
        Ecdh_ComputeJacobiChunk(255U - (pData->procStep * gSecLibEcStepsAtATime), gSecLibEcStepsAtATime, pData);
        /* Go to the next step */
        pData->procStep++;
        result = FALSE;
    }
    /* Final step was completed -> resume SecLib procedure */
    else
    {
        Ecdh_JacobiCompleteMult(pData);

#if (defined(mDbgRevertKeys_d) && (mDbgRevertKeys_d > 0))
        {
            FLib_MemCpyReverseOrder(mReversedEcdhKey.components_8bit.x, pData->outX,
                                    sizeof(mReversedEcdhKey.components_8bit.x));
            FLib_MemCpyReverseOrder(mReversedEcdhKey.components_8bit.y, pData->outY,
                                    sizeof(mReversedEcdhKey.components_8bit.y));
            FLib_MemCpyReverseOrder(pData->outX, mReversedEcdhKey.components_8bit.x, sizeof(pData->outX));
            FLib_MemCpyReverseOrder(pData->outY, mReversedEcdhKey.components_8bit.y, sizeof(pData->outY));
        }
#endif /* mDbgRevertKeys_d */

        result = TRUE;
    }
    return result;
}

secResultType_t SecLib_GenerateBluetoothF5KeysSecure(uint8_t       *pMacKey,
                                                     uint8_t       *pLtk,
                                                     const uint8_t *pW,
                                                     const uint8_t *pN1,
                                                     const uint8_t *pN2,
                                                     const uint8_t  a1at,
                                                     const uint8_t *pA1,
                                                     const uint8_t  a2at,
                                                     const uint8_t *pA2)
{
    NOT_USED(pMacKey);
    NOT_USED(pLtk);
    NOT_USED(pW);
    NOT_USED(pN1);
    NOT_USED(pN2);
    NOT_USED(a1at);
    NOT_USED(pA1);
    NOT_USED(a2at);
    NOT_USED(pA2);
    return gSecError_c;
}

/************************************************************************************
 * \brief Converts a plaintext symmetric key into a blob of blobType. Reverses key beforehand.
 *
 * \param[in]  pKey      Pointer to the key.
 *
 * \param[out] pBlob     Pointer to the blob (shall be allocated, 40 or 16, depending on blobType)
 *
 * \param[in]  blobType  Blob type.
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t SecLib_ObfuscateKeySecure(const uint8_t *pKey, uint8_t *pBlob, const uint8_t blobType)
{
    NOT_USED(pKey);
    NOT_USED(pBlob);
    NOT_USED(blobType);
    return gSecError_c;
}

/************************************************************************************
 * \brief Converts a blob of a symmetric key into the plaintext. Reverses key afterwards.
 *
 * \param[in]  pBlob    Pointer to the blob.
 *
 * \param[out] pKey     Pointer to the key.
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t SecLib_DeobfuscateKeySecure(const uint8_t *pBlob, uint8_t *pKey)
{
    NOT_USED(pBlob);
    NOT_USED(pKey);
    return gSecError_c;
}

/************************************************************************************
 * \brief Function used to derive the Bluetooth SKD used in LL encryption.
 *        Available on EdgeLock (SSS only)
 *
 * \param  [in] pInSKD   pointer to the received SKD (16-byte array)
 * \param  [in] pLtkBlob pointer to the blob (40-byte array)
 * \param  [in] bOpenKey  if TRUE sends derived key to NBU
 * \param  [out] pOutSKD pointer to the resulted SKD (16-byte array)
 *
 * \retval gSecSuccess_c operation succeeded
 * \retval gSecError_c operation failed / not implemented
 ************************************************************************************/
secResultType_t SecLib_DeriveBluetoothSKDSecure(const uint8_t *pInSKD,
                                                const uint8_t *pLtkBlob,
                                                bool_t         bOpenKey,
                                                uint8_t       *pOutSKD)
{
    NOT_USED(pInSKD);
    NOT_USED(pLtkBlob);
    NOT_USED(bOpenKey);
    NOT_USED(pOutSKD);

    return gSecError_c;
}

secResultType_t SecLib_GenerateSymmetricKey(const uint32_t keySize, const bool_t blobOutput, void *pOut)
{
    NOT_USED(keySize);
    NOT_USED(blobOutput);
    NOT_USED(pOut);
    return gSecError_c;
}

secResultType_t SecLib_GenerateBluetoothEIRKBlobSecure(const void  *pIRK,
                                                       const bool_t blobInput,
                                                       const bool_t generateDKeyIRK,
                                                       uint8_t     *pOutEIRKblob)
{
    NOT_USED(pIRK);
    NOT_USED(blobInput);
    NOT_USED(generateDKeyIRK);
    NOT_USED(pOutEIRKblob);
    return gSecError_c;
}
secResultType_t ECDH_P256_ComputeA2BKeySecure(const ecdhPublicKey_t *pInPeerPublicKey, ecdhDhKey_t *pOutE2EKey)
{
    NOT_USED(pInPeerPublicKey);
    NOT_USED(pOutE2EKey);
    return gSecError_c;
}

secResultType_t SecLib_ExportA2BBlobSecure(const void *pKey, const secInputKeyType_t keyType, uint8_t *pOutKey)
{
    NOT_USED(pKey);
    NOT_USED(keyType);
    NOT_USED(pOutKey);
    return gSecError_c;
}

void ECDH_P256_FreeDhKeyDataSecure(computeDhKeyParam_t *pDhKeyData)
{
    NOT_USED(pDhKeyData);
}

secResultType_t SecLib_ImportA2BBlobSecure(const uint8_t *pKey, const secInputKeyType_t keyType, uint8_t *pOutKey)
{
    NOT_USED(pKey);
    NOT_USED(keyType);
    NOT_USED(pOutKey);
    return gSecError_c;
}

secResultType_t ECDH_P256_FreeE2EKeyDataSecure(ecdhDhKey_t *pE2EKeyData)
{
    NOT_USED(pE2EKeyData);
    return gSecError_c;
}

secResultType_t SecLib_VerifyBluetoothAhSecure(uint8_t *pHash, const uint8_t *pKey, const uint8_t *pR)
{
    NOT_USED(pHash);
    NOT_USED(pKey);
    NOT_USED(pR);
    return gSecError_c;
}

/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
 * \brief  This function performs SHA1 on multiple blocks
 *
 * \param [in]    pData      Pointer to the input data
 * \param [in]    nBlk       Number of SHA1 blocks to hash
 * \param [in]    context        Pointer to the SHA1 context data
 *
 ********************************************************************************** */
static void SHA1_hash_n(uint8_t *pData, uint32_t nBlk, uint32_t *pHash)
{
#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    SECLIB_MUTEX_LOCK();
    mmcau_sha1_hash_n(pData, nBlk, (unsigned int *)pHash);
    SECLIB_MUTEX_UNLOCK();
#else
    sw_sha1_hash_n(pData, (int32_t)nBlk, pHash);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs SHA256 on multiple blocks
 *
 * \param [in]    pData      Pointer to the input data
 * \param [in]    nBlk       Number of SHA256 blocks to hash
 * \param [in]    context        Pointer to the SHA256 context data
 *
 ********************************************************************************** */
static void SHA256_hash_n(const uint8_t *pData, uint32_t nBlk, uint32_t *pHash)
{
#if (defined(FSL_FEATURE_SOC_MMCAU_COUNT) && (FSL_FEATURE_SOC_MMCAU_COUNT > 0))
    SECLIB_MUTEX_LOCK();
    mmcau_sha256_hash_n(pData, nBlk, (unsigned int *)pHash);
    SECLIB_MUTEX_UNLOCK();
#else
    sw_sha256_hash_n(pData, (int32_t)nBlk, pHash);
#endif
}

#if (defined FSL_FEATURE_SOC_AES_HW && (FSL_FEATURE_SOC_AES_HW > 0))
/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 ECB encryption
 *
 * \param [in]    ECB_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_ECB_Enc_HW(AES_param_t *ECB_p)
{
    uint8_t tempBuffIn[AES_BLOCK_SIZE]  = {0};
    uint8_t tempBuffOut[AES_BLOCK_SIZE] = {0};

    /* If remaining data bigger than one AES block size */
    while (ECB_p->Len > AES_BLOCK_SIZE)
    {
        AES_128_Encrypt(ECB_p->pPlain, ECB_p->Key, ECB_p->pCipher);
        ECB_p->pPlain += AES_BLOCK_SIZE;
        ECB_p->pCipher += AES_BLOCK_SIZE;
        ECB_p->Len -= AES_BLOCK_SIZE;
    }

    /* If remaining data is smaller then one AES block size */
    FLib_MemCpy(tempBuffIn, ECB_p->pPlain, ECB_p->Len);
    AES_128_Encrypt(tempBuffIn, ECB_p->Key, tempBuffOut);
    FLib_MemCpy(ECB_p->pCipher, tempBuffOut, AES_BLOCK_SIZE);
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128ECB_Enc_Id);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 ECB decryption
 *
 * \param [in]    ECB_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_ECB_Dec_HW(AES_param_t *ECB_p)
{
    uint8_t tempBuffIn[AES_BLOCK_SIZE]  = {0};
    uint8_t tempBuffOut[AES_BLOCK_SIZE] = {0};

    /* If remaining data bigger than one AES block size */
    while (ECB_p->Len > AES_BLOCK_SIZE)
    {
        AES_128_Decrypt(ECB_p->pCipher, ECB_p->Key, ECB_p->pPlain);
        ECB_p->pPlain += AES_BLOCK_SIZE;
        ECB_p->pCipher += AES_BLOCK_SIZE;
        ECB_p->Len -= AES_BLOCK_SIZE;
    }

    /* If remaining data is smaller then one AES block size */
    FLib_MemCpy(tempBuffIn, ECB_p->pCipher, ECB_p->Len);
    AES_128_Decrypt(tempBuffIn, ECB_p->Key, tempBuffOut);
    FLib_MemCpy(ECB_p->pPlain, tempBuffOut, ECB_p->Len);
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128ECB_Dec_Id);
#endif /* USE_TASK_FOR_HW_AES */
}

/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 ECB block encryption
 *
 * \param [in]    ECB_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_ECB_Block_Enc_HW(AES_param_t *ECBB_p)
{
    while (ECBB_p->Blocks > 0u)
    {
        AES_128_Encrypt(ECBB_p->pPlain, ECBB_p->Key, ECBB_p->pCipher);
        ECBB_p->Blocks--;
        ECBB_p->pPlain += AES_BLOCK_SIZE;
        ECBB_p->pCipher += AES_BLOCK_SIZE;
    }
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128ECBB_Enc_Id);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 ECB block decryption
 *
 * \param [in]    ECB_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_ECB_Block_Dec_HW(AES_param_t *ECBB_p)
{
    while (ECBB_p->Blocks > 0u)
    {
        AES_128_Decrypt(ECBB_p->pCipher, ECBB_p->Key, ECBB_p->pPlain);
        ECBB_p->Blocks--;
        ECBB_p->pPlain += AES_BLOCK_SIZE;
        ECBB_p->pCipher += AES_BLOCK_SIZE;
    }
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128ECBB_Dec_Id);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 CTR encryption
 *
 * \param [in]    CTR_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_CTR_Enc_HW(AES_param_t *CTR_p)
{
    uint8_t tempBuffIn[AES_BLOCK_SIZE] = {0};
    uint8_t encrCtr[AES_BLOCK_SIZE]    = {0};

    /* If remaining data bigger than one AES block size */
    while (CTR_p->Len > AES_BLOCK_SIZE)
    {
        FLib_MemCpy(tempBuffIn, CTR_p->pPlain, AES_BLOCK_SIZE);
        AES_128_Encrypt(CTR_p->CTR_counter, CTR_p->Key, encrCtr);
        SecLib_XorN(tempBuffIn, encrCtr, AES_BLOCK_SIZE);
        FLib_MemCpy(CTR_p->pCipher, tempBuffIn, AES_BLOCK_SIZE);
        CTR_p->pPlain += AES_BLOCK_SIZE;
        CTR_p->pCipher += AES_BLOCK_SIZE;
        CTR_p->Len -= AES_BLOCK_SIZE;
        AES_128_IncrementCounter(CTR_p->CTR_counter);
    }

    /* If remaining data is smaller then one AES block size  */
    FLib_MemCpy(tempBuffIn, CTR_p->pPlain, CTR_p->Len);
    AES_128_Encrypt(CTR_p->CTR_counter, CTR_p->Key, encrCtr);
    SecLib_XorN(tempBuffIn, encrCtr, AES_BLOCK_SIZE);
    FLib_MemCpy(CTR_p->pCipher, tempBuffIn, CTR_p->Len);
    AES_128_IncrementCounter(CTR_p->CTR_counter);
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128CTR_Enc_Id);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 CTR decryption
 *
 * \param [in]    CTR_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_CTR_Dec_HW(AES_param_t *CTR_p)
{
    uint8_t tempBuffIn[AES_BLOCK_SIZE] = {0};
    uint8_t encrCtr[AES_BLOCK_SIZE]    = {0};

    /* If remaining data bigger than one AES block size */
    while (CTR_p->Len > AES_BLOCK_SIZE)
    {
        FLib_MemCpy(tempBuffIn, CTR_p->pCipher, AES_BLOCK_SIZE);
        AES_128_Encrypt(CTR_p->CTR_counter, CTR_p->Key, encrCtr);
        SecLib_XorN(tempBuffIn, encrCtr, AES_BLOCK_SIZE);
        FLib_MemCpy(CTR_p->pPlain, tempBuffIn, AES_BLOCK_SIZE);
        CTR_p->pPlain += AES_BLOCK_SIZE;
        CTR_p->pCipher += AES_BLOCK_SIZE;
        CTR_p->Len -= AES_BLOCK_SIZE;
        AES_128_IncrementCounter(CTR_p->CTR_counter);
    }

    /* If remaining data is smaller then one AES block size  */
    FLib_MemCpy(tempBuffIn, CTR_p->pCipher, CTR_p->Len);
    AES_128_Encrypt(CTR_p->CTR_counter, CTR_p->Key, encrCtr);
    SecLib_XorN(tempBuffIn, encrCtr, AES_BLOCK_SIZE);
    FLib_MemCpy(CTR_p->pPlain, tempBuffIn, CTR_p->Len);
    AES_128_IncrementCounter(CTR_p->CTR_counter);
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128CTR_Dec_Id);
#endif
}

/*! *********************************************************************************
 * \brief  This function performs hardware AES-128 CMAC encryption
 *
 * \param [in]    CMAC_p      Pointer to AES parameter structure
 *
 ********************************************************************************** */
static void AES_128_CMAC_HW(AES_param_t *CMAC_p)
{
    uint8_t X[16];
    uint8_t Y[16];
    uint8_t M_last[16] = {0};
    uint8_t padded[16] = {0};

    uint8_t K1[16] = {0};
    uint8_t K2[16] = {0};

    uint8_t  n;
    uint32_t i;
    uint8_t  flag;
    uint8_t  n;
    uint8_t  residual_len;

    AES_128_CMAC_Generate_Subkey(CMAC_p->Key, K1, K2);

    n            = (uint8_t)((CMAC_p->Len + (AES_BLOCK_SIZE - 1u)) / AES_BLOCK_SIZE); /* n is number of rounds */
    residual_len = (uint8_t)(AES_PARTIAL_BLOCK_BYTES(CMAC_p->Len));

    if (n == 0u)
    {
        n    = 1u;
        flag = 0u;
    }
    else
    {
        if (residual_len == 0u)
        { /* last block is a complete block */
            flag = 1u;
        }
        else
        { /* last block is not complete block */
            flag = 0u;
        }
    }

    /* Process the last block  - the last part the MSB first input data */
    if (flag > 0u)
    { /* last block is complete block */
        SecLib_Xor128(&CMAC_p->pPlain[16u * (n - 1u)], K1, M_last);
    }
    else
    {
        (void)SecLib_Padding(&CMAC_p->pPlain[AES_BLOCK_SIZE * (n - 1u)], padded, residual_len);
        SecLib_Xor128(padded, K2, M_last);
    }

    for (i = 0u; i < 16u; i++)
    {
        X[i] = 0u;
    }

    for (i = 0u; i < n - 1u; i++)
    {
        SecLib_Xor128(X, &CMAC_p->pPlain[AES_BLOCK_SIZE * i], Y); /* Y := Mi (+) X  */
        AES_128_Encrypt(Y, CMAC_p->Key, X);                       /* X := AES-128(KEY, Y) */
    }

    SecLib_Xor128(X, M_last, Y);
    AES_128_Encrypt(Y, CMAC_p->Key, X);

    for (i = 0u; i < 16u; i++)
    {
        CMAC_p->pCipher[i] = X[i];
    }
#if (defined(USE_TASK_FOR_HW_AES) && (USE_TASK_FOR_HW_AES > 0))
    AESM_Complete(AES128CMAC_Id);
#endif
}

#endif /* FSL_FEATURE_SOC_AES_HW */

/*! *********************************************************************************
 * \brief  This function pads an incomplete 16 byte block of data, where padding is
 *         the concatenation of x and a single '1',
 *         followed by the minimum number of '0's, so that the total length is equal to 128 bits.
 * Padding scheme is ISO/IEC 7816-4: one 80h byte (1 bit), followed by as many 00h as
 * required to fill a 128 bit block.
 *
 * \param[in, out] lastb Pointer to the last block of message to be padded
 *
 * \param[in]  pad_block Padded block destination
 *
 * \param[in]  length    Number of message bytes in the block to be padded : must be in [0..AES_BLOCK_SIZE-1]
 *
 * \return  length of padding [1..AES_BLOCK_SIZE] if ok, 0 otherwise
 *
 ********************************************************************************** */
static uint8_t SecLib_Padding(const uint8_t *lastb, uint8_t pad_block[AES_BLOCK_SIZE], uint8_t length)
{
    uint8_t  padding_sz = 0;
    uint32_t j;
    if (length < AES_BLOCK_SIZE)
    {
        for (j = 0u; j < AES_BLOCK_SIZE; j++)
        {
            /* there may be 0 bytes to copy if message was a multiple of AES_BLOCK_SIZE */
            if (j < length)
            {
                /* original last block */
                pad_block[j] = lastb[j];
            }
            else if (j == length)
            {
                pad_block[j] = 0x80u;
            }
            else
            {
                pad_block[j] = 0x00u;
            }
        }
        padding_sz = AES_BLOCK_SIZE - length;
    }
    return padding_sz;
}
/*! *********************************************************************************
 * \brief  This function removes padding from an octet string (at most 16 bytes of data).
 *
 * \param[in] pIn Pointer to start of last AES block of a message to be depadded
 *
 * \return  if > 0 Final size of padding to be removed : must be in [1..AES_BLOCK_SIZE].
 *          if 0 : error occurred the last block does not contain expected padding patter.
 *
 ********************************************************************************** */
static uint8_t SecLib_DePadding(const uint8_t pad_block[AES_BLOCK_SIZE])
{
    uint8_t padding_sz = 0u;

    for (uint8_t i = AES_BLOCK_SIZE; i > 0u; i--)
    {
        uint8_t ch = pad_block[i - 1u];
        if (ch == 0x80u)
        {
            padding_sz = AES_BLOCK_SIZE - i + 1u;
            break;
        }
        else if (ch != 0x00u)
        {
            /* not padding */
            padding_sz = 0u;
            break;
        }
        else
        {
            /* MISRA rule 15.7 but useless */
            continue;
        }
    }
    return padding_sz;
}

/*! *********************************************************************************
 * \brief  This function Xors 2 blocks of 128 bits and copies the result to a set destination
 *
 * \param [in]    a        Pointer to the first block to XOR
 *
 * \param [in]    b        Pointer to the second block to XOR.
 *
 * \param [out]   out      Destination pointer
 *
 * \remarks   This is public open source code! Terms of use must be checked before use!
 *
 ********************************************************************************** */
static void SecLib_Xor128(const uint8_t *a, const uint8_t *b, uint8_t *out)
{
    uint32_t i;

    for (i = 0u; i < AES_BLOCK_SIZE; i++)
    {
        out[i] = a[i] ^ b[i];
    }
}
/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************** */

#if (!defined(FSL_FEATURE_SOC_LTC_COUNT) || (FSL_FEATURE_SOC_LTC_COUNT == 0))
/*! *********************************************************************************
 * \brief  Increments the value of a given counter vector.
 *
 * \param [in,out]     ctr         Counter.
 *
 * \remarks used for AES CTR
 *
 ********************************************************************************** */
static void AES_128_IncrementCounter(uint8_t *ctr)
{
    uint32_t   i;
    uint64_t   tempLow;
    uuint128_t tempCtr;

    for (i = 0u; i < AES_BLOCK_SIZE; i++)
    {
        tempCtr.u8[AES_BLOCK_SIZE - i - 1] = ctr[i];
    }

    tempLow = tempCtr.u64[0];
    tempCtr.u64[0]++;

    if (tempLow > tempCtr.u64[0])
    {
        tempCtr.u64[1]++;
    }

    for (i = 0u; i < AES_BLOCK_SIZE; i++)
    {
        ctr[i] = tempCtr.u8[AES_BLOCK_SIZE - i - 1];
    }
}
#endif /* !(FSL_FEATURE_SOC_LTC_COUNT) */

/*! *********************************************************************************
 * \brief  Generates the two subkeys that correspond to an AES key
 *
 * \param [in]    key        AES Key.
 *
 * \param [out]   K1         First subkey.
 *
 * \param [out]   K2         Second subkey.
 *
 * \remarks   This is public open source code! Terms of use must be checked before use!
 *
 ********************************************************************************** */
static void AES_128_CMAC_Generate_Subkey(const uint8_t *key, uint8_t *K1, uint8_t *K2)
{
    uint8_t  const_Rb[16] = {0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                             0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x87u};
    uint8_t  L[16];
    uint8_t  Z[16];
    uint8_t  tmp[16] = {0};
    uint32_t i;

    for (i = 0u; i < 16u; i++)
    {
        Z[i] = 0u;
    }

    AES_128_Encrypt(Z, key, L);

    if ((L[0] & 0x80u) == 0u)
    {
        /* If MSB(L) = 0, then K1 = L << 1 */
        SecLib_LeftShiftOneBit(L, K1);
    }
    else
    {
        /* Else K1 = ( L << 1 ) (+) Rb */
        SecLib_LeftShiftOneBit(L, tmp);
        SecLib_Xor128(tmp, const_Rb, K1);
    }

    if ((K1[0] & 0x80u) == 0u)
    {
        SecLib_LeftShiftOneBit(K1, K2);
    }
    else
    {
        SecLib_LeftShiftOneBit(K1, tmp);
        SecLib_Xor128(tmp, const_Rb, K2);
    }
}

/*! *********************************************************************************
 * \brief    Shifts a given vector to the left with one bit.
 *
 * \param [in]      input         Input vector.
 *
 * \param [out]     output        Output vector.
 *
 * \remarks   This is public open source code! Terms of use must be checked before use!
 *
 ********************************************************************************** */
static void SecLib_LeftShiftOneBit(uint8_t *input, uint8_t *output)
{
    int32_t i;
    uint8_t overflow = 0u;

    for (i = 15; i >= 0; i--)
    {
        output[i] = input[i] << 1u;
        output[i] |= overflow;
        overflow = ((input[i] & 0x80u) > 0u) ? 1u : 0u;
    }
}
