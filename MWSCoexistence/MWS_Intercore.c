/*! *********************************************************************************
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * \file
 *
 * This is the source file for the Mobile Wireless Standard Intercore
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

/************************************************************************************
*************************************************************************************
* Includes
*************************************************************************************
************************************************************************************/
#include "MWS.h"
#include "MWS_Intercore.h"
#include "fsl_common.h"
#include "fwk_platform_mws.h"
#include "fsl_adapter_rpmsg.h"
#include "FunctionLib.h"

/*! *********************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
********************************************************************************** */
typedef struct
{
    hal_rpmsg_handle_t               mwsRpmsgHandle;
    mws_rpmsg_packet_t               mwsRpmsgPacket;
    mws_remote_protocol_handle_t     mwsRemoteProtocolsHandles[4];
    volatile mws_intercore_request_t mwsIntercoreRequestState;
    uint8_t                          mwsProtocolPriority[gMWS_None_c];
} mws_intercore_state_t;

/************************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
************************************************************************************/
static hal_rpmsg_return_status_t MWS_Intercore_RpmsgRxCallBack(void *param, uint8_t *data, uint32_t len);
static void                      MWS_Intercore_SendMessage(mws_rpmsg_packet_t *pMsg);
/* Handler for received requests */
static void MWS_Intercore_HandleReceivedRequest(void);
static void MWS_Intercore_HandleReceivedNotify(void);
static void MWS_Intercore_HandleReceivedRegisterRequest(void);
static void MWS_Intercore_HandleReceivedAcquireRequest(void);
static void MWS_Intercore_HandleReceivedAbortRequest(void);
/* Handler for received notifications */
static void MWS_Intercore_HandleReceivedIdleNotify(void);

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/

static mws_intercore_state_t mwsIntercoreState = {
    .mwsProtocolPriority = {gMWS_BLEPriority_d, gMWS_802_15_4Priority_d, gMWS_ANTPriority_d, gMWS_GENFSKPriority_d},
};

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/
void MWS_Intercore_Init(void)
{
    static uint8_t initialized = 0U;

    if (initialized == 0U)
    {
        /* Init RPMSG channel */
        PLATFORM_InitMwsRpmsg();
        mwsIntercoreState.mwsRpmsgHandle = PLATFORM_GetMwsRpmsgHandle();
        (void)HAL_RpmsgInstallRxCallback(mwsIntercoreState.mwsRpmsgHandle, MWS_Intercore_RpmsgRxCallBack, NULL);
    }
}

void MWS_Intercore_Register(mwsProtocols_t protocol)
{
    /* Send a register request to remote MWS module */
    mws_rpmsg_packet_t mwsPacket;

    mwsPacket.mwsPacketType             = MWS_RequestPacket_c;
    mwsPacket.mwsRequest.mwsRequestType = MWS_RequestRegister_c;
    mwsPacket.mwsRequest.mwsProtocol    = protocol;

    /* send the message and wait for the reply */
    mwsIntercoreState.mwsIntercoreRequestState.mwsState = MWS_RequestOngoing_c;
    MWS_Intercore_SendMessage(&mwsPacket);
    while (mwsIntercoreState.mwsIntercoreRequestState.mwsState == MWS_RequestOngoing_c)
    {
    }

    assert(mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent == gMWS_Intercore_Init_c);
}

mwsStatus_t MWS_Intercore_Acquire(mwsProtocols_t protocol)
{
    /* send acquire request to 2nd core and wait for a reply */
    mwsStatus_t        status = gMWS_Success_c;
    mws_rpmsg_packet_t mwsPacket;

    /* build the acquire request */
    mwsPacket.mwsPacketType             = MWS_RequestPacket_c;
    mwsPacket.mwsRequest.mwsRequestType = MWS_RequestAcquire_c;
    mwsPacket.mwsRequest.mwsProtocol    = protocol;

    /* send the message and wait for the reply */
    mwsIntercoreState.mwsIntercoreRequestState.mwsState = MWS_RequestOngoing_c;
    MWS_Intercore_SendMessage(&mwsPacket);
    while (mwsIntercoreState.mwsIntercoreRequestState.mwsState == MWS_RequestOngoing_c)
    {
    }

    /* Check if the access is denied or not */
    if (mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent == gMWS_Intercore_Denied_c)
    {
        status = gMWS_Denied_c;
    }

    return status;
}

