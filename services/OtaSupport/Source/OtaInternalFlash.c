/*
 * Copyright 2021-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file
 *
 * This is the Source file for the EEPROM emulated inside the MCU's FLASH
 *
 */
#include "FunctionLib.h"
#include "OtaPrivate.h"
#include "fsl_adapter_flash.h"
#include "fwk_platform_ota.h"
#include <stdbool.h>
#include "fwk_hal_macros.h"
#include "fwk_platform_definitions.h"

/******************************************************************************
*******************************************************************************
* Private Macros
*******************************************************************************
******************************************************************************/
#define RAISE_ERROR(x, val) \
    {                       \
        x = (val);          \
        break;              \
    }

#ifndef PLATFORM_INTFLASH_TOTAL_SIZE
/* PLATFORM_INTFLASH_TOTAL_SIZE has to be defined for MCXW23x platforms */
#define PLATFORM_INTFLASH_TOTAL_SIZE FSL_FEATURE_FLASH_PFLASH_BLOCK_SIZE
#endif

#define StorageNbMaxSectors (PLATFORM_INTFLASH_TOTAL_SIZE / FSL_FEATURE_FLASH_PFLASH_SECTOR_SIZE)
/* Size of storage sector bit map array */
#define StorageSectorBitmapSize (StorageNbMaxSectors / 32U)

#if defined(FSL_FEATURE_FLASH_PFLASH_PHRASE_SIZE)
#define gEepromParams_WriteAlignment_c FSL_FEATURE_FLASH_PFLASH_PHRASE_SIZE
#elif defined(FSL_FEATURE_FLASH_PHRASE_SIZE_BYTES) /* Different naming for W23 */
#define gEepromParams_WriteAlignment_c FSL_FEATURE_FLASH_PHRASE_SIZE_BYTES
#endif

#ifndef gEepromParams_WriteAlignment_c
#define gEepromParams_WriteAlignment_c (1U)
#endif

#define IntStorageSectorSize ((uint32_t)FSL_FEATURE_FLASH_PFLASH_SECTOR_SIZE)
#define gIntFlashNbSectors   ((uint16_t)((ota_internal_partition->size / IntStorageSectorSize) & (uint32_t)UINT16_MAX))

/* Convert relative offset in internal storage into real address */
#define PHYS_ADDR(x, start_offset) ((uint32_t)(x) + (uint32_t)start_offset)

#define IS_PHRASE_ALIGNED(x)   (((x) & (FSL_FEATURE_FLASH_PFLASH_PHRASE_SIZE - 1U)) == 0U)
#define IS_SECTOR_ALIGNED(x)   (((x) & (IntStorageSectorSize - 1U)) == 0U)
#define CURRENT_SECTOR_ADDR(x) (((x) & ~(IntStorageSectorSize - 1U)))
#define NEXT_SECTOR_ADDR(x)    (CURRENT_SECTOR_ADDR(x) + IntStorageSectorSize)

/******************************************************************************
*******************************************************************************
* Private Prototypes
*******************************************************************************
******************************************************************************/
STATIC ota_flash_status_t InternalFlash_PrepareForWrite(uint32_t NoOfBytes, uint32_t offs);
STATIC ota_flash_status_t InternalFlash_EraseBlockBySectorNumber(uint16_t blk_nb);
STATIC ota_flash_status_t InternalFlash_Init(void);
STATIC ota_flash_status_t InternalFlash_PartitionErase(void);
STATIC ota_flash_status_t InternalFlash_WriteData(uint32_t NoOfBytes, uint32_t offs, uint8_t *Outbuf);
STATIC ota_flash_status_t InternalFlash_FlushWriteBuffer(void);
STATIC ota_flash_status_t InternalFlash_ReadData(uint16_t NoOfBytes, uint32_t offs, uint8_t *inbuf);
STATIC ota_flash_status_t InternalFlash_EraseArea(uint32_t *pOffs, uint32_t *pSize, bool non_blocking);
STATIC uint8_t            InternalFlash_isBusy(void);
#if defined               OtaDeprecatedFlashVerifyWrite_d && (OtaDeprecatedFlashVerifyWrite_d > 0)
STATIC ota_flash_status_t InternalVerifyFlashProgram(uint8_t *pData, uint32_t offs, uint32_t length);
#endif
static int GetEraseBitMap(uint16_t blk_nb);
static int SetEraseBitMap(uint16_t blk_nb, bool setNclr);

