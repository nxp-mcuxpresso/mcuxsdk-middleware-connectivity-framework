/*! *********************************************************************************
 * Copyright 2021-2023, 2025-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause.
 *
 * \file
 *
 * This is the Source file for the EEPROM emulated inside the MCU's FLASH
 *
 ********************************************************************************** */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_config.h"
#include "fwk_platform_extflash.h"
#include "fwk_platform_ota.h"
#include "FunctionLib.h"
#include "OtaPrivate.h"
#include "OtaSupport.h"
#include "fwk_hal_macros.h"

/******************************************************************************
*******************************************************************************
* Private Macros
*******************************************************************************
******************************************************************************/

#define ExtStorageSectorSize ((uint32_t)PLATFORM_EXTFLASH_SECTOR_SIZE)
#define gExtFlashNbSectors   ((uint16_t)((ota_ext_partition->size / ExtStorageSectorSize) & (uint32_t)UINT16_MAX))
#define StorageNbMaxSectors  (PLATFORM_EXTFLASH_TOTAL_SIZE / PLATFORM_EXTFLASH_SECTOR_SIZE)

#define OTA_WRITE_BUFFER_SIZE (1U * PLATFORM_EXTFLASH_PAGE_SIZE)

#define IS_SECTOR_ALIGNED(x)   (((x) & (ExtStorageSectorSize - 1U)) == 0U)
#define CURRENT_SECTOR_ADDR(x) (((x) & ~(ExtStorageSectorSize - 1U)))
#define NEXT_SECTOR_ADDR(x)    (CURRENT_SECTOR_ADDR(x) + ExtStorageSectorSize)

#define RAISE_ERROR(x, val) \
    {                       \
        x = (val);          \
        break;              \
    }

/* If gOtaEraseBeforeImageBlockReq_c is defined and enabled, then it means the OTA module will
 * erase the space needed for the next block before writing anything to the flash (OTA_MakeHeadRoomForNextBlock)
 * In such case, we don't need to use an erase bitmap as the erase will always be done before writing.
 */
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
/* The dimension of the Erase bitmap must be sufficient to provide one bit per flash sector. */
#define ERASE_BITMAP_SIZE (StorageNbMaxSectors / 32U)
#endif

/*!< Macro to convert offset relative to start of external storage */
#define PHYS_ADDR(x) ((uint32_t)(x) + ota_ext_partition->start_offset)

/******************************************************************************
*******************************************************************************
* Private Prototypes
*******************************************************************************
******************************************************************************/
STATIC ota_flash_status_t ExternalFlash_Init(void);
STATIC ota_flash_status_t ExternalFlash_PartitionErase(void);
STATIC ota_flash_status_t ExternalFlash_EraseBlock(uint32_t offs, uint32_t size);
STATIC ota_flash_status_t ExternalFlash_WriteData(uint32_t NoOfBytes, uint32_t offs, uint8_t *Outbuf);
STATIC ota_flash_status_t ExternalFlash_FlushWriteBuffer(void);
STATIC ota_flash_status_t ExternalFlash_ReadData(uint16_t NoOfBytes, uint32_t offs, uint8_t *inbuf);
STATIC uint8_t            ExternalFlash_isBusy(void);
STATIC ota_flash_status_t ExternalFlash_EraseArea(uint32_t *pOffs, uint32_t *pSize, bool non_blocking);
#if defined               OtaDeprecatedFlashVerifyWrite_d && (OtaDeprecatedFlashVerifyWrite_d > 0)
STATIC ota_flash_status_t ExternalVerifyFlashProgram(uint8_t *pData, uint32_t offs, uint32_t length);
#endif
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
static int GetEraseBitMap(uint16_t blk_nb);
static int SetEraseBitMap(uint16_t blk_nb, bool setNclr);
#endif

/******************************************************************************
*******************************************************************************
* Private type definitions
*******************************************************************************
******************************************************************************/
struct OtaExtFlashCtx_t
{
    uint8_t mWriteBuffer[OTA_WRITE_BUFFER_SIZE];
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
    uint32_t mEraseBitmap[ERASE_BITMAP_SIZE];
#endif
    uint32_t mWriteBufferOffs;
    uint32_t mWriteBufferIndex;
    bool     init_done;
};

