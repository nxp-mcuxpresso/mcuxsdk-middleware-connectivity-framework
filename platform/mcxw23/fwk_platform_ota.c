/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "fsl_common.h"
#include "fwk_platform_ota.h"
#include "fsl_rom_api.h"
#include "fsl_debug_console.h"
#include "OtaSupport.h"

#if ((defined(__CC_ARM) || defined(__UVISION_VERSION) || defined(__ARMCC_VERSION)))
extern uint32_t Image$$OTA_region$$ZI$$Base[];
extern uint32_t Image$$OTA_region$$ZI$$Limit[];
extern uint32_t Image$$OTA_region$$Length;

#define OTA_AREA_START_ADDRESS (Image$$OTA_region$$ZI$$Base)
#define OTA_AREA_SIZE          (Image$$OTA_region$$ZI$$Limit)
#else
extern uint32_t INT_STORAGE_START;
extern uint32_t INT_STORAGE_SIZE;
#endif /* ((defined(__CC_ARM) || defined(__UVISION_VERSION) || defined(__ARMCC_VERSION))) */

static flash_config_t s_flashInstance;

const OtaPartition_t *PLATFORM_OtaGetOtaInternalPartitionConfig(void)
{
    static const OtaPartition_t int_ota_partition_cfg = {
        .start_offset   = (uint32_t)&INT_STORAGE_START,
        .size           = (uint32_t)&INT_STORAGE_SIZE,
        .sector_size    = FSL_FEATURE_FLASH_SECTOR_SIZE_BYTES,
        .page_size      = FSL_FEATURE_FLASH_PAGE_SIZE_BYTES,
        .internal_flash = true,
        .spi_baudrate   = 0,
    };
    return &int_ota_partition_cfg;
}

int PLATFORM_OtaBootDataUpdateOnCommit(const OtaLoaderInfo_t *ota_loader_info)
{
    return 0;
}

#define RAM_FUNC __attribute__((section(".ramfunc"))) __attribute__((used)) __attribute__((noinline))

/* Create custom implementations for FLASH_Erase and FLASH_Program to make sure they are put in RAM */
RAM_FUNC status_t Erase(flash_config_t *config, uint32_t start, uint32_t lengthInBytes, uint32_t key)
{
    return BOOTLOADER_API_TREE_POINTER->flashDriver->flash_erase(config, start, lengthInBytes, key);
}

RAM_FUNC status_t Program(flash_config_t *config, uint32_t start, uint8_t *src, uint32_t lengthInBytes)
{
    return BOOTLOADER_API_TREE_POINTER->flashDriver->flash_program(config, start, src, lengthInBytes);
}

RAM_FUNC void OtaPanic(void)
{
    while (1)
    {
        ;
    }
}

#define COPY_BUFFER_SIZE (512U)
static uint8_t s_copyBuffer[COPY_BUFFER_SIZE];
RAM_FUNC void  CopyAndReboot(const OtaLoaderInfo_t *ota_loader_info)
{
    /* IMPORTANT: Everything in this function has to be executed from RAM because flash will be erased.
       Some functions have not been used on purpose to minimize the functions that need to be placed in RAM (e.g. no
       prints, no asserts, no functions from standard lib ...) */

    status_t status;
    uint32_t copySize = ota_loader_info->image_sz;
    uint8_t *source   = (uint8_t *)ota_loader_info->image_addr;
    uint32_t offset   = 0;

    DisableGlobalIRQ();

    /* Erase all necessary sectors for the new image */
    status = Erase(&s_flashInstance, 0,
                   (((ota_loader_info->image_sz - 1) / FSL_FEATURE_FLASH_SECTOR_SIZE_BYTES) + 1) *
                       FSL_FEATURE_FLASH_SECTOR_SIZE_BYTES,
                   (uint32_t)kFLASH_ApiEraseKey);

    if (kStatus_Success != status)
    {
        OtaPanic();
    }
    /* Copy the image from the temporary download location to the target location overwriting the otap_client demo app
     */
    while (copySize)
    {
        uint32_t chunkSize = MIN(copySize, COPY_BUFFER_SIZE);
        uint32_t writeSize =
            (((chunkSize - 1) / FSL_FEATURE_FLASH_PHRASE_SIZE_BYTES) + 1) * FSL_FEATURE_FLASH_PHRASE_SIZE_BYTES;
        for (uint32_t i = 0; i < chunkSize; i++)
        {
            s_copyBuffer[i] = source[i];
        }
        status = Program(&s_flashInstance, offset, s_copyBuffer, writeSize);
        if (kStatus_Success != status)
        {
            OtaPanic();
        }
        copySize -= chunkSize;
        offset += chunkSize;
        source += chunkSize;
    }

    /* Reset the device in order to launch the new application */
    PMC->RESETCTRL |= PMC_RESETCTRL_SWRRESETENABLE_MASK;
    SYSCON->SWR_RESET = 0x5A000001;

    while (1)
    {
        ;
    }
}
int PLATFORM_OtaNotifyNewImageReady(const OtaLoaderInfo_t *ota_loader_info)
{
    status_t status;
    if (OTA_GetImgState() == OtaImgState_Acquiring)
    {
        PRINTF("New firmware image received, sending confirmation to server\n");
    }
    else if (OTA_GetImgState() == OtaImgState_CandidateRdy)
    {
        PRINTF("Launching new firmware (by overwriting otap_client fw)\n");
        status = FLASH_Init(&s_flashInstance);
        assert_equal(status, kStatus_Success);
        (void)status;
        CopyAndReboot(ota_loader_info);
    }
    else
    {
        assert(0);
    }

    return 0;
}

uint32_t PLATFORM_OtaGetImageOffset(void)
{
    return 0U;
}

int PLATFORM_OtaClearBootFlags(void)
{
    return 0;
}

int PLATFORM_OtaGetImageState(uint8_t *p_state)
{
    return 0;
}

int PLATFORM_OtaUpdateImageState(uint8_t img_state)
{
    return 0;
}

int PLATFORM_OtaCheckImageValidity(const uint8_t *data, uint32_t size)
{
    return 0;
}

const OtaPartition_t *PLATFORM_OtaGetOtaExternalPartitionConfig(void)
{
    return NULL;
}

int PLATFORM_InitExternalFlash(void)
{
    return 0;
}

bool PLATFORM_IsExternalFlashSectorBlank(uint32_t address)
{
    return 0;
}

int PLATFORM_EraseExternalFlash(uint32_t address, uint32_t size)
{
    return 0;
}

int PLATFORM_ReadExternalFlash(uint8_t *dest, uint32_t length, uint32_t address, bool requestFastRead)
{
    return 0;
}

int PLATFORM_WriteExternalFlash(uint8_t *data, uint32_t length, uint32_t address)
{
    return 0;
}

int PLATFORM_IsExternalFlashBusy(bool *isBusy)
{
    return 0;
}