/******************************************************************************
*******************************************************************************
* Private type definitions
*******************************************************************************
******************************************************************************/

union physical_address
{
    uint32_t  value;
    uint32_t *pointer;
};

/******************************************************************************
*******************************************************************************
* Private Memory Declarations
*******************************************************************************
******************************************************************************/
static uint32_t mEraseBitmap[StorageSectorBitmapSize];
#if (gEepromParams_WriteAlignment_c > 1)
static uint8_t  mWriteBuff[gEepromParams_WriteAlignment_c];
static uint32_t mWriteBuffLevel = 0U;
static uint32_t mWriteBuffOffs  = 0U;
#endif

/******************************************************************************
*******************************************************************************
* Public Memory
*******************************************************************************
******************************************************************************/

static const OtaFlashOps_t int_flash_ops = {
    .init           = &InternalFlash_Init,
    .format_storage = &InternalFlash_PartitionErase,
    .writeData      = &InternalFlash_WriteData,
    .readData       = &InternalFlash_ReadData,
    .isBusy         = &InternalFlash_isBusy,
    .eraseArea      = &InternalFlash_EraseArea,
    .flushWriteBuf  = &InternalFlash_FlushWriteBuffer,
};

static const OtaPartition_t *ota_internal_partition;
static uint32_t              partition_start_addr = 0UL;

/******************************************************************************
*******************************************************************************
* Private Functions
*******************************************************************************
******************************************************************************/

/*! *********************************************************************************
 * \brief  Check whether [offset, offset+range_sz) fits within the OTA partition.
 *
 * Uses subtraction-based comparison to avoid unsigned wrap-around in the addition
 * (CERT INT30-C).
 *
 * \return   true if the range belongs to OTA partition, false otherwise.
 *
 ***********************************************************************************/
STATIC bool OtaCheckRangeBelongsToPartition(uint32_t offset, uint32_t range_sz)
{
    /* Avoid "offset + range_sz" wrap: check range_sz <= (size - offset) instead */
    return (offset <= ota_internal_partition->size) && (range_sz <= (ota_internal_partition->size - offset));
}

/*! *********************************************************************************
 * \brief  Initialize internal storage for OTA.
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 *
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_Init(void)
{
    ota_flash_status_t status    = kStatus_OTA_Flash_Success;
    static bool        init_done = false;
    if (!init_done)
    {
        /*  Ensure that Storage BitMap size is sufficient to deal with the entire
            internal storage area : here the sector size is 8kB and the storage size
            is ~ 512kB */
        /* Wipe clean the EraseBitMap */
        FLib_MemSet(mEraseBitmap, 0x00, sizeof(mEraseBitmap));
#if (gEepromParams_WriteAlignment_c > 1)
        FLib_MemSet(mWriteBuff, 0xff, gEepromParams_WriteAlignment_c);
        mWriteBuffLevel = 0U;
        mWriteBuffOffs  = 0U;
#endif

        status = (ota_flash_status_t)HAL_FlashInit();
    }
    return status;
}

/*! *********************************************************************************
 * \brief  Clean internal storage partition by erasing all sectors
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 *
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_PartitionErase(void)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;

    /* gIntFlashNbSectors is not a constant, it depends on ota_internalPartition->size */
    for (uint16_t i = 0U; i < (uint16_t)(gIntFlashNbSectors & 0xffffUL); i++)
    {
        /* Erase sectors one by one */
        status = InternalFlash_EraseBlockBySectorNumber(i);
        if (kStatus_OTA_Flash_Success != status)
        {
            break;
        }
    }

    return status;
}