mwsStatus_t MWS_Intercore_Abort(void)
{
    /* send abort request to 2nd core and wait for a reply */
    mwsStatus_t        status = gMWS_Success_c;
    mws_rpmsg_packet_t mwsPacket;

    /* build the abort request */
    mwsPacket.mwsPacketType             = MWS_RequestPacket_c;
    mwsPacket.mwsRequest.mwsRequestType = MWS_RequestAbort_c;
    // mwsPacket.mwsRequest.mwsProtocol = protocol;

    /* send the message and wait for the reply */
    mwsIntercoreState.mwsIntercoreRequestState.mwsState = MWS_RequestOngoing_c;
    MWS_Intercore_SendMessage(&mwsPacket);
    while (mwsIntercoreState.mwsIntercoreRequestState.mwsState == MWS_RequestOngoing_c)
    {
    }

    assert(mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent == gMWS_Intercore_Abort_c);
    return status;
}

mwsStatus_t MWS_Intercore_SignalIdle(mwsProtocols_t protocol)
{
    /* send idle notification to remote protocols */
    mwsStatus_t        status = gMWS_Success_c;
    mws_rpmsg_packet_t mwsPacket;

    mwsPacket.mwsPacketType            = MWS_NotifyPacket_c;
    mwsPacket.mwsNotify.mwsNotifyEvent = gMWS_Intercore_Idle_c;
    MWS_Intercore_SendMessage(&mwsPacket);
    return status;
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/
static hal_rpmsg_return_status_t MWS_Intercore_RpmsgRxCallBack(void *param, uint8_t *data, uint32_t len)
{
    hal_rpmsg_return_status_t status = kStatus_HAL_RL_RELEASE;

    FLib_MemCpy(&(mwsIntercoreState.mwsRpmsgPacket), data,
                sizeof(mws_rpmsg_packet_t)); // check if we can work without copy

    switch (mwsIntercoreState.mwsRpmsgPacket.mwsPacketType)
    {
        case MWS_RequestPacket_c:
            /* handle request */
            MWS_Intercore_HandleReceivedRequest();
            break;
        case MWS_NotifyPacket_c:
            /* handle notify */
            MWS_Intercore_HandleReceivedNotify();
            break;
        default:
            /* drop packet */
            break;
    }

    return status;
}

static void MWS_Intercore_SendMessage(mws_rpmsg_packet_t *pMsg)
{
    uint8_t *          pRpmsgBuffer = NULL;
    hal_rpmsg_status_t rpmsgStatus;

    pRpmsgBuffer = HAL_RpmsgAllocTxBuffer(mwsIntercoreState.mwsRpmsgHandle, sizeof(mws_rpmsg_packet_t));
    assert(pRpmsgBuffer != NULL);

    FLib_MemCpy(pRpmsgBuffer, (uint8_t *)pMsg, sizeof(mws_rpmsg_packet_t));

    rpmsgStatus = HAL_RpmsgNoCopySend(mwsIntercoreState.mwsRpmsgHandle, pRpmsgBuffer, sizeof(mws_rpmsg_packet_t));
    assert(rpmsgStatus == kStatus_HAL_RpmsgSuccess);
    (void)rpmsgStatus;
}

static void MWS_Intercore_HandleReceivedRequest(void)
{
    switch (mwsIntercoreState.mwsRpmsgPacket.mwsRequest.mwsRequestType)
    {
        case MWS_RequestRegister_c:
            /* Register remote protocol */
            MWS_Intercore_HandleReceivedRegisterRequest();
            break;
        case MWS_RequestAcquire_c:
            /* Remote protocol wants to use the XCVR, handle the request */
            MWS_Intercore_HandleReceivedAcquireRequest();
            break;
        case MWS_RequestAbort_c:
            /* handle abort */
            MWS_Intercore_HandleReceivedAbortRequest();
            break;
        default:
            /* should not happened */
            assert(0);
            break;
    }
}

static void MWS_Intercore_HandleReceivedNotify(void)
{
    switch (mwsIntercoreState.mwsRpmsgPacket.mwsNotify.mwsNotifyEvent)
    {
        case gMWS_Intercore_Init_c:
            mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent = gMWS_Intercore_Init_c;
            mwsIntercoreState.mwsIntercoreRequestState.mwsState        = MWS_RequestEnded_c;
            break;
        case gMWS_Intercore_Idle_c:
            /* Other core is not using the XCVR, we can notify local protocols */
            MWS_Intercore_HandleReceivedIdleNotify();
            break;
        case gMWS_Intercore_Active_c:
            /* Access granted to the local protocol */
            mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent = gMWS_Intercore_Active_c;
            mwsIntercoreState.mwsIntercoreRequestState.mwsState        = MWS_RequestEnded_c;
            break;
        case gMWS_Intercore_Denied_c:
            /* Access denied to the local protocol */
            mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent = gMWS_Intercore_Denied_c;
            mwsIntercoreState.mwsIntercoreRequestState.mwsState        = MWS_RequestEnded_c;
            break;
        case gMWS_Intercore_Release_c:
            /* Needed ? */
            break;
        case gMWS_Intercore_Abort_c:
            /* Other core has processed the abort request */
            mwsIntercoreState.mwsIntercoreRequestState.mwsRequestEvent = gMWS_Intercore_Abort_c;
            mwsIntercoreState.mwsIntercoreRequestState.mwsState        = MWS_RequestEnded_c;
            break;
        case gMWS_Intercore_GetInactivityDuration_c:
            /* TBD */
            break;
        default:; /* MISRA compliant */
            break;
    }
}

static void MWS_Intercore_HandleReceivedRegisterRequest(void)
{
    mws_rpmsg_packet_t mwsNotifyPacket;
    mwsProtocols_t     protocol = mwsIntercoreState.mwsRpmsgPacket.mwsRequest.mwsProtocol;

    /* register a remote protocol */
    if ((protocol < gMWS_None_c) && (gMWS_None_c != mwsIntercoreState.mwsRemoteProtocolsHandles[protocol].procotol))
    {
        mwsIntercoreState.mwsRemoteProtocolsHandles[protocol].procotol = protocol;
        mwsIntercoreState.mwsRemoteProtocolsHandles[protocol].priority =
            mwsIntercoreState.mwsProtocolPriority[protocol];
    }

    mwsNotifyPacket.mwsPacketType            = MWS_NotifyPacket_c;
    mwsNotifyPacket.mwsNotify.mwsNotifyEvent = gMWS_Intercore_Init_c;

    MWS_Intercore_SendMessage(&mwsNotifyPacket);
}

static void MWS_Intercore_HandleReceivedAcquireRequest(void)
{
    /* Acquire request received from remote protocol
     *  */
    mws_rpmsg_packet_t mwsNotifyPacket;

    mwsNotifyPacket.mwsPacketType = MWS_NotifyPacket_c;

#if 0
    mwsStatus_t        status = gMWS_Success_c;
    /* Disabling acquire on NBU for now as BLE LL doesnt support MWS yet */
    status = MWS_Acquire(mwsIntercoreState.mwsRpmsgPacket.mwsRequest.mwsProtocol, FALSE);

    if (status == gMWS_Success_c)
    {
        mwsNotifyPacket.mwsNotify.mwsNotifyEvent = gMWS_Intercore_Active_c;
    }
    else
    {
        mwsNotifyPacket.mwsNotify.mwsNotifyEvent = gMWS_Intercore_Denied_c;
    }
#else
    mwsNotifyPacket.mwsNotify.mwsNotifyEvent = gMWS_Intercore_Active_c;
#endif

    MWS_Intercore_SendMessage(&mwsNotifyPacket);
}

static void MWS_Intercore_HandleReceivedAbortRequest(void)
{
    (void)MWS_Abort();
    /* send notification to external protocol the abort is done */
    mws_rpmsg_packet_t mwsNotifyPacket;
    mwsNotifyPacket.mwsPacketType            = MWS_NotifyPacket_c;
    mwsNotifyPacket.mwsNotify.mwsNotifyEvent = gMWS_Intercore_Abort_c;

    MWS_Intercore_SendMessage(&mwsNotifyPacket);
}

static void MWS_Intercore_HandleReceivedIdleNotify(void)
{
    mwsStatus_t status;
    status = MWS_SignalIdle(gMWS_None_c);
    assert(status == gMWS_Success_c);
    (void)status;
}
