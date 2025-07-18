/*! *********************************************************************************
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2018, 2022-2025 NXP
 * All rights reserved.
 *
 * \file
 *
 * This is a source file which implements the FSCI commands received from the host.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
#include "EmbeddedTypes.h"
#include "FsciInterface.h"
#include "FsciCommands.h"
#include "FsciCommunication.h"
#include "FunctionLib.h"
#include "fsl_component_mem_manager.h"
#if !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0)
#include "fsl_adapter_flash.h"
#include "fsl_adapter_reset.h"
#endif /* !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0) */
#include "ModuleInfo.h"
#include "fwk_config.h"
#include "fwk_platform.h"

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
#include "fwk_timer_manager.h"
#endif

#if defined(gPlatformHasNbu_d) && (gPlatformHasNbu_d == 1)
#include "fwk_platform_ics.h"
#endif

#if gFSCI_IncludeMacCommands_c
#include "FsciMacCommands.h"
#endif

#if (defined(gFSCI_IncludeLpmCommands_c) && gFSCI_IncludeLpmCommands_c == 1)
#include "PWR_Interface.h"
#endif
#if (defined(gErpcLowPowerApiServiceIncluded_c) && (gErpcLowPowerApiServiceIncluded_c == 1))
#include "PWR_BlackBox_Interface.h"
#endif

#if (defined(gBeeStackIncluded_d) && gBeeStackIncluded_d == 1)
#include "ZigbeeTask.h"
#endif

#ifdef MULTICORE_APPLICATION_CORE
//#include "erpc_host.h"
#endif

#if defined gPlatResetMethod_c
#include "fwk_platform_reset.h"
#endif

#if gFsciIncluded_c
/************************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
************************************************************************************/

/* Weak function. */
#if defined(__GNUC__)
#define __WEAK_FUNC __attribute__((weak))
#elif defined(__ICCARM__)
#define __WEAK_FUNC __weak
#elif defined(__CC_ARM)
#define __WEAK_FUNC __weak
#endif

/* \brief FSCI Bootloader related settings */
#define gFsciBootTrigger_c (0xF5C18007U)

#if ((defined(CPU_QN908X)) && (CPU_QN908X > 0U))
#define gBootImageFlagsAddress_c (5 * 1024 + NUMBER_OF_INT_VECTORS * 4)
#elif (defined(CPU_MKW36Z512VFP4) || defined(CPU_MKW36Z512VHT4))
#define gBootImageFlagsAddress_c (mFlashEndAddress_c / 64 + NUMBER_OF_INT_VECTORS * 4)
#elif (defined(CPU_K32W042S1M2VPJ_cm4))
#define gBootImageFlagsAddress_c (16 * 1024)
#else
#ifndef GCOV_DO_COVERAGE
#define gBootImageFlagsAddress_c (mFlashEndAddress_c / 32 + NUMBER_OF_INT_VECTORS * 4)
#else
#define gBootImageFlagsAddress_c (mFlashEndAddress_c - (uint32_t)FSL_FEATURE_FLASH_PFLASH_BLOCK_WRITE_UNIT_SIZE)
#endif
#endif

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
/*
 * \brief   Converts the macro argument from milliseconds to microseconds
 */
#define TmrMillisecondsToMicroseconds(n) ((uint64_t)((n)*1000UL))
#endif

/************************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
************************************************************************************/
#if gFSCI_IncludeMacCommands_c && !gFsciHost_802_15_4_c
extern void Mac_GetExtendedAddress(uint8_t *pAddr, instanceId_t instanceId);
extern void Mac_SetExtendedAddress(uint8_t *pAddr, instanceId_t instanceId);
#endif
#if gFSCI_IncludeMacCommands_c
extern uint8_t PhyGetLastRxLqiValue(void);
#endif

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
mem_alloc_test_status_t FSCI_MemAllocTestCanAllocate(void *pCaller);
#endif

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/
typedef PACKED_STRUCT FsciWakeUpConfig_tag
{
    bool_t   signalWhenWakeUpFlag; /* Flag used to send or not a WakeUp.Ind message */
    uint32_t deepSleepDuration;    /* The deep sleep duration in 802.15.4 phy symbols (16 us) */
}
FsciWakeUpConfig_t;

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
typedef PACKED_STRUCT FsciMemAllocBufferTest_tag
{
    uint32_t LRMinValue;               /* Min value of the link register interval  */
    uint32_t LRMaxValue;               /* Max value of the link register interval  */
    bool_t   enableNumberOfBuffers;    /* Enable Number of buffers feature*/
    uint8_t  numberOfBuffersValue;     /* Number of buffers to not be allocated for
                                          the requested LR nterval */
    bool_t   enableTimeOfNoAllocation; /* Enable Time of failure test */
    uint32_t timeOfNoAllocationValue;  /* Time in milliseconds when buffers will not be
                                          allocated for request from LR the interval */
    uint64_t endtimeOfFailureValue;    /* Time in microseconds when to disable the time condition */
    bool_t   enable;                   /* TRUE - test condition enable,
                                          FALSE - test condition disabled */
    uint8_t counterNumberOfBuffers;    /* Counts the number of buffers executed for this slot */
}
FsciMemAllocBufferTest_t;
#endif
/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
************************************************************************************/
/* Set to TRUE when FSCI_Error() is called. */
uint8_t mFsciErrorReported;
bool_t (*pfFSCI_OtaSupportCalback)(clientPacket_t *pPacket) = NULL;

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
/* FSCI Error message */
static gFsciErrorMsg_t mFsciErrorMsg = {
    .header =
        {
            .startMarker = gFSCI_StartMarker_c,
            .opGroup     = gFSCI_CnfOpcodeGroup_c,
            .opCode      = mFsciMsgError_c,
            .len         = sizeof(clientPacketStatus_t),
        },
    .status    = (uint8_t)gFsciSuccess_c,
    .checksum  = 0u,
    .checksum2 = 0u,
};

/* FSCI Ack message */
#if gFsciTxAck_c
static gFsciAckMsg_t mFsciAckMsg = {
    {gFSCI_StartMarker_c, gFSCI_CnfOpcodeGroup_c, mFsciMsgAck_c, sizeof(uint8_t)}, 0, 0, 0};
#endif