/*! *********************************************************************************
 * \brief  Writes a data buffer into internal storage and checks operation
 *
 * \param[in] NoOfBytes   Number of bytes to be written
 * \param[in] offs        offset address relative to start of Internal Storage
 * \param[in] Outbuf      pointer on buffer to be written
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlashWriteAndVerify(uint32_t NoOfBytes, uint32_t offs, uint8_t *Outbuf)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;

    do
    {
        union physical_address write_address;
        write_address.value = PHYS_ADDR(offs, partition_start_addr);

        if (kStatus_HAL_Flash_Success != HAL_FlashProgramUnaligned(write_address.value, NoOfBytes, Outbuf))
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_Fail);
        }
#if defined OtaDeprecatedFlashVerifyWrite_d && (OtaDeprecatedFlashVerifyWrite_d > 0)
        /* Do flash program verification where we best know what we did write */
        status = InternalVerifyFlashProgram(mWriteBuff, write_address.value, gEepromParams_WriteAlignment_c);
        if (kStatus_OTA_Flash_Success != status)
        {
            break;
        }
#endif

    } while (false);

    return status;
}

#if (gEepromParams_WriteAlignment_c > 1)
/*! *********************************************************************************
 * \brief  Continue writing to flash phrase staging buffer until completion.
 *
 * \param[out] pOffs      pointer on offset to be updated for caller.
 * \param[out] pNoOfBytes  pointer on number of bytes to be update for caller.
 * \param[out] pOutbuf     pointer on output buffer pointer to be updated for caller.
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t AppendAndFlushPendingWriteBuffer(uint32_t *pOffs, uint32_t *pNoOfBytes, uint8_t **pOutbuf)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;

    uint32_t offs      = *pOffs;
    uint32_t NoOfBytes = *pNoOfBytes;
    uint8_t *Outbuf    = *pOutbuf;
    /* There was something pending in mWriteBuff already */
    do
    {
        if (mWriteBuffLevel >= (uint32_t)sizeof(mWriteBuff))
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_Error);
        }
        uint32_t size_to_copy = ~0UL;
        /* Check if offs is within the range covered by mWriteBuff  */
        if (offs >= mWriteBuffOffs)
        {
            /* coverity[overflow:FALSE] (offs - mWriteBuffOffs) cannot underflow if (offs >= mWriteBuffOffs) */
            uint32_t buf_offs = (offs - mWriteBuffOffs);
            if (buf_offs < (uint32_t)sizeof(mWriteBuff))
            {
                if (mWriteBuffLevel != buf_offs)
                {
                    RAISE_ERROR(status, kStatus_OTA_Flash_Error);
                }

                buf_offs &= ((uint32_t)gEepromParams_WriteAlignment_c - 1UL);
                /* The target offset for writing resides within the mWriteBuff phrase buffer */
                uint32_t remain_till_end_of_phrase = ((uint32_t)sizeof(mWriteBuff) - buf_offs);
                size_to_copy                       = MIN(NoOfBytes, remain_till_end_of_phrase);
                /* The check (offs >= mWriteBuffOffs) ascertains that phrase_offset will not wrap around */
                FLib_MemCpy(&mWriteBuff[buf_offs], Outbuf, size_to_copy);
                mWriteBuffLevel += size_to_copy;
                offs += size_to_copy;
                Outbuf += size_to_copy;
                NoOfBytes = (NoOfBytes > size_to_copy) ? (NoOfBytes - size_to_copy) : 0U;
            }
        }
        if (size_to_copy == ~0UL)
        {
            /* The incoming data to append did not belong to the same range as that of mWriteBuff.
             * Pad with 0xff till end of buffer.
             */
            FLib_MemSet(&mWriteBuff[mWriteBuffLevel], 0xFF, (uint32_t)sizeof(mWriteBuff) - mWriteBuffLevel);
            mWriteBuffLevel = (uint32_t)sizeof(mWriteBuff);
        }

        if (mWriteBuffLevel < (uint32_t)sizeof(mWriteBuff))
        {
            /* not enough bytes yet — early exit, no flash write */
            status = kStatus_OTA_Flash_Success;
            break;
        }

        status = InternalFlashWriteAndVerify((uint32_t)sizeof(mWriteBuff), mWriteBuffOffs, mWriteBuff);
        if (kStatus_OTA_Flash_Success == status)
        {
            mWriteBuffLevel = 0U;
        }
    } while (false);
    *pOffs      = offs;
    *pNoOfBytes = NoOfBytes;
    *pOutbuf    = Outbuf;

    return status;
}
#endif