/******************************************************************************
*******************************************************************************
* Private Memory Declarations
*******************************************************************************
******************************************************************************/

static struct OtaExtFlashCtx_t ctx = {
/*    .mWriteBuffer[0 ...(OTA_WRITE_BUFFER_SIZE - 1)] = 0xffU, non standard syntax */
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
/* .mEraseBitmap[0 ... ERASE_BITMAP_SIZE - 1] = 0U, non standard syntax either  */
#endif
    .mWriteBufferOffs  = 0U,
    .mWriteBufferIndex = 0U,
    .init_done         = false,

};
static const OtaPartition_t *ota_ext_partition;

static const OtaFlashOps_t ext_flash_ops = {
    .init           = &ExternalFlash_Init,
    .format_storage = &ExternalFlash_PartitionErase,
    .writeData      = &ExternalFlash_WriteData,
    .readData       = &ExternalFlash_ReadData,
    .isBusy         = &ExternalFlash_isBusy,
    .eraseArea      = &ExternalFlash_EraseArea,
    .flushWriteBuf  = &ExternalFlash_FlushWriteBuffer,
};

/******************************************************************************
*******************************************************************************
* Public Memory
*******************************************************************************
******************************************************************************/

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
    return (offset <= ota_ext_partition->size) && (range_sz <= (ota_ext_partition->size - offset));
}

#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
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
#endif
/*! *********************************************************************************
 * \brief  Initialize External storage for OTA.
 *
 * \return    kStatus_HAL_Flash_Success if successful, other values in case of error
 *
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_Init(void)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;

    do
    {
        if (!ctx.init_done)
        {
            status = kStatus_OTA_Flash_Error;
            status_t st;
            /* Initialize arrays at runtime */
            FLib_MemSet(ctx.mWriteBuffer, 0xFFU, OTA_WRITE_BUFFER_SIZE);

            st = PLATFORM_InitExternalFlash();

#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
            FLib_MemSet(ctx.mEraseBitmap, 0x0U, ERASE_BITMAP_SIZE);
#endif
            if (kStatus_Success != st)
            {
                OTA_DEBUG_TRACE("Init fail %x\r\n", st);
                break;
            }
            ctx.init_done = true;
        }
        status = kStatus_OTA_Flash_Success;
    } while (false);
    return status;
}

/*! *********************************************************************************
 * \brief  Clean External storage partition by erasing all sectors
 *  (so not quite a chip erase)
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 *
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_PartitionErase(void)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;
    uint32_t           i;
    uint32_t           phys_sector_addr;
    for (i = 0U; i < (ota_ext_partition->size / ota_ext_partition->sector_size); i++)
    {
        phys_sector_addr = PHYS_ADDR(i * ota_ext_partition->sector_size);
        if (PLATFORM_IsExternalFlashSectorBlank(phys_sector_addr))
        {
            ; /* let loop continue */
        }
        else
        {
            if (PLATFORM_EraseExternalFlash(phys_sector_addr, ota_ext_partition->sector_size) != kStatus_Success)
            {
                status = kStatus_OTA_Flash_Fail;
                break;
            }
        }
    }
    return status;
}

/*! *********************************************************************************
 * \brief  Erase sector identified by its number in OTA partition.
 *
 * \return    kStatus_OTA_Flash_Success if operation successful,
 *             kStatus_OTA_Flash_Error  otherwise.
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_EraseBlockBySectorNumber(uint16_t blk_nb)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Error;
    do
    {
        assert(gExtFlashNbSectors <= StorageNbMaxSectors);

        if (blk_nb >= gExtFlashNbSectors)
        {
            assert(0);
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }

        if (PLATFORM_EraseExternalFlash(PHYS_ADDR(blk_nb * ExtStorageSectorSize), ExtStorageSectorSize) !=
            kStatus_Success)
        {
            status = kStatus_OTA_Flash_Fail;
            break;
        }
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
        /* mark each erased sector as such in the erase bit map */
        /* If blocks are 8kBytes large, one byte of bitmap is required per 64kByte tranche */
        if (SetEraseBitMap(blk_nb, true) < 0)
        {
            assert(0);
            RAISE_ERROR(status, kStatus_OTA_Flash_Error);
        }