/* FSCI OpCodes and corresponding handler functions */
static const gFsciOpCode_t FSCI_ReqOCtable[] = {
    {mFsciMsgModeSelectReq_c, FSCI_MsgModeSelectReqFunc},
    {mFsciMsgGetModeReq_c, FSCI_MsgGetModeReqFunc},
    {mFsciMsgResetCPUReq_c, FSCI_MsgResetCPUReqFunc},
    {mFsciMsgGetNumberOfFreeBuffersReq_c, FSCI_GetNumberOfFreeBuffersFunc},

    {mFsciMsgReadExtendedAdrReq_c, FSCI_MsgReadExtendedAdrReqFunc},
    {mFsciMsgWriteExtendedAdrReq_c, FSCI_MsgWriteExtendedAdrReqFunc},
    {mFsciLowLevelMemoryWriteBlock_c, FSCI_WriteMemoryBlock},
    {mFsciLowLevelMemoryReadBlock_c, FSCI_ReadMemoryBlock},
    {mFsciLowLevelPing_c, FSCI_Ping},

    {mFsciOtaSupportImageNotifyReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportStartImageReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportSetModeReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportQueryImageRsp_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportPushImageChunkReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportCommitImageReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportCancelImageReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportSetFileVerPoliciesReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportAbortOTAUpgradeReq_c, FSCI_OtaSupportHandlerFunc},
    {mFsciOtaSupportGetClientInfo_c, FSCI_OtaSupportHandlerFunc},

    /* Bootloader cmd */
    {mFsciEnableBootloaderReq_c, FSCI_EnableBootloaderFunc},

#if gFSCI_IncludeLpmCommands_c
    {mFsciMsgAllowDeviceToSleepReq_c, FSCI_MsgAllowDeviceToSleepReqFunc},
    {mFsciMsgGetWakeUpReasonReq_c, FSCI_MsgGetWakeUpReasonReqFunc},
#endif

    {mFsciGetUniqueId_c, FSCI_ReadUniqueId},
    {mFsciGetMcuId_c, FSCI_ReadMCUId},
    {mFsciGetSwVersions_c, FSCI_ReadModVer},
#if defined(gPlatformHasNbu_d) && (gPlatformHasNbu_d == 1)
    {mFsciGetNbuVersion_c, FSCI_ReadNbuVer},
#endif

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
    {mFSCIMemAllocTestReq_c, FSCI_MemAllocTest},
#endif
};

/* Used for maintaining backward compatibility */
static const opGroup_t mFsciModeSelectSAPs[] = {
    gFSCI_McpsSapId_c,  gFSCI_MlmeSapId_c, gFSCI_AspSapId_c,   gFSCI_NldeSapId_c, gFSCI_NlmeSapId_c,
    gFSCI_AspdeSapId_c, gFSCI_AfdeSapId_c, gFSCI_ApsmeSapId_c, gFSCI_ZdpSapId_c,
};

#if gFSCI_IncludeLpmCommands_c
uint8_t                   mFsciInterfaceToSendWakeUp;
static FsciWakeUpConfig_t mFsciWakeUpConfig = {
    FALSE,     /* WakeUp.Ind message is NOT sent when wake up */
    0x0003D090 /* deep sleep duration 4 seconds */
};
#endif

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
/* FsciNotAllocMemoryBuffer table */
FsciMemAllocBufferTest_t mFSCI_MemAllocBufferTest[gFSCI_NotAllocMemoryBufferSize_c] = {{0}};
uint8_t                  mFSCI_MemAllocBufferTestIndex                              = 0;
#endif

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

/*! *********************************************************************************
 * \brief   This is the handler function for the FSCI OpGroup.
 *          It calls the handler function for the received OpCode.
 *
 * \param[in] pPkt pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 ********************************************************************************** */
void fsciMsgHandler(clientPacket_t *pPkt, uint32_t fsciInterface)
{
    uint32_t i;

    /* Call the handler function for the received OpCode */
    for (i = 0; i < NumberOfElements(FSCI_ReqOCtable); i++)
    {
        if (FSCI_ReqOCtable[i].opCode == pPkt->structured.header.opCode)
        {
            if (FSCI_ReqOCtable[i].pfOpCodeHandle(pPkt, fsciInterface))
            {
                /* Reuse received message */
                pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
                FSCI_transmitFormatedPacket(pPkt, fsciInterface);
            }
            break;
        }
    }

    /* If handler function was not found, send error message */
    if (i >= NumberOfElements(FSCI_ReqOCtable))
    {
        (void)MEM_BufferFree(pPkt);
        FSCI_Error((uint8_t)gFsciUnknownOpcode_c, fsciInterface);
    }
}

/*! *********************************************************************************
 * \brief  Send an error message back to the external client.
 *         This function should not block even if there is no more dynamic memory available
 *
 * \param[in] errorCode the error encountered
 * \param[in] fsciInterface the interface on which the packet was received
 *
 *
 ********************************************************************************** */
void FSCI_Error(uint8_t errorCode, uint32_t fsciInterface)
{
    uint8_t virtInterface = FSCI_GetVirtualInterface(fsciInterface);
    uint8_t size          = sizeof(gFsciErrorMsg_t) - offsetof(gFsciErrorMsg_t, header.opGroup);

    /* Don't cascade error messages. */
    if (mFsciErrorReported == 0u)
    {
        uint16_t checksum_comp_sz = (offsetof(gFsciErrorMsg_t, checksum) - offsetof(gFsciErrorMsg_t, header.opGroup));
        mFsciErrorMsg.status      = errorCode;
        /* sizeof(gFsciErrorMsg_t) is 7 or 8 depending on length field size : checksum_comp_sz is 4 or 5
         * and checksum is computed from offset 1  */
        // coverity[overrun-buffer-arg:FALSE] Out-of-bounds access (OVERRUN) is false positive
        mFsciErrorMsg.checksum = FSCI_computeChecksum(&mFsciErrorMsg.header.opGroup, checksum_comp_sz);

        if (virtInterface != 0u)
        {
#if (gFsciMaxVirtualInterfaces_c > 0)
            mFsciErrorMsg.checksum2 = mFsciErrorMsg.checksum;
            mFsciErrorMsg.checksum += virtInterface;
            mFsciErrorMsg.checksum2 ^= mFsciErrorMsg.checksum;
            size++;
#else
            (void)virtInterface;
#endif
        }
#if !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0)
        (void)SerialManager_WriteBlocking((serial_write_handle_t)gFsciSerialWriteHandle[fsciInterface],
                                          (uint8_t *)&mFsciErrorMsg, size);
#else
        union
        {
            void         *pVoid;
            pfFSCI_Send_t pfFSCI_Send;
        } fsciHandle;
        fsciHandle.pVoid = gFsciSerialInterfaces[fsciInterface];
        fsciHandle.pfFSCI_Send((uint8_t *)&mFsciErrorMsg, size, FALSE);
#endif
        mFsciErrorReported = 1u;
    }
}

