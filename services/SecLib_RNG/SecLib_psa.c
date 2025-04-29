
/*! *********************************************************************************
 * Copyright 2025 NXP
 *
 * \file
 *
 * This is the source file for the security module used by the connectivity stacks. The Security
 *    Module SecLib provides an abstraction from the Hardware to the upper layer.
 *    In this file, a wrapper to PSA API component is implemented.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */

#include <stdint.h>
#include "EmbeddedTypes.h"
#include "SecLib.h"
#include "psa/crypto.h"
#include "psa/crypto_types.h"
#include "psa/crypto_values.h"
#include "psa/crypto_extra.h"
#include "entropy_poll.h"
#include "p256-m_driver_entrypoints.h"
#include "p256-m.h"
#include "FunctionLib.h"
#include "fwk_platform_crypto.h"
#include "SecLib_ecp256.h"
#include "CryptoLibSW.h"

/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */
#define KEY_ID_BLE0 0x426c6530

#define RAISE_ERROR(st, expected)                \
    if (st != expected)                          \
    {                                            \
        PRINTF(                                  \
            "\tassertion failed at %s:%d - "     \
            "actual:-%d expected:-%d\r\n",       \
            __FILE__, __LINE__, -st, -expected); \
        break;                                   \
    }

/*! *********************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
********************************************************************************** */
#define ECP256_COORDINATE_BITLEN 256u
#define ECP256_COORDINATE_LEN    (ECP256_COORDINATE_BITLEN >> 3)
#define ECP256_COORDINATE_WLEN   ((ECP256_COORDINATE_LEN) / 4U)

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
typedef struct psa_ecp256_context_t
{
    big_int256_t PrivateKey[ECP256_COORDINATE_WLEN];        /*!< The private key : RNG output */
    big_int256_t OwnPublicKey[2U * ECP256_COORDINATE_WLEN]; /*! Own Public computed from PrivateKey */
    uint32_t     keyId;
    psa_key_id_t OwnKey;                                    /*! Own Key object reference */
} psa_ecp256_context_t;

static psa_ecp256_context_t  psa_g_ECP_KeyPair;
static psa_ecp256_context_t *psa_pECPKeyPair = ((void *)0);

/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************** */

static bool ECP256_LePointValid(const ecp256Point_t *P)
{
#if gSecLibUseDspExtension_d
    ecp256Point_t tmp;
    ECP256_PointCopy_and_change_endianness(tmp.raw, P->raw);
    return ECP256_PointValid(&tmp);
#else
    extern bool_t EcP256_IsPointOnCurve(const uint32_t *X, const uint32_t *Y);
    return EcP256_IsPointOnCurve((const uint32_t *)&P->components_32bit.x[0],
                                 (const uint32_t *)&P->components_32bit.y[0]);
#endif
}

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
 * \brief  This function performs initialization of the cryptographic HW acceleration.
 *
 ********************************************************************************** */
void SecLib_Init(void)
{
    psa_status_t status;
    do
    {
        status = psa_crypto_init();
        RAISE_ERROR(status, PSA_SUCCESS)
    } while (false);
}

void SecLib_ReInit(void)
{
    /* Initialize cryptographic hardware.*/
    (void)PLATFORM_ResetCrypto();
}

/*! *********************************************************************************
 * \brief  This function will allow reinitizialize the cryptographic HW acceleration
 * next time we need it, typically after lowpower mode.
 *
 ********************************************************************************** */
void SecLib_DeInit(void)
{
    (void)PLATFORM_TerminateCrypto();
}

/* to see if implementation needed */
void SecLib_SetExternalMultiplicationCb(secLibCallback_t pfCallback)
{
    return;
}

/*! *********************************************************************************
 * \brief  This function performs all SHA256 steps on multiple bytes: initialize,
 *         update, finish, and update context data.
 *         The final hash value is stored at the provided output location.
 *
 * \param [in]       pData       Pointer to the input data
 * \param [in]       numBytes    Number of bytes to hash
 * \param [in,out]   pOutput     Pointer to the output location
 *
 ********************************************************************************** */
