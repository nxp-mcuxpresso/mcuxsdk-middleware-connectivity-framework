/*! *********************************************************************************
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * \file
 *
 * This is the header file for the Mobile Wireless Standard interface.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 ********************************************************************************** */

#ifndef _MWS_INTERCORE_H_
#define _MWS_INTERCORE_H_

/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */
#include "EmbeddedTypes.h"

/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */

#ifndef gMWS_Intercore_Enabled_d
#define gMWS_Intercore_Enabled_d 0
#endif

/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */
typedef struct
{
    mwsProtocols_t procotol;
    uint8_t        priority;
} mws_remote_protocol_handle_t;

typedef enum
{
    gMWS_Intercore_Init_c,
    gMWS_Intercore_Idle_c,
    gMWS_Intercore_Active_c,
    gMWS_Intercore_Denied_c,
    gMWS_Intercore_Release_c,
    gMWS_Intercore_Abort_c,
    gMWS_Intercore_GetInactivityDuration_c
} mws_intercore_events_t;

typedef enum
{
    MWS_RequestPacket_c,
    MWS_NotifyPacket_c
} mws_rpmsg_packet_type_t;

typedef enum
{
    MWS_RequestRegister_c,
    MWS_RequestAcquire_c,
    MWS_RequestAbort_c
} mws_rpmsg_request_type_t;

typedef PACKED_STRUCT
{
    mws_rpmsg_request_type_t mwsRequestType;
    mwsProtocols_t           mwsProtocol;
}
mws_rpmsg_request_t;

typedef PACKED_STRUCT
{
    mws_intercore_events_t mwsNotifyEvent;
}
mws_rpmsg_notify_t;

typedef PACKED_STRUCT
{
    mws_rpmsg_packet_type_t mwsPacketType;
    PACKED_UNION
    {
        mws_rpmsg_request_t mwsRequest;
        mws_rpmsg_notify_t  mwsNotify;
    };
}
mws_rpmsg_packet_t;

typedef enum
{
    MWS_RequestOngoing_c,
    MWS_RequestEnded_c,
} mws_intercore_request_state_t;

typedef struct
{
    mws_intercore_request_state_t mwsState;
    mws_intercore_events_t        mwsRequestEvent;
} mws_intercore_request_t;

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */
void        MWS_Intercore_Init(void);
void        MWS_Intercore_Register(mwsProtocols_t protocol);
mwsStatus_t MWS_Intercore_Acquire(mwsProtocols_t protocol);
mwsStatus_t MWS_Intercore_Abort(void);
mwsStatus_t MWS_Intercore_SignalIdle(mwsProtocols_t protocol);

#endif /* _MWS_INTERCORE_H_ */