#if gFsciTxAck_c
/*! *********************************************************************************
 * \brief  Send an ack message back to the external client.
 *
 * \param[in] checksum of the packet received
 * \param[in] fsciInterface the interface on which the packet was received
 *
 *
 ********************************************************************************** */
void FSCI_Ack(uint8_t checksum, uint32_t fsciInterface)
{
    uint8_t virtInterface = FSCI_GetVirtualInterface(fsciInterface);
    uint8_t size          = sizeof(mFsciAckMsg) - 1u;

    mFsciAckMsg.checksumPacketReceived = checksum;
    mFsciAckMsg.checksum               = FSCI_computeChecksum(&mFsciAckMsg.header.opGroup, (uint16_t)size - 2u);

    if (virtInterface != 0u)
    {
#if (gFsciMaxVirtualInterfaces_c > 0)
        mFsciAckMsg.checksum2 = mFsciAckMsg.checksum;
        mFsciAckMsg.checksum += virtInterface;
        mFsciAckMsg.checksum2 ^= mFsciAckMsg.checksum;
        size++;
#else
        (void)virtInterface;
#endif
    }

    (void)Serial_SyncWrite(gFsciSerialInterfaces[fsciInterface], (uint8_t *)&mFsciAckMsg, size);
}
#endif

#if gFsciHostSupport_c
/*! *********************************************************************************
 * \brief  Send a CPU-Reset Request to a FSCI black box
 *
 * \param[in] fsciInterface the interface to send the packet on
 *
 ********************************************************************************** */
gFsciStatus_t FSCI_ResetReq(uint32_t fsciInterface)
{
    clientPacket_t *pFsciPacket = MEM_BufferAlloc(sizeof(clientPacketHdr_t) + 2u);
    gFsciStatus_t   status      = gFsciSuccess_c;
    uint8_t         checksum    = 0u;
    uint8_t         size        = 0u;

    if (NULL == pFsciPacket)
    {
        status = gFsciOutOfMessages_c;
    }
    else
    {
        pFsciPacket->structured.header.startMarker = gFSCI_StartMarker_c;
        pFsciPacket->structured.header.opGroup     = gFSCI_ReqOpcodeGroup_c;
        pFsciPacket->structured.header.opCode      = mFsciMsgResetCPUReq_c;
        pFsciPacket->structured.header.len         = 0u;
        size                                       = sizeof(clientPacketHdr_t) + 1u;
        checksum                                   = FSCI_computeChecksum(pFsciPacket->raw + 1u, size - 2u);
        pFsciPacket->structured.payload[0]         = checksum;
        (void)Serial_SyncWrite(gFsciSerialInterfaces[fsciInterface], pFsciPacket->raw, size);
        (void)MEM_BufferFree(pFsciPacket);
    }

    return status;
}
#endif

/*! *********************************************************************************
 * \brief   Set FSCI operating mode for certain OpGroups
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_MsgModeSelectReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    uint8_t         i;
    uint8_t         payloadIndex = 0u;
    gFsciOpGroup_t *p;

    fsciLen_t dataLen = pData->structured.header.len;

    if (dataLen > 0u)
    {
        /* gFsciTxBlocking = pData->structured.payload[payloadIndex] */
        payloadIndex++;
        dataLen--;
    }

    for (i = 0; i < dataLen; i++)
    {
        p = FSCI_GetReqOpGroup(mFsciModeSelectSAPs[i], (uint8_t)fsciInterface);
        if (NULL != p)
        {
            p->mode = (gFsciMode_t)pData->structured.payload[payloadIndex + i];
        }
    }

    pData->structured.payload[0] = (uint8_t)gFsciSuccess_c;
    pData->structured.header.len = sizeof(uint8_t);
    return TRUE;
}

/*! *********************************************************************************
 * \brief   Returns FSCI operating mode for certain OpGroups
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_MsgGetModeReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    uint8_t         i;
    uint8_t         payloadIndex = 0u;
    gFsciOpGroup_t *p;

    pData->structured.payload[payloadIndex++] = (uint8_t)gFsciSuccess_c;
    pData->structured.payload[payloadIndex++] = (uint8_t)gFsciTxBlocking;

    for (i = 0u; i < NumberOfElements(mFsciModeSelectSAPs); i++)
    {
        p = FSCI_GetReqOpGroup(mFsciModeSelectSAPs[i], (uint8_t)fsciInterface);
        if (NULL != p)
        {
            pData->structured.payload[payloadIndex++] = (uint8_t)(p->mode);
        }
        else
        {
            pData->structured.payload[payloadIndex++] = (uint8_t)gFsciInvalidMode;
        }
    }

    pData->structured.header.len = payloadIndex;
    return TRUE;
}

/*! *********************************************************************************
 * \brief   Function used for writing to RAM memory.
 *          Payload contains the packet received over the serial interface
 *          bytes 0-3 --> start address for writing
 *          byte  4   --> number of bytes to be written
 *          bytes 5+  --> data to be written starting with start address.
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_WriteMemoryBlock(clientPacket_t *pData, uint32_t fsciInterface)
{
#if !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0)
    uint16_t len;
    uint8_t *addr;

    FLib_MemCpy(&addr, pData->structured.payload, sizeof(uint8_t *));
    len = pData->structured.payload[sizeof(uint8_t *)];

    /* Check RAM boundaries */
    if ((gPlatformRamStartAddress_c <= (uint32_t)addr) && (gPlatformRamEndAddress_c >= (uint32_t)addr))
    {
        FLib_MemCpy(addr, &pData->structured.payload[sizeof(uint8_t *) + 1u], len);
    }
    else
    {
        FSCI_Error((uint8_t)gFsciError_c, fsciInterface);
        len = 0u;
    }

    pData->structured.header.len = (uint16_t)sizeof(len);
    pData->structured.payload[0] = (uint8_t)len;