void SHA256_Hash(const uint8_t *pData, uint32_t numBytes, uint8_t *pOutput)
{
    const psa_algorithm_t alg        = PSA_ALG_SHA_256;
    size_t                hashLength = 0;
    psa_status_t          status;
    psa_hash_operation_t  op = psa_hash_operation_init();

    do
    {
        status = psa_hash_setup(&op, alg);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_hash_compute(alg, pData, numBytes, pOutput, SHA256_HASH_SIZE, &hashLength);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_hash_abort(&op);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

/*! *********************************************************************************
 * \brief  This function calculates XOR of individual byte pairs in two uint8_t arrays.
 *         pDst[i] := pDst[i] ^ pSrc[i] for i=0 to n-1
 *
 * \param[in]  pDst First byte array operand for XOR and destination byte array
 *
 * \param[in]  pSrc Second byte array operand for XOR
 *
 * \param[in]  n  Length of the byte array which will be XORed
 *
 ********************************************************************************** */
void SecLib_XorN(uint8_t *pDst, const uint8_t *pSrc, uint8_t n)
{
    while (n != 0U)
    {
        *pDst = *pDst ^ *pSrc;
        pDst  = pDst + 1;
        pSrc  = pSrc + 1;
        n--;
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
    enum
    {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
        key_bits   = AES_128_KEY_BYTE_LEN,
    };
    const psa_algorithm_t alg = PSA_ALG_ECB_NO_PADDING;

    psa_status_t         status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t         key        = 0;
    size_t               output_len = 0;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);

    do
    {
        status = psa_import_key(&attributes, pKey, key_bits, &key);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_cipher_encrypt(key, alg, pInput, AES_BLOCK_SIZE, pOutput, AES_BLOCK_SIZE, &output_len);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_destroy_key(key);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-ECB encryption on a message block.
 *         This function only accepts input lengths which are multiple
 *         of 16 bytes (AES 128 block size).
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
    enum
    {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
        key_bits   = AES_128_KEY_BYTE_LEN,
    };
    const psa_algorithm_t alg = PSA_ALG_ECB_NO_PADDING;

    psa_status_t         status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t         key        = 0;
    size_t               output_len = 0;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);

    do
    {
        status = psa_import_key(&attributes, pKey, key_bits, &key);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_cipher_encrypt(key, alg, pInput, inputLen, pOutput, inputLen, &output_len);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_destroy_key(key);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

/*! *********************************************************************************
 * \brief  This function performs AES-128 decryption on a 16-byte block.
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
void AES_128_Decrypt(const uint8_t *pInput, const uint8_t *pKey, uint8_t *pOutput)
{
    enum
    {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
        key_bits   = AES_128_KEY_BYTE_LEN,
    };
    const psa_algorithm_t alg = PSA_ALG_ECB_NO_PADDING;

    psa_status_t         status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t         key        = 0;
    size_t               output_len = 0;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);

    do
    {
        status = psa_import_key(&attributes, pKey, key_bits, &key);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_cipher_decrypt(key, alg, pInput, AES_BLOCK_SIZE, pOutput, AES_128_BLOCK_SIZE, &output_len);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_destroy_key(key);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

/*! *********************************************************************************
 * \brief  This function performs AES-128 decryption on a 16-byte block.
 *
 * \param[in]  pInput Pointer to the location of the 16-byte plain text block.
 *
 * \param[in]  inputLen Input message length in bytes.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte ciphered output.
 *
 * \pre All Input/Output pointers must refer to a memory address aligned to 4 bytes!
 *
 ********************************************************************************** */
void AES_128_ECB_Decrypt(const uint8_t *pInput, uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput)
{
    enum
    {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
        key_bits   = AES_128_KEY_BYTE_LEN,
    };
    const psa_algorithm_t alg = PSA_ALG_ECB_NO_PADDING;

    psa_status_t         status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t         key        = 0;
    size_t               output_len = 0;

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);

    do
    {
        status = psa_import_key(&attributes, pKey, key_bits, &key);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_cipher_decrypt(key, alg, pInput, inputLen, pOutput, inputLen, &output_len);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_destroy_key(key);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

/*! *********************************************************************************
 * \brief  This function performs AES-128-CMAC on a message block accepting input data
 *         which is in LSB first format and computing the authentication code
 *         starting from the end of the data.
 *
 * \param[in]  pInput Pointer to the location of the input message.
 *
 * \param[in]  inputLen Length of the input message in bytes.
 *             The input data must be provided LSB first.
 *
 * \param[in]  pKey Pointer to the location of the 128-bit key.
 *              The key must be provided MSB first.
 *
 * \param[out]  pOutput Pointer to the location to store the 16-byte authentication code.
 *              The code will be generated MSB first.
 *
 ********************************************************************************** */
void AES_128_CMAC_LsbFirstInput(const uint8_t *pInput, uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput)
{
    enum
    {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
        key_bits   = AES_128_KEY_BYTE_LEN,
    };
    const psa_algorithm_t alg = PSA_ALG_CMAC;

    psa_status_t         status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t         key        = 0;
    size_t               output_len = 0;

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);

    do
    {
        status = psa_import_key(&attributes, pKey, key_bits, &key);
        RAISE_ERROR(status, PSA_SUCCESS);

        psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

        status = psa_mac_sign_setup(&operation, key, alg);
        RAISE_ERROR(status, PSA_SUCCESS);

        /* Walk the input buffer from the end to the start and reverse the blocks
         * before calling the CMAC update function. */
        uint8_t reversedBlock[AES_128_BLOCK_SIZE] = {0};
        pInput += inputLen;
        do
        {
            uint32_t currentCmacInputBlkLen = 0;
            if (inputLen < AES_128_BLOCK_SIZE)
            {
                /* If this is the first and single block it is legal for it to have an input length of 0
                 * in which case nothing will be copied in the reversed CMAC input buffer. */
                currentCmacInputBlkLen = inputLen;
            }
            else
            {
                currentCmacInputBlkLen = AES_128_BLOCK_SIZE;
            }
            pInput -= currentCmacInputBlkLen;
            inputLen -= currentCmacInputBlkLen;
            /* Copy the input block to the reversed CMAC input buffer */
            FLib_MemCpyReverseOrder(reversedBlock, pInput, currentCmacInputBlkLen);

            status = psa_mac_update(&operation, reversedBlock, currentCmacInputBlkLen);
            RAISE_ERROR(status, PSA_SUCCESS);

        } while (inputLen != 0U);

        status = psa_mac_sign_finish(&operation, pOutput, PSA_MAC_LENGTH(PSA_KEY_TYPE_AES, key_bits, alg), &output_len);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_destroy_key(key);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

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
    enum
    {
        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(PSA_KEY_TYPE_AES),
        key_bits   = AES_128_KEY_BYTE_LEN,
    };
    const psa_algorithm_t alg = PSA_ALG_CMAC;

    psa_status_t         status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t         key        = 0;
    size_t               output_len = 0;

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);

    do
    {
        status = psa_import_key(&attributes, pKey, key_bits, &key);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_mac_compute(key, alg, pInput, inputLen, pOutput, AES_BLOCK_SIZE, &output_len);
        RAISE_ERROR(status, PSA_SUCCESS);

        status = psa_destroy_key(key);
        RAISE_ERROR(status, PSA_SUCCESS);
    } while (false);
}

/************************************************************************************
 * \brief Computes the Diffie-Hellman Key for an ECDH P256 key pair.
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_ComputeDhKeySeg(computeDhKeyParam_t *pDhKeyData)
{
    return ECDH_P256_ComputeDhKey(&pDhKeyData->privateKey, &pDhKeyData->peerPublicKey, &pDhKeyData->outPoint,
                                  pDhKeyData->keepInternalBlob);
}
#include "fsl_component_mem_manager.h"
/************************************************************************************
 * \brief Computes the Diffie-Hellman Key for an ECDH P256 key pair.
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_ComputeDhKey(const ecdhPrivateKey_t *pInPrivateKey,
                                       const ecdhPublicKey_t  *pInPeerPublicKey,
                                       ecdhDhKey_t            *pOutDhKey,
                                       const bool_t            keepBlobDhKey)
{
    secResultType_t ret = gSecSuccess_c;
    uint8_t         bufPriv[sizeof(ecdhPrivateKey_t)];
    uint8_t         bufPub[sizeof(ecdhPublicKey_t)];
    uint8_t         bufSecret[sizeof(ecdhDhKey_t)];
    do
    {
        if (pOutDhKey == NULL)
        {
            ret = gSecError_c;
            break;
        }

        if (!ECP256_LePointValid(pInPeerPublicKey))
        {
            ret = gSecInvalidPublicKey_c;
            break;
        }

        /*little-indian to big-indian*/
        ECP256_PointCopy_and_change_endianness(bufPub, pInPeerPublicKey->raw);
        ECP256_coordinate_copy(bufPriv, psa_pECPKeyPair->PrivateKey->raw_8bit);

        psa_status_t status = p256_ecdh_shared_secret(bufSecret, (uint8_t const *)bufPriv, (uint8_t const *)bufPub);
        RAISE_ERROR(status, PSA_SUCCESS);

        /*big-indian to little-indian*/
        ECP256_PointCopy_and_change_endianness(pOutDhKey->raw, bufSecret);

    } while (false);
    return ret;
}

/************************************************************************************
 * \brief Generates a new ECDH P256 Private/Public key pair
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_GenerateKeysSeg(computeDhKeyParam_t *pDhKeyData)
{
    return ECDH_P256_GenerateKeys(&pDhKeyData->outPoint, &pDhKeyData->privateKey);
}

/************************************************************************************
 * \brief Generates a new ECDH P256 Private/Public key pair
 *
 * \return gSecSuccess_c or error
 *
 ************************************************************************************/
secResultType_t ECDH_P256_GenerateKeys(ecdhPublicKey_t *pOutPublicKey, ecdhPrivateKey_t *pOutPrivateKey)
{
    secResultType_t ret = gSecSuccess_c;
    psa_status_t    st  = PSA_SUCCESS;
    do
    {
        if (psa_pECPKeyPair != NULL)
        {
            /* Once the key oject gets destroyed context is not ready anymore */
            psa_destroy_key(psa_pECPKeyPair->OwnKey);
            FLib_MemSet(psa_pECPKeyPair, 0, sizeof(psa_ecp256_context_t));
            psa_pECPKeyPair = NULL;
        }

        /* psa_g_ECP_KeyPair.keyId = KEY_ID_BLE0; */

        psa_pECPKeyPair = &psa_g_ECP_KeyPair;

        st = p256_gen_keypair(psa_pECPKeyPair->PrivateKey->raw_8bit, psa_pECPKeyPair->OwnPublicKey->raw_8bit);
        RAISE_ERROR(st, PSA_SUCCESS)

        /*big-indian to little-indian*/
        ECP256_PointCopy_and_change_endianness(pOutPublicKey->raw, psa_pECPKeyPair->OwnPublicKey->raw_8bit);
    } while (false);
    return ret;
}

/************************************************************************************
 * \brief Function used to create the mac key and LTK using Bluetooth F5 algorithm.
 *        Less secure version not using secure bus.
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
 *
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
            result = gSecBadArgument_c;
            break;
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

/*! *********************************************************************************
 * \brief  This function implements the SMP ah cryptographic toolbox function which
           calculates the hash part of a Resolvable Private Address.
 *         The key is kept in plaintext.
 *
 * \param[out]  pHash  Pointer where the 24 bit hash of a Resolvable Private Address value
 *                     will be written.
 *
 * \param[in]  pKey  Pointer to the 128 bit key.
 *
 * \param[in]  pR   Pointer to the 24 bit random value (Prand) of a Resolvable private Address.
 *                  The most significant bits of this field must be 0b01 for Resolvable Private
 *                  Addresses.
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