/*! *********************************************************************************
 * \brief  Writes a data buffer into internal storage, at a given address
 *
 * \param[in] NoOfBytes   Number of bytes to be written
 * \param[in] offs        offset address relative to start of Internal Storage
 * \param[in] Outbuf      pointer on buffer to be written
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_WriteData(uint32_t NoOfBytes, uint32_t offs, uint8_t *Outbuf)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Fail;
    do
    {
        if (0U == NoOfBytes)
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }

        status = InternalFlash_PrepareForWrite(NoOfBytes, offs);
        if (kStatus_OTA_Flash_Success != status)
        {
            break;
        }
#if (gEepromParams_WriteAlignment_c > 1)
        if (mWriteBuffLevel >= sizeof(mWriteBuff))
        {
            /* mWriteBuffLevel must remain smaller than a mWriteBuff (a phrase)  */
            RAISE_ERROR(status, kStatus_OTA_Flash_AlignmentError);
        }
        if (mWriteBuffLevel != 0U)
        {
            status = AppendAndFlushPendingWriteBuffer(&offs, &NoOfBytes, &Outbuf);
            if ((status != kStatus_OTA_Flash_Success) || (NoOfBytes == 0U))
            {
                break;
            }
        }

        if ((offs & (gEepromParams_WriteAlignment_c - 1U)) != 0U)
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_AlignmentError);
        }
        /* Store unaligned bytes for later processing */
        mWriteBuffLevel = NoOfBytes % gEepromParams_WriteAlignment_c;
        NoOfBytes -= mWriteBuffLevel;
        /* offs + NoOfBytes cannot wrap: OtaCheckRangeBelongsToPartition() already verified
         * that (offs + original_NoOfBytes) <= partition size (a uint32_t value). */
        /* coverity[overflow:FALSE] */
        mWriteBuffOffs = offs + NoOfBytes;
        FLib_MemCpy(mWriteBuff, &Outbuf[NoOfBytes], mWriteBuffLevel);
#endif

        /* Write data to FLASH */
        if (NoOfBytes > 0U)
        {
            status = InternalFlashWriteAndVerify(NoOfBytes, offs, Outbuf);
            if (kStatus_OTA_Flash_Success != status)
            {
                break;
            }
        }
        status = kStatus_OTA_Flash_Success;
    } while (false);
    return status;
}

/*! *********************************************************************************
 * \brief  Writes remainder of 16 byte buffer to flash when terminating FW update
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_FlushWriteBuffer(void)
{
    ota_flash_status_t status;
#if (gEepromParams_WriteAlignment_c > 1)
    do
    {
        uint32_t size;

        if (mWriteBuffLevel == 0U)
        {
            status = kStatus_OTA_Flash_Success;
            break;
        }
        size = gEepromParams_WriteAlignment_c - mWriteBuffLevel;
        /* Pad the remainder of write buffer with 0xff fill */
        FLib_MemSet(&mWriteBuff[mWriteBuffLevel], 0xffu, (uint32_t)size);
        /* the write RAM Write Buffer to FLASH */
        status = InternalFlashWriteAndVerify(gEepromParams_WriteAlignment_c, mWriteBuffOffs, mWriteBuff);
        if (kStatus_OTA_Flash_Success != status)
        {
            break;
        }
        mWriteBuffLevel = 0U;
        status          = kStatus_OTA_Flash_Success;
    } while (false);
#else
    status = kStatus_OTA_Flash_Success;
#endif
    return status;
}