#else  /* !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0) */
    FSCI_Error((uint8_t)gFsciError_c, fsciInterface);
#endif /* !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0) */
    return TRUE;
}

/*! *********************************************************************************
 * \brief   Function used for reading from RAM memory.
 *          Payload contains the packet received over the serial interface
 *          bytes 0-3 --> start address for reading
 *          byte  4   --> number of bytes to read
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_ReadMemoryBlock(clientPacket_t *pData, uint32_t fsciInterface)
{
    bool_t status = TRUE; /* Try to reuse the received buffer */
#if !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0)
    clientPacket_t *pPkt;
    uint16_t        len;
    uint32_t       *addr;

    FLib_MemCpy(&addr, pData->structured.payload, sizeof(uint8_t *));
    len = pData->structured.payload[sizeof(uint8_t *)];

    if (MEM_BufferGetSize(pData) >= (sizeof(clientPacketHdr_t) + len + gFsci_TailBytes_c))
    {
        pPkt = pData;
    }
    else
    {
        pPkt = MEM_BufferAlloc((uint32_t)sizeof(clientPacketHdr_t) + len + gFsci_TailBytes_c);
    }

    if (pPkt == NULL)
    {
        FSCI_Error((uint8_t)gFsciOutOfMessages_c, fsciInterface);
        (void)MEM_BufferFree(pData);
        status = FALSE;
    }
    /* Check RAM and FLASH boundaries */
    else if (((gPlatformRamStartAddress_c <= (uint32_t)addr) && ((uint32_t)addr <= gPlatformRamEndAddress_c)) ||
             ((gPlatformFlashStartAddress_c < (uint32_t)addr) && ((uint32_t)addr <= gPlatformFlashEndAddress_c)))
    {
        FLib_MemCpy(pPkt->structured.payload, addr, len);

        pPkt->structured.header.len = len;

        /* Check if the received buffer was reused. */
        if (pPkt != pData)
        {
            /* A new buffer was allocated. Fill with aditional information */
            pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
            pPkt->structured.header.opCode  = mFsciLowLevelMemoryReadBlock_c;
            FSCI_transmitFormatedPacket(pPkt, fsciInterface);
            (void)MEM_BufferFree(pData);
            status = FALSE;
        }
    }
    else
    {
        FSCI_Error((uint8_t)gFsciError_c, fsciInterface);
        (void)MEM_BufferFree(pData);
        status = FALSE;
    }
#else  /* !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0) */
    FSCI_Error((uint8_t)gFsciError_c, fsciInterface);
    (void)MEM_BufferFree(pData);
    status                       = FALSE;
#endif /* !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0) */
    return status;
}

/*! *********************************************************************************
 * \brief  This function simply echoes back the payload
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 * \remarks Remarks: if USB communication is used, the connection will be lost
 *
 ********************************************************************************** */
bool_t FSCI_Ping(clientPacket_t *pData, uint32_t fsciInterface)
{
    /* Nothing to do here */
    return TRUE;
}

/*! *********************************************************************************
 * \brief  This function resets the MCU
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 * \remarks Remarks: if USB communication is used, the connection will be lost
 *
 ********************************************************************************** */
bool_t FSCI_MsgResetCPUReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    bool_t status = FALSE;
#if gFSCI_IncludeMacCommands_c && gFsciHost_802_15_4_c
    /* Get Host FSCI interface for MAC instance and forward packet */
    clientPacket_t *pFsciPacket = MEM_BufferAlloc(sizeof(clientPacketHdr_t) + 9);
    uint8_t         checksum;
    uint8_t         size;

    if (NULL == pFsciPacket)
    {
        FSCI_Error(gFsciOutOfMessages_c, fsciInterface);
        status = FALSE;
    }
    else
    {
        FLib_MemCpy(pFsciPacket, pData, sizeof(clientPacketHdr_t));
        pFsciPacket->structured.header.startMarker = gFSCI_StartMarker_c;
        size = sizeof(clientPacketHdr_t) + pFsciPacket->structured.header.len + 1 /* CRC */;

        /* Compute Checksum */
        checksum = FSCI_computeChecksum(pFsciPacket->raw + 1, size - 2);
        pFsciPacket->structured.payload[pFsciPacket->structured.header.len] = checksum;

        (void)Serial_SyncWrite(gFsciSerialInterfaces[fsciHostGetMacInterfaceId(fsciGetMacInstanceId(fsciInterface))],
                               pFsciPacket->raw, size + 9); /* additional bytes to allow last byte reception */

        (void)MEM_BufferFree(pFsciPacket);
    }
#endif

#if gFSCI_ResetCpu_c
#if defined gPlatResetMethod_c
    /* Force use of alternate reset method */
    PLATFORM_ResetCpu();
#else
    /* not defined same as if defined as gUseResetByNvicReset */
    OSA_DisableIRQGlobal();
    PLATFORM_Delay(500U);
    HAL_ResetMCU();
#endif
#endif

    (void)MEM_BufferFree(pData);
    return status;
}

/*! *********************************************************************************
 * \brief Read the number of free MemManager buffers
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_GetNumberOfFreeBuffersFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    (void)MEM_BufferFree(pData);
    /*TO DO*/
    /*gFreeMessagesCount isn't defined in SDK component or MemManagerLight.c*/
    /*gFreeMessagesCount in line 656 is always 0*/
    return FALSE;
}

/*! *********************************************************************************
 * \brief  This function writes the MAC Extended Address into the MAC layer
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 * \remarks Remarks: this function is legacy
 *
 ********************************************************************************** */
bool_t FSCI_MsgWriteExtendedAdrReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
#if gFSCI_IncludeMacCommands_c

#if gFsciHost_802_15_4_c
    /* Get Host FSCI interface for MAC instance and forward packet */
    clientPacket_t *pFsciPacket = MEM_BufferAlloc(sizeof(clientPacketHdr_t) + sizeof(uint64_t) + gFsci_TailBytes_c);
    if (NULL == pFsciPacket)
    {
        FSCI_Error(gFsciOutOfMessages_c, fsciInterface);
        pData->structured.payload[0] = gFsciOutOfMessages_c;
        pData->structured.header.len = sizeof(clientPacketStatus_t);
        return FALSE;
    }
    else
    {
        FLib_MemCpy(pFsciPacket, pData, sizeof(clientPacketHdr_t) + sizeof(uint64_t));
        FSCI_transmitFormatedPacket(pFsciPacket, fsciHostGetMacInterfaceId(fsciGetMacInstanceId(fsciInterface)));
    }