#endif
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
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
        int erased_status = GetEraseBitMap(blk_nb);
        if (erased_status < 0)
        {
            /* Is a sign that blk_nb is too big */
            assert(0);
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }
        if (erased_status == 1)
        {
            /* nothing to do : already blank */
            status = kStatus_OTA_Flash_Success;
        }
        else
#endif
        {
            /* Was not blank or state is unknown : request erase */
            status = ExternalFlash_EraseBlockBySectorNumber(blk_nb);
        }
    } while (false);

    return status;
}

/*! *********************************************************************************
 * \brief  Erase a sector size worth of data in External storage.
 *
 * \param[in] offs     offset address from which erase operation is required
 * \param[in] size     must be gIntFlash_SectorSize_c for External flash
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 *
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_EraseBlock(uint32_t offs, uint32_t size)
{
    ota_flash_status_t status;
    uint32_t           startBlk, endBlk;

    do
    {
        if (!OtaCheckRangeBelongsToPartition(offs, size))
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }
        /* startBlk is the number of the first sector where range starts */
        startBlk = (CURRENT_SECTOR_ADDR(offs) / ExtStorageSectorSize);
        /* endBlk is the number of the first sector after the range,  at least one sector further
         * since NoOfBytes was checked to be non 0*/
        endBlk = startBlk + (size / ExtStorageSectorSize);
        if (endBlk < (uint32_t)gExtFlashNbSectors)
        {
            endBlk++;
        }
        /* clamp to uint16_t width */
        startBlk &= (uint32_t)UINT16_MAX;
        endBlk &= (uint32_t)UINT16_MAX;

        status = kStatus_OTA_Flash_Success;

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

/*! *********************************************************************************
 * \brief  Writes a data buffer into External storage, at a given address
 *
 * \param[in] NoOfBytes   Number of bytes to be written
 * \param[in] offs        offset address relative to start of External Storage
 * \param[in] Outbuf      pointer on buffer to be written
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_WriteData(uint32_t NoOfBytes, uint32_t offs, uint8_t *Outbuf)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;

    do
    {
        if (!OtaCheckRangeBelongsToPartition(offs, NoOfBytes))
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }
        if (0U == NoOfBytes)
        {
            RAISE_ERROR(status, kStatus_OTA_Flash_InvalidArgument);
        }
        if (NoOfBytes == ota_ext_partition->page_size)
        {
            /* Write the current write buffer to flash */
            if (PLATFORM_WriteExternalFlash(Outbuf, ota_ext_partition->page_size, PHYS_ADDR(offs)) != kStatus_Success)
            {
                status = kStatus_OTA_Flash_Fail;
                break;
            }
        }
        else
        {
            while (NoOfBytes > 0U)
            {
                uint32_t sizeToCopy;
                uint32_t pageId   = offs / OTA_WRITE_BUFFER_SIZE;
                uint32_t pageOffs = pageId * OTA_WRITE_BUFFER_SIZE;
                /* COVERITY Impossible but let's satisfy Coverity */
                if (pageOffs > (UINT32_MAX - OTA_WRITE_BUFFER_SIZE))
                {
                    status = kStatus_OTA_Flash_InvalidArgument;
                    break;
                }

                /* Coverity [overflow:FALSE] */ /* pageEndOffs cannot overflow */
                uint32_t pageEndOffs          = pageOffs + OTA_WRITE_BUFFER_SIZE - 1U;
                uint32_t remainingBytesInPage = pageEndOffs - offs + 1U;
                uint32_t offset               = offs - pageOffs;
                bool     isWriteBufferFull    = false;

                ctx.mWriteBufferOffs = pageOffs;

                if (NoOfBytes >= remainingBytesInPage)
                {
                    sizeToCopy        = remainingBytesInPage;
                    isWriteBufferFull = true;
                }
                else
                {
                    sizeToCopy = NoOfBytes;
                }

                /* Copy the data to write in the buffer */
                /* Coverity [overflow_sink:FALSE] */ /* offset is guaranteed to be less than OTA_WRITE_BUFFER_SIZE */
                FLib_MemCpy(&ctx.mWriteBuffer[offset], Outbuf, sizeToCopy);
                ctx.mWriteBufferIndex += sizeToCopy;

                if (isWriteBufferFull == true)
                {
                    /* The write buffer is full, we can store all the data in flash */
                    status = ExternalFlash_FlushWriteBuffer();
                    if (status != kStatus_OTA_Flash_Success)
                    {
                        break;
                    }
                }

                /* Subtract copied bytes from the data to be copied, and increase address offset */
                NoOfBytes -= sizeToCopy;
                offs += sizeToCopy;
                Outbuf += sizeToCopy;
            }
        }

    } while (false);
    return status;
}