/*! *********************************************************************************
 * \brief  Read data from an address pointing to internal flash to a RAM buffer
 *
 * \param[in] NoOfBytes   Number of bytes to be read
 * \param[in] offs        offset address relative to start of Internal Storage
 * \param[in] Outbuf      pointer on buffer to be write to
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_ReadData(uint16_t NoOfBytes, uint32_t offs, uint8_t *inbuf)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;

    if (!OtaCheckRangeBelongsToPartition(offs, NoOfBytes))
    {
        status = kStatus_OTA_Flash_InvalidArgument;
    }
    else
    {
#if (gEepromParams_WriteAlignment_c > 1)
        /* Part of the data may have to be read from flash */
        uint32_t end_offs          = offs + NoOfBytes;
        uint32_t end_offs_inRam    = mWriteBuffOffs + mWriteBuffLevel;
        uint32_t index_in_read_buf = 0U;
        while (offs < end_offs)
        {
            if ((offs >= mWriteBuffOffs) && (offs < end_offs_inRam))
            {
                uint32_t internal_ram_idx = offs - mWriteBuffOffs;
                inbuf[index_in_read_buf]  = mWriteBuff[internal_ram_idx];
            }
            else
            {
                inbuf[index_in_read_buf] = *(uint8_t *)PHYS_ADDR(offs, partition_start_addr);
            }
            index_in_read_buf++;
            offs++;
        }
#else
        /* Read data from flash, there is no staging buffer */
        union physical_address read_address;

        read_address.value = PHYS_ADDR(offs, partition_start_addr);
        FLib_MemCpy(inbuf, read_address.pointer, NoOfBytes);
#endif
    }

    return status;
}

/*! *********************************************************************************
 * \brief  Return busy status
 *
 * \return    always false in the case of internal flash
 ***********************************************************************************/
STATIC uint8_t InternalFlash_isBusy(void)
{
    return 0U;
}

/*! *********************************************************************************
 * \brief Get mEraseBitmap bit field value indicating whether sector is blank.
 *
 * \param blk_nb index of sector
 *
 * \return -1 if blk_nb is too large,
 *          0 if bit is cleared (sector not blank),
 *          1 if bit is set (sector blank)
 ***********************************************************************************/
static int GetEraseBitMap(uint16_t blk_nb)
{
    int ret = -1;

    if (blk_nb < (uint16_t)StorageNbMaxSectors)
    {
        /* Block number is less than StorageNbMaxSectors */
        uint8_t map_index;
        uint8_t map_shift;
        ret = 0;
        /* map_index is guaranteed to be smaller than (StorageNbMaxSectors / 32),
         * which is the size of the mEraseBitmap array.
         */
        map_index = (uint8_t)(((uint32_t)blk_nb >> 5) & 0xffU);
        map_shift = (uint8_t)(blk_nb & 0x1fU);
        if ((mEraseBitmap[map_index] & ((uint32_t)1U << map_shift)) != 0U)
        {
            /* Sector blank */
            ret = 1;
        }
    }
    return ret;
}

/*! *********************************************************************************
 * \brief Set or clear bit in mEraseBitmap bit field.
 *
 * \param blk_nb index of sector
 * \param setNclr set bit if true, clear if false.
 *
 * \return -1 if blk_nb is too large, 0 otherwise
 ***********************************************************************************/
static int SetEraseBitMap(uint16_t blk_nb, bool setNclr)
{
    int ret = -1;

    if (blk_nb < (uint16_t)StorageNbMaxSectors)
    {
        /* Block number is less than StorageNbMaxSectors */
        uint8_t map_index;
        uint8_t map_shift;

        ret       = 0;
        map_index = (uint8_t)(((uint32_t)blk_nb >> 5) & 0xffU);
        /* map_index is guaranteed to be smaller than (StorageNbMaxSectors / 32),
         * which is the size of the mEraseBitmap array.
         */
        map_shift = (uint8_t)(blk_nb & 0x1fU);
        if (setNclr)
        {
            /* Write bit in bit field : sector is marked as blank */
            mEraseBitmap[map_index] |= ((uint32_t)1U << map_shift);
        }
        else
        {
            /* Clear bit in bit field : sector is not blank / or do not know */
            mEraseBitmap[map_index] &= ~((uint32_t)1U << map_shift);
        }
    }
    return ret;
}