#else
    Mac_SetExtendedAddress(pData->structured.payload, fsciGetMacInstanceId(fsciInterface));
#endif

#if gRF4CEIncluded_d
    extern uint8_t aExtendedAddress[8];
    FLib_MemCpy(aExtendedAddress, pData->structured.payload, 8);
#endif

    pData->structured.payload[0] = (uint8_t)gFsciSuccess_c;
#else
    pData->structured.payload[0] = (uint8_t)gFsciRequestIsDisabled_c;
#endif /* gFSCI_IncludeMacCommands_c */
    pData->structured.header.len = sizeof(clientPacketStatus_t);
    return TRUE;
}

/*! *********************************************************************************
 * \brief  This function sends the MAC Extended Address over the serial interface
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE if response fits in the buffer, FALSE otherwise, handled internally
 *
 * \remarks Remarks: this function is legacy
 *
 ********************************************************************************** */
bool_t FSCI_MsgReadExtendedAdrReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    clientPacket_t *pPkt;
    bool_t          status;
    /* Packet hdr, status and CRC */
    uint16_t size = sizeof(clientPacketHdr_t) + sizeof(clientPacketStatus_t) + gFsci_TailBytes_c;

#if gFSCI_IncludeMacCommands_c && !gFsciHost_802_15_4_c
    /* Ext addr is 8 bytes */
    size += sizeof(uint64_t);
#endif

    if (MEM_BufferGetSize(pData) >= size)
    {
        pPkt = pData;
    }
    else
    {
        /* Need to allocate another buffer, as the current one is not large enough */
        pPkt = MEM_BufferAlloc(size);
    }

    if (NULL == pPkt)
    {
        MEM_BufferFree(pData);
        FSCI_Error(gFsciOutOfMessages_c, fsciInterface);
        status = FALSE;
    }
    else
    {
#if gFSCI_IncludeMacCommands_c && !gFsciHost_802_15_4_c
        Mac_GetExtendedAddress(&pPkt->structured.payload[sizeof(clientPacketStatus_t)],
                               fsciGetMacInstanceId(fsciInterface));

        pPkt->structured.payload[0] = gFsciSuccess_c;
        pPkt->structured.header.len = sizeof(clientPacketStatus_t) + sizeof(uint64_t);
#else
        pPkt->structured.payload[0] = (uint8_t)gFsciRequestIsDisabled_c;
        pPkt->structured.header.len = sizeof(clientPacketStatus_t);
#endif

        if (pPkt != pData)
        {
            MEM_BufferFree(pData);
            pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
            pPkt->structured.header.opCode  = mFsciMsgReadExtendedAdrReq_c;
            FSCI_transmitFormatedPacket(pPkt, fsciInterface);
            status = FALSE;
        }
        else
        {
            status = TRUE;
        }
    }

    return status;
}

/*! *********************************************************************************
 * \brief  This function sends the LQI value for the last received packet over
 *         the serial interface
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
#if gFSCI_IncludeMacCommands_c
bool_t FSCI_GetLastLqiValue(clientPacket_t *pData, uint32_t fsciInterface)
{
    pData->structured.payload[0] = PhyGetLastRxLqiValue();
    pData->structured.header.len = sizeof(clientPacketStatus_t);
    return TRUE;
}
#endif

#if gFSCI_IncludeLpmCommands_c

__WEAK_FUNC bool_t PWR_ChangeDeepSleepMode(uint8_t dsMode)
{
    (void)dsMode;
    return TRUE;
}

/*! *********************************************************************************
 * \brief Allow the MCU to enter LowPower
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_MsgAllowDeviceToSleepReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    uint8_t dsMode = PWR_GetDeepSleepMode();

    mFsciInterfaceToSendWakeUp = (uint8_t)fsciInterface;
    /* Set the new configuration */
    FLib_MemCpy(&mFsciWakeUpConfig, pData->structured.payload, sizeof(mFsciWakeUpConfig));
    if (pData->structured.header.len > sizeof(mFsciWakeUpConfig))
    {
        dsMode = pData->structured.payload[sizeof(mFsciWakeUpConfig)];
    }

    if (mFsciWakeUpConfig.deepSleepDuration < 10)
    {
        pData->structured.payload[0] = gFsciError_c;
    }
    else
    {
        pData->structured.payload[0] = gFsciSuccess_c;
    }

    pData->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
    pData->structured.header.len     = sizeof(clientPacketStatus_t);

    // Perform a Sync Tx
    gFsciTxBlocking = TRUE;
    FSCI_transmitFormatedPacket(pData, fsciInterface);
    gFsciTxBlocking = FALSE;

    if (mFsciWakeUpConfig.deepSleepDuration >= 10)
    {
#ifdef MULTICORE_APPLICATION_CORE
#if gErpcLowPowerApiServiceIncluded_c
        PWR_ChangeBlackBoxDeepSleepMode(dsMode);
        PWR_SetBlackBoxDeepSleepTimeInMs(mFsciWakeUpConfig.deepSleepDuration * 16 / 1000);
        PWR_AllowBlackBoxToSleep(); /* Allow device to sleep */
#endif
        PWR_ChangeDeepSleepMode(dsMode);
        PWR_SetDeepSleepTimeInSymbols(mFsciWakeUpConfig.deepSleepDuration);
        PWR_AllowDeviceToSleep(); /* Allow device to sleep */
#else
        PWR_ChangeDeepSleepMode(dsMode);
        PWR_SetDeepSleepTimeInSymbols(mFsciWakeUpConfig.deepSleepDuration);
        PWR_AllowDeviceToSleep(); /* Allow device to sleep */
#endif
    }

    return FALSE;
}
/*! *********************************************************************************
 * \brief Read the MCU wake-up reason
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_MsgGetWakeUpReasonReqFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    (void)MEM_BufferFree(pData);
#if 0
    /* PWRLib_MCU_WakeupReason function not supported */
    FSCI_transmitPayload( gFSCI_CnfOpcodeGroup_c, mFsciMsgGetWakeUpReasonReq_c, (void*)&PWRLib_MCU_WakeupReason, (uint16_t)sizeof(PWRLib_WakeupReason_t), fsciInterface);