/*! *********************************************************************************
 * \brief  Writes remainder of 16 byte buffer to flash when terminating FW update
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_FlushWriteBuffer(void)
{
    ota_flash_status_t status;
    status_t           st;

    status = kStatus_OTA_Flash_Success;

    do
    {
#if !defined(gOtaEraseBeforeImageBlockReq_c) || (gOtaEraseBeforeImageBlockReq_c == 0)
        /* Erase the sector if needed */
        status = ExternalFlash_EraseBlock(ctx.mWriteBufferOffs, PLATFORM_EXTFLASH_SECTOR_SIZE);
        if (kStatus_OTA_Flash_Success != status)
        {
            break;
        }
#endif

        /* Write the current write buffer to flash */
        st = PLATFORM_WriteExternalFlash(ctx.mWriteBuffer, OTA_WRITE_BUFFER_SIZE, PHYS_ADDR(ctx.mWriteBufferOffs));
        if (st != kStatus_Success)
        {
            status = kStatus_OTA_Flash_Fail;
            break;
        }

#if 0
        /* debug - verify the data stored */
        static uint8_t verifyBuffer[PLATFORM_EXTFLASH_SECTOR_SIZE];

        status = ExternalFlash_ReadData(PLATFORM_EXTFLASH_SECTOR_SIZE, ctx.mWriteBufferOffs, verifyBuffer);
        if(memcmp(ctx.mWriteBuffer, verifyBuffer, PLATFORM_EXTFLASH_SECTOR_SIZE) != 0)
        {
            status = kStatus_OTA_Flash_Fail;
            break;
        }
#endif

        /* Reset the write buffer for the next sector */
        FLib_MemSet(ctx.mWriteBuffer, 0xFFU, OTA_WRITE_BUFFER_SIZE);
        ctx.mWriteBufferIndex = 0U;

    } while (false);

    return status;
}

/*! *********************************************************************************
 * \brief  Read data from an address pointing to External flash to a RAM buffer
 *
 * \param[in] NoOfBytes   Number of bytes to be read
 * \param[in] offs        offset address relative to start of External Storage
 * \param[out] Outbuf      pointer on buffer to be write to
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_ReadData(uint16_t NoOfBytes, uint32_t offs, uint8_t *inbuf)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Error;
    status_t           st;

    if (!OtaCheckRangeBelongsToPartition(offs, NoOfBytes))
    {
        status = kStatus_OTA_Flash_InvalidArgument;
    }
    else
    {
        st = PLATFORM_ReadExternalFlash(inbuf, NoOfBytes, PHYS_ADDR(offs), true);
        if (st == kStatus_Success)
        {
            status = kStatus_OTA_Flash_Success;
        }

        if (status == kStatus_OTA_Flash_Success)
        {
            for (uint32_t i = 0U; i < ctx.mWriteBufferIndex; i++)
            {
                if ((ctx.mWriteBufferOffs + i) >= offs && (ctx.mWriteBufferOffs + i) < (offs + NoOfBytes))
                {
                    inbuf[ctx.mWriteBufferOffs - offs + i] = ctx.mWriteBuffer[i];
                }
            }
        }
    }
    return status;
}

/*! *********************************************************************************
 * \brief  Return busy status i.e. whether engaged in Program or Erase operation
 *
 * \return    1 if busy, 0 otherwise
 ***********************************************************************************/
STATIC uint8_t ExternalFlash_isBusy(void)
{
    status_t st;
    bool     isBusy = true;

    st = PLATFORM_IsExternalFlashBusy(&isBusy);
    assert(st == kStatus_Success);
    NOT_USED(st);

    return (uint8_t)isBusy;
}