/*! *********************************************************************************
 * \brief  Erase sector identified by its number in OTA partition.
 *
 * \return    kStatus_OTA_Flash_Success if operation successful,
 *             kStatus_OTA_Flash_Error  otherwise.
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_EraseBlockBySectorNumber(uint16_t blk_nb)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Error;
    do
    {
        // assert(gIntFlashNbSectors < StorageNbMaxSectors);

        if (blk_nb >= gIntFlashNbSectors)
        {
            assert(0);
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }

        /* blk_nb is constrained to remain smaller than the number of sectors in the partition no possible overflow
         */
        /* coverity [overflow_sink:FALSE] */
        if (HAL_FlashEraseSector(PHYS_ADDR(blk_nb * IntStorageSectorSize, partition_start_addr),
                                 IntStorageSectorSize) != kStatus_HAL_Flash_Success)
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_Error);
        }
        /* mark each erased sector as such in the erase bit map */
        /* If blocks are 8kBytes large, one byte of bitmap is required per 64kByte tranche */
        if (SetEraseBitMap(blk_nb, true) < 0)
        {
            assert(0);
            RAISE_ERROR(status, kStatus_OTA_Flash_Error);
        }
        status = kStatus_OTA_Flash_Success;

    } while (false);
    return status;
}

/*! *********************************************************************************
 * \brief  Erase sector only if not known to be blank.
 *
 * \param[in] blk_nb sector number to be erased
 *
 * \return    kStatus_OTA_Flash_Success if operation successful,
 *             kStatus_OTA_Flash_InvalidArgument if blk_nb is invalid
 *             kStatus_OTA_Flash_Error  otherwise.
 ***********************************************************************************/
STATIC ota_flash_status_t EraseOneSectorIfNeeded(uint16_t blk_nb)
{
    ota_flash_status_t status;
    do
    {
        int erased_status = GetEraseBitMap(blk_nb);

        if (erased_status < 0)
        {
            /* Is a sign that blk_nb is too big */
            assert(0);
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }
        if (erased_status == 0)
        {
            /* Was not blank or state is unknown : request erase */
            status = InternalFlash_EraseBlockBySectorNumber(blk_nb);
        }
        else
        {
            /* nothing to do */
            status = kStatus_OTA_Flash_Success;
        }
    } while (false);

    return status;
}

/*! *********************************************************************************
 * \brief  Erase number of sectors required for an upcoming program operation
 *
 * \param[in] NoOfBytes   Number of bytes of area to be cleared already checked to be non 0
 *            will be rounded to a multiple of flash sectors
 * \param[in] offs        offset address relative to start of Internal Storage from which
 *            erase operation is required
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t InternalFlash_PrepareForWrite(uint32_t NoOfBytes, uint32_t offs)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;
    uint32_t           startBlk;
    uint32_t           endBlk;

    do
    {
        /* Validate the range : commonlize call for InternalFlash_EraseArea and InternalFlash_WriteData */
        if (!OtaCheckRangeBelongsToPartition(offs, NoOfBytes))
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }

        /* startBlk is the number of the first sector where range starts */
        startBlk = (CURRENT_SECTOR_ADDR(offs) / IntStorageSectorSize);
        /* endBlk is the number of the first sector after the range,  at least one sector further
         * since NoOfBytes was checked to be non 0*/
        endBlk = startBlk + (NoOfBytes / IntStorageSectorSize);
        if (endBlk < (uint32_t)gIntFlashNbSectors)
        {
            endBlk++;
        }
        /* clamp to uint16_t width */
        startBlk &= (uint32_t)UINT16_MAX;
        endBlk &= (uint32_t)UINT16_MAX;

        for (uint16_t i = (uint16_t)startBlk; i < (uint16_t)endBlk; i++)
        {
            status = EraseOneSectorIfNeeded(i);
            if (kStatus_OTA_Flash_Success != status)
            {
                break; /* for */
            }
        }
    } while (false);

    return status;
}