#endif
    return FALSE;
}

/*! *********************************************************************************
 * \brief Notify that the MCU has exit LowPower mode
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
void FSCI_SendWakeUpIndication(void)
{
    clientPacket_t *pPkt;

    PWR_DisallowDeviceToSleep(); /* Disallow device to sleep */
#ifdef MULTICORE_APPLICATION_CORE
#if gErpcLowPowerApiServiceIncluded_c
    PWR_DisallowBlackBoxToSleep(); /* Allow device to sleep */
#endif
#endif

    if (mFsciWakeUpConfig.signalWhenWakeUpFlag)
    {
        pPkt = MEM_BufferAlloc(sizeof(clientPacketHdr_t) + sizeof(clientPacketStatus_t) + gFsci_TailBytes_c);

        if (pPkt)
        {
            pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
            pPkt->structured.header.opCode  = mFsciMsgWakeUpIndication_c;
            pPkt->structured.header.len     = sizeof(clientPacketStatus_t);
            pPkt->structured.payload[0]     = gFsciSuccess_c;
            FSCI_transmitFormatedPacket(pPkt, mFsciInterfaceToSendWakeUp);
        }
    }
}
#endif /* #if gFSCI_IncludeLpmCommands_c */

/*! *********************************************************************************
 * \brief  This function sends the content of the SIM_UID registers over the
 *         serial interface
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_ReadUniqueId(clientPacket_t *pData, uint32_t fsciInterface)
{
    clientPacket_t *pPkt;
    uint8_t        *p;
    bool_t          status;
    uint32_t        size = sizeof(clientPacketHdr_t) + 4u * sizeof(uint32_t) + gFsci_TailBytes_c;

    /* Check if the received buffer is large enough to be reused */
    if (MEM_BufferGetSize(pData) >= size)
    {
        pPkt = pData;
    }
    else
    {
        pPkt = MEM_BufferAlloc(size);
    }

    if (pPkt == NULL)
    {
        FSCI_Error((uint8_t)gFsciOutOfMessages_c, fsciInterface);
        (void)MEM_BufferFree(pData);
        status = FALSE;
    }
    else
    {
        p                           = pPkt->structured.payload;
        pPkt->structured.header.len = 4u * sizeof(uint32_t);
#if defined(SIM_UIDH_UID_MASK) && !defined(SIM_UIDM_UID_MASK)
        FLib_MemCpy(p, (void *)&SIM->UIDH, sizeof(uint32_t));
#else
        FLib_MemSet(p, 0, sizeof(uint32_t));
#endif
#if (defined(FSL_FEATURE_SOC_SIM_COUNT) && (FSL_FEATURE_SOC_SIM_COUNT > 0U))
        {
            p += sizeof(uint32_t);
#if defined(SIM_UIDMH_UID_MASK)
            FLib_MemCpy(p, (void const *)((uint32_t *)((uint32_t)&SIM->UIDMH)), sizeof(uint32_t));
#elif defined(SIM_UIDH_UID_MASK)
            FLib_MemCpy(p, (void *)&SIM->UIDH, sizeof(uint32_t));
#endif
            p += sizeof(uint32_t);
#if defined(SIM_UIDML_UID_MASK)
            FLib_MemCpy(p, (void const *)((uint32_t *)((uint32_t)&SIM->UIDML)), sizeof(uint32_t));
#elif defined(SIM_UIDM_UID_MASK)
            FLib_MemCpy(p, (void *)&SIM->UIDM, sizeof(uint32_t));
#endif
            p += sizeof(uint32_t);
            FLib_MemCpy(p, (void const *)((uint32_t *)((uint32_t)&SIM->UIDL)), sizeof(uint32_t));
        }
#else  /*(defined(FSL_FEATURE_SOC_SIM_COUNT) && (FSL_FEATURE_SOC_SIM_COUNT > 0U))*/
        p += sizeof(uint32_t);
        // #TODO - use BLE_address instead of 0xFF
        FLib_MemSet(p, 0xff, sizeof(uint32_t) * 3u);
#endif /*(defined(FSL_FEATURE_SOC_SIM_COUNT) && (FSL_FEATURE_SOC_SIM_COUNT > 0U))*/

        /* Check if the received buffer was reused. */
        if (pPkt != pData)
        {
            /* A new buffer was allocated. Fill with aditional information */
            pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
            pPkt->structured.header.opCode  = mFsciGetUniqueId_c;
            FSCI_transmitFormatedPacket(pPkt, fsciInterface);
            (void)MEM_BufferFree(pData);
            status = FALSE;
        }
        else
        {
            status = TRUE;
        }
    }

    return status;
}