/*! *********************************************************************************
 * \brief  Erase area comprised between address up to required size
 *
 * \param[in] pOffs on entry *pOffs contain address where to start erasing and is
 *            updated to the limit where is was actually erased on exit
 * \param[in] pSize         *pSize contains the size to erase on entry, is update to
 *            the value actually erased on exit (rounded to the multiple of sectors)
 * \param[in] non_blocking not used in this implementation
 *
 * \return    kStatus_OTA_Flash_Success if successful, other values in case of error
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalFlash_EraseArea(uint32_t *pOffs, uint32_t *pSize, bool non_blocking)
{
    ota_flash_status_t status;
    uint32_t           remain_sz  = *pSize;
    uint32_t           erase_offs = *pOffs;
    /* The erase operation in internal flash is necessarily blocking */
    NOT_USED(non_blocking);
    do
    {
        if (!IS_SECTOR_ALIGNED(erase_offs))
        { /* Ensure we deal only with sector aligned addresses */
            RAISE_ERROR(status, kStatus_OTA_Flash_AlignmentError);
        }
        if (remain_sz != 0U)
        {
            status = ExternalFlash_EraseBlock(erase_offs, remain_sz);
            if (status != kStatus_OTA_Flash_Success)
            {
                break;
            }
            erase_offs += remain_sz; /* advance offset by remaining size */
            /* In the case of internal flash the entire erase operation must
             * complete till the end
             */
            remain_sz = 0U;
        }
        status = kStatus_OTA_Flash_Success;
    } while (false);

    *pOffs = NEXT_SECTOR_ADDR(erase_offs); /* Round to next sector boundary */
    *pSize = remain_sz;

    return status;
}

#if defined OtaDeprecatedFlashVerifyWrite_d && (OtaDeprecatedFlashVerifyWrite_d > 0)
/*! *********************************************************************************
 * \brief  Read back flash after programming operation
 *
 * \param[in] pData pointer of programmed content buffer
 *
 * \param[in] offs  offset from which to read back in external flash
 *
 * \param[in] length size to compare
 *
 * \return    kStatus_OTA_Flash_Success if identical, kStatus_OTA_Flash_Fail otherwise
 ***********************************************************************************/
STATIC ota_flash_status_t ExternalVerifyFlashProgram(uint8_t *pData, uint32_t offs, uint32_t length)
{
    ota_flash_status_t status = kStatus_OTA_Flash_Success;
    uint8_t            read_page_buf[PLATFORM_EXTFLASH_PAGE_SIZE];
    uint32_t           end_offs = offs + length;
    /* Perform reads page by page because it seems to be a reasonable size but
     * read buffer size could be different.
     */
    while (offs < end_offs)
    {
        uint32_t read_sz = (end_offs - offs);
        if (read_sz > sizeof(read_page_buf))
        {
            read_sz = sizeof(read_page_buf);
        }
        if (PLATFORM_ReadExternalFlash(read_page_buf, read_sz, PHYS_ADDR(offs), true) != kStatus_Success)
        {
            status = kStatus_OTA_Flash_Fail;
            break;
        }
        if (!FLib_MemCmp(pData, (void const *)read_page_buf, read_sz))
        {
            status = kStatus_OTA_Flash_Fail;
            break;
        }
        offs += read_sz;
        pData += read_sz;
    }

    return status;
}
#endif

otaResult_t OTA_SelectExternalStoragePartition(void)
{
    otaResult_t    status = gOtaExternalFlashError_c;
    OtaStateCtx_t *hdl    = &mOtaHdl;

    do
    {
        OTA_DEBUG_TRACE("Select External flash\r\n");

        if (hdl->FwUpdImageState == OtaImgState_Acquiring)
        {
            RAISE_ERROR(status, gOtaInvalidOperation_c);
        }
        ota_ext_partition = PLATFORM_OtaGetOtaExternalPartitionConfig();
        if (ota_ext_partition == NULL)
        {
            RAISE_ERROR(status, gOtaInvalidParam_c);
        }
        hdl->ota_partition  = ota_ext_partition;
        hdl->FlashOps       = &ext_flash_ops;
        hdl->ImageOffset    = PLATFORM_OtaGetImageOffset();
        hdl->MaxImageLength = hdl->ota_partition->size - hdl->ImageOffset;

        /* Define the start of the erase process */
        hdl->ErasedUntilOffset = hdl->ImageOffset;

        if (hdl->FlashOps->init() != kStatus_OTA_Flash_Success)
        {
            RAISE_ERROR(status, gOtaExternalFlashError_c);
        }
        status = gOtaSuccess_c;
    } while (false);

    return status;
}