STATIC ota_flash_status_t InternalFlash_EraseArea(uint32_t *pOffs, uint32_t *pSize, bool non_blocking)
{
    ota_flash_status_t status;

    /* The erase operation in internal flash is necessarily blocking */
    NOT_USED(non_blocking);

    do
    {
        uint32_t remain_sz  = *pSize;
        uint32_t erase_offs = *pOffs;
        if (!IS_SECTOR_ALIGNED(erase_offs))
        {
            /* Ensure erase offset is sector-aligned */
            RAISE_ERROR(status, kStatus_OTA_Flash_AlignmentError);
        }

        if (remain_sz != 0U)
        {
            status = InternalFlash_PrepareForWrite(remain_sz, erase_offs);
            if (status != kStatus_OTA_Flash_Success)
            {
                break;
            }
            erase_offs += remain_sz; /* Advance offset by remaining size */
            /* In the case of internal flash the entire erase operation must
             * complete till the end
             */
            remain_sz = 0U;
        }
        *pOffs = NEXT_SECTOR_ADDR(erase_offs); /* Round up to next sector boundary */
        *pSize = remain_sz;
        status = kStatus_OTA_Flash_Success;
    } while (false);

    return status;
}
#if (defined OtaDeprecatedFlashVerifyWrite_d && (OtaDeprecatedFlashVerifyWrite_d > 0))
STATIC ota_flash_status_t InternalVerifyFlashProgram(uint8_t *pData, uint32_t offs, uint32_t length)
{
    ota_flash_status_t     status = kStatus_OTA_Flash_Success;
    union physical_address verify_address;
    verify_address.value = PHYS_ADDR(offs, partition_start_addr);
    if (!FLib_MemCmp(pData, verify_address.pointer, length))
    {
        status = kStatus_OTA_Flash_Fail;
    }
    return status;
}
#endif /* OtaDeprecatedFlashVerifyWrite_d */

otaResult_t OTA_SelectInternalStoragePartition(void)
{
    otaResult_t    status = gOtaInternalFlashError_c;
    OtaStateCtx_t *hdl    = &mOtaHdl;
    do
    {
        hal_flash_status_t flashInitStatus;

        if (hdl->FwUpdImageState == OtaImgState_Acquiring)
        {
            RAISE_ERROR(status, gOtaInvalidOperation_c);
        }

        flashInitStatus = HAL_FlashInit();

        if (flashInitStatus != kStatus_HAL_Flash_Success)
        {
            RAISE_ERROR(status, gOtaError_c);
        }

        OTA_DEBUG_TRACE("Select Internal flash\r\n");

        ota_internal_partition = PLATFORM_OtaGetOtaInternalPartitionConfig();
        if (ota_internal_partition == NULL)
        {
            RAISE_ERROR(status, gOtaInvalidParam_c);
        }

        hdl->FlashOps = &int_flash_ops;
#if defined FSL_FEATURE_FLASH_PFLASH_START_ADDRESS && (FSL_FEATURE_FLASH_PFLASH_START_ADDRESS > 0)
        if (ota_internal_partition->start_offset < FSL_FEATURE_FLASH_PFLASH_START_ADDRESS)
        {
            RAISE_ERROR(status, gOtaInvalidParam_c);
        }
#endif
        if (/*(ota_internal_partition->size >= (FSL_FEATURE_FLASH_PFLASH_BLOCK_SIZE - IntStorageSectorSize)) ||*/
            /* Likewise it could never be smaller than 2 flash sectors */
            (ota_internal_partition->size < (2U * IntStorageSectorSize)) ||
            (IntStorageSectorSize != ota_internal_partition->sector_size))
        {
            /* This might happen if target is not generated with gUseInternalStorageLink_d=1 */
            RAISE_ERROR(status, gOtaInvalidParam_c);
        }

        hdl->ota_partition = ota_internal_partition;

        partition_start_addr = ota_internal_partition->start_offset;

        hdl->ImageOffset       = PLATFORM_OtaGetImageOffset();
        hdl->MaxImageLength    = hdl->ota_partition->size - hdl->ImageOffset;
        hdl->ErasedUntilOffset = 0U;

        status = gOtaSuccess_c;

        /* Try to initialize the OTA Storage */
        if (hdl->FlashOps->init() != kStatus_OTA_Flash_Success)
        {
            RAISE_ERROR(status, gOtaInternalFlashError_c);
        }

    } while (false);

    return status;
}