/*! *********************************************************************************
 * \brief  This function sends the content of the SIM_SDID register over the
 *         serial interface
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_ReadMCUId(clientPacket_t *pData, uint32_t fsciInterface)
{
    pData->structured.header.len = sizeof(uint32_t);
#if (defined(FSL_FEATURE_SOC_SIM_COUNT) && (FSL_FEATURE_SOC_SIM_COUNT > 0U))
    FLib_MemCpy(pData->structured.payload, (void const *)((uint32_t *)((uint32_t)&SIM->SDID)), sizeof(uint32_t));
#else
    FLib_MemSet(pData->structured.payload, 0xff, sizeof(uint32_t));
#endif
    return TRUE;
}

/*! *********************************************************************************
 * \brief  This function reads all module information located in section VERSION_TAGS
 *         and sends this information over the serial interface
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_ReadModVer(clientPacket_t *pData, uint32_t fsciInterface)
{
    bool_t          status = TRUE;
    clientPacket_t *pPkt;
    moduleInfo_t   *pInfo                    = gVERSION_TAGS_startAddr_d;
    uint8_t         noOfEntriesSecondaryCore = 0u;
#if defined(CPU_K32W042S1M2VPJ_cm4) && (CPU_K32W042S1M2VPJ_cm4 == 1)
    uint8_t *pString  = MEM_BufferAlloc(MAX_REGISTERED_MODULES_STRLEN);
    uint8_t  totalLen = 0u;
    if (NULL != pString)
    {
        noOfEntriesSecondaryCore = ModVer_GetNoOfEntries_Multicore();
    }
#endif
    uint16_t size = (uint16_t)(sizeof(clientPacketHdr_t) + gFsci_TailBytes_c +
                               gVERSION_TAGS_entries_d * gVERSION_TAGS_entrySizeNoPadding_d +
                               noOfEntriesSecondaryCore * gVERSION_TAGS_entrySizeNoPadding_d);

    /* Check if the received buffer is large enough to be reused */
    if (MEM_BufferGetSize(pData) >= size)
    {
        pPkt = pData;
    }
    else
    {
        pPkt = MEM_BufferAlloc(size);
    }

    if (pPkt == NULL)
    {
        FSCI_Error((uint8_t)gFsciOutOfMessages_c, fsciInterface);
        (void)MEM_BufferFree(pData);
        status = FALSE;
    }
    else
    {
        pPkt->structured.payload[0] = (uint8_t)(gVERSION_TAGS_entries_d + noOfEntriesSecondaryCore);
        size                        = sizeof(uint8_t);

        while (pInfo < gVERSION_TAGS_endAddr_d)
        {
            FLib_MemCpy(&pPkt->structured.payload[size], &pInfo->moduleId,
                        gVERSION_TAGS_entrySizeNoPaddingNoModuleString_d);
            size += gVERSION_TAGS_entrySizeNoPaddingNoModuleString_d;
            pInfo++;
        }

#if defined(CPU_K32W042S1M2VPJ_cm4) && (CPU_K32W042S1M2VPJ_cm4 == 1)
        if (NULL != pString)
        {
            totalLen = ModVer_GetInfoFSCIFormat_Multicore(pString);
            FLib_MemCpy(&pPkt->structured.payload[size], pString, totalLen);
            size += totalLen;
            (void)MEM_BufferFree(pString);
        }
#endif
        pPkt->structured.header.len = (uint8_t)size;

        /* Check if the received buffer was reused. */
        if (pPkt != pData)
        {
            /* A new buffer was allocated. Fill with aditional information */
            pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
            pPkt->structured.header.opCode  = mFsciGetSwVersions_c;
            FSCI_transmitFormatedPacket(pPkt, fsciInterface);
            (void)MEM_BufferFree(pData);
            status = FALSE;
        }
    }

    return status;
}

#if defined(gPlatformHasNbu_d) && (gPlatformHasNbu_d == 1)
/*! *********************************************************************************
* \brief  This function reads NBU version information located retrieving it from
          NBU processor and passes it on over the serial interface
*
* \param[in] pData pointer to location of the received data
* \param[in] fsciInterface the interface on which the packet was received
*
* \return  TRUE in order to recycle the received message
*
********************************************************************************** */
bool_t FSCI_ReadNbuVer(clientPacket_t *pData, uint32_t fsciInterface)
{
    bool_t          status = FALSE;
    clientPacket_t *pPkt;
    NbuInfo_t       nbu_info;

    do
    {
        uint8_t tag_lg;

        /* Pointer on NbuInfo structure received from NBU :
         * potentially to be released when used */
#if !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0)
        if (PLATFORM_GetNbuInfo(&nbu_info) < 0)
        {
            FSCI_Error((uint8_t)gFsciError_c, fsciInterface);
            break;
        }
#endif /* !defined(gFsciOverRpmsg_c) || (gFsciOverRpmsg_c == 0) */
        uint16_t size = (uint16_t)(sizeof(clientPacketHdr_t) + gFsci_TailBytes_c + sizeof(NbuInfo_t));

        pPkt = MEM_BufferAlloc(size);

        if (pPkt == NULL)
        {
            FSCI_Error((uint8_t)gFsciOutOfMessages_c, fsciInterface);
            break;
        }

        nbu_info.repo_tag[MAX_TAG_SZ - 1] = '\0';
        size                              = 0;

        FLib_MemCpy(&pPkt->structured.payload[0], &nbu_info, 3);
        size += 3;

        pPkt->structured.payload[size] = MAX_SHA_SZ;
        size++;

        FLib_MemCpy(&pPkt->structured.payload[size], &nbu_info.repo_digest, MAX_SHA_SZ);
        size += MAX_SHA_SZ;

        tag_lg                         = (uint8_t)strlen(&nbu_info.repo_tag[0]);
        pPkt->structured.payload[size] = tag_lg;
        size++;
        if (tag_lg > 0)
        {
            FLib_MemCpy(&pPkt->structured.payload[size], &nbu_info.repo_tag, tag_lg);
        }
        size += tag_lg;

        FLib_MemCpy(&pPkt->structured.payload[size], &nbu_info.versionBuildNo, 1);
        size += 1;

        pPkt->structured.header.len = (uint8_t)size;

        /* A new buffer was allocated. Fill with additional information */
        pPkt->structured.header.opGroup = gFSCI_CnfOpcodeGroup_c;
        pPkt->structured.header.opCode  = mFsciGetNbuVersion_c;
        FSCI_transmitFormatedPacket(pPkt, fsciInterface);
    } while (false);

    (void)MEM_BufferFree(pData);
    status = FALSE;

    return status;
}
#endif /* #if defined(gPlatformHasNbu_d) && (gPlatformHasNbu_d == 1) */

#if defined(gFSCI_MemAllocTest_Enabled_d) && (gFSCI_MemAllocTest_Enabled_d)
/*! *********************************************************************************
 * \brief   Add possibility that for a number of times or a period of time a memory
 *          block will not be allocated for a specific location identified with a link
 *          register interval.
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_MemAllocTest(clientPacket_t *pData, uint32_t fsciInterface)
{
    uint64_t systemTime;
    uint8_t  coppyIndex = 0u;

    /* Copy LRMinValue and LRMaxValue and enableNumberOfBuffers */
    FLib_MemCpy(&mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex], pData->structured.payload, 9);
    coppyIndex = coppyIndex + 9u;

    if (mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableNumberOfBuffers == TRUE)
    {
        /* Coppy numberOfBuffersValue and enableTimeOfNoAllocation if enableNumberOfBuffers enabled */
        FLib_MemCpy(&mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].numberOfBuffersValue,
                    &pData->structured.payload[coppyIndex], 2u);
        coppyIndex = coppyIndex + 2u;
    }
    else
    {
        /* Copy enableTimeOfNoAllocation if enableNumberOfBuffers is not enabled */
        FLib_MemCpy(&mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableTimeOfNoAllocation,
                    &pData->structured.payload[coppyIndex], 1);
        coppyIndex = coppyIndex + 1;
    }

    if (mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableTimeOfNoAllocation == TRUE)
    {
        /* Copy timeOfNoAllocationValue if enableTimeOfNoAllocation enabled */
        FLib_MemCpy(&mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].timeOfNoAllocationValue,
                    &pData->structured.payload[coppyIndex], 4u);
        coppyIndex = coppyIndex + 4u;
    }

    if (mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableTimeOfNoAllocation == TRUE)
    {
        /* Calculate end of time for Time Of No Allocation
          (timeOfNoAllocationValue in milliseconds convert to microseconds) */
        systemTime = TMR_GetTimestamp();
        mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].endtimeOfFailureValue =
            systemTime + TmrMillisecondsToMicroseconds(
                             mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].timeOfNoAllocationValue);
    }

    if (mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableNumberOfBuffers == TRUE)
    {
        mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].counterNumberOfBuffers = 0;
    }

    if ((mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableNumberOfBuffers != TRUE) &&
        (mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enableTimeOfNoAllocation != TRUE))
    {
        /* Prepare gFsciError_c packet, no test conditions enabled */
        pData->structured.payload[0] = gFsciError_c;
        pData->structured.header.len = sizeof(uint8_t);
    }
    else
    {
        /* Enable this slot */
        mFSCI_MemAllocBufferTest[mFSCI_MemAllocBufferTestIndex].enable = TRUE;

        mFSCI_MemAllocBufferTestIndex++;
        if (mFSCI_MemAllocBufferTestIndex == gFSCI_NotAllocMemoryBufferSize_c)
        {
            mFSCI_MemAllocBufferTestIndex = 0u;
        }

        /* Prepare confirm packet  */
        pData->structured.payload[0] = gFsciSuccess_c;
        pData->structured.header.len = sizeof(uint8_t);
    }

    return TRUE;
}

/*! *********************************************************************************
 * \brief   Verify if any available FSCI memory allocation test conditions are
 *          enabled and if the received LR is in the conditioning interval.
 *          If the criteria for a condition is met return TRUE, so the buffer should
 *          not be allocated else, return FALSE.
 *
 * \param[in] pCaller   Pointer to location of the LR that requires the buffer allocation
 *
 * \return  kStatus_AllocBlock        Criteria for a condition is met do not allocate buffer.
 * \return  kStatus_AllocSuccess      Criteria for a condition is not met allocate buffer.
 *
 ********************************************************************************** */
mem_alloc_test_status_t FSCI_MemAllocTestCanAllocate(void *pCaller)
{
    uint8_t  index;
    uint64_t systemTime;
    for (index = 0u; index < gFSCI_NotAllocMemoryBufferSize_c; index++)
    {
        if (mFSCI_MemAllocBufferTest[index].enable == TRUE)
        {
            if (((uint32_t)pCaller <= mFSCI_MemAllocBufferTest[index].LRMaxValue) &&
                ((uint32_t)pCaller >= mFSCI_MemAllocBufferTest[index].LRMinValue))
            {
                if (mFSCI_MemAllocBufferTest[index].enableTimeOfNoAllocation == TRUE)
                {
                    systemTime = TMR_GetTimestamp();
                    if (systemTime > mFSCI_MemAllocBufferTest[index].endtimeOfFailureValue)
                    {
                        /* Time of failure for this slot passed, disable this slot */
                        mFSCI_MemAllocBufferTest[index].enable = FALSE;
                        continue;
                    }

                    if (mFSCI_MemAllocBufferTest[index].enableNumberOfBuffers == FALSE)
                    {
                        /* Return kStatus_AllocBlock so buffer will no be allocated */
                        return kStatus_AllocBlock;
                    }
                }

                if (mFSCI_MemAllocBufferTest[index].enableNumberOfBuffers == TRUE)
                {
                    if (mFSCI_MemAllocBufferTest[index].numberOfBuffersValue >
                        mFSCI_MemAllocBufferTest[index].counterNumberOfBuffers)
                    {
                        /* Return TRUE so buffer will no be allocated */
                        mFSCI_MemAllocBufferTest[index].counterNumberOfBuffers++;
                        return kStatus_AllocBlock;
                    }
                    else if (mFSCI_MemAllocBufferTest[index].numberOfBuffersValue ==
                             mFSCI_MemAllocBufferTest[index].counterNumberOfBuffers)
                    {
                        mFSCI_MemAllocBufferTest[index].enable = FALSE;
                        continue;
                    }
                    else
                    {
                        /* MISRA */
                    }
                }
            }
        }
    }
    return kStatus_AllocSuccess;
}
#endif /* gFSCI_MemAllocTest_Enabled_d */

/*! *********************************************************************************
 * \brief  This function handles the requests for the OTA OpCodes
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE in order to recycle the received message
 *
 ********************************************************************************** */
bool_t FSCI_OtaSupportHandlerFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    bool_t status = FALSE;
#if gFSCI_IncludeMacCommands_c && gFsciHost_802_15_4_c
    (void)Serial_SyncWrite(gFsciSerialInterfaces[fsciHostGetMacInterfaceId(fsciGetMacInstanceId(fsciInterface))], pData,
                           sizeof(clientPacketHdr_t) + pData->structured.header.len + 1);
#else
    if (pfFSCI_OtaSupportCalback != NULL)
    {
        status = pfFSCI_OtaSupportCalback(pData);
    }
#endif
    if (status == FALSE)
    {
        (void)MEM_BufferFree(pData);
    }

    return status;
}
/*! *********************************************************************************
 * \brief  This function handles the requests for enable the MSD Bootloader
 *
 * \param[in] pData pointer to location of the received data
 * \param[in] fsciInterface the interface on which the packet was received
 *
 * \return  TRUE if the received message was recycled, FALSE if it must be deleted
 *
 ********************************************************************************** */
bool_t FSCI_EnableBootloaderFunc(clientPacket_t *pData, uint32_t fsciInterface)
{
    /* No longer support on these platform - Shall use now  PLATFORM_OtaNotifyNewImageReady() */
    return FALSE;
}
#endif /* gFsciIncluded_c */
