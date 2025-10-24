/*! *********************************************************************************
* Copyright 2017, 2024 NXP
* All rights reserved.
*
* \file
*
* This is the header file for the Mobile Wireless Standard interface.
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

#ifndef _MWS_H_
#define _MWS_H_ 

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
#if defined(gMWS_UseCoexistence_d) && (gMWS_UseCoexistence_d == 1) && \
    defined(gWCI2_UseCoexistence_d) && (gWCI2_UseCoexistence_d == 1)
#error "Cannot use both MWS and WCI2 coexistence at the same time"
#endif

#ifndef gMWS_Enabled_d
#define gMWS_Enabled_d        0
#endif

#ifndef gMWS_UsePrioPreemption_d
#define gMWS_UsePrioPreemption_d  1
#endif

#ifndef gMWS_UseCoexistence_d
#define gMWS_UseCoexistence_d  0
#endif

/* Default priority values of the protocols */
#ifndef gMWS_BLEPriority_d
#define gMWS_BLEPriority_d    0
#endif

#ifndef gMWS_802_15_4Priority_d
#define gMWS_802_15_4Priority_d    1
#endif

#ifndef gMWS_GENFSKPriority_d
#define gMWS_GENFSKPriority_d    2
#endif

#ifndef gMWS_ANTPriority_d
#define gMWS_ANTPriority_d    3
#endif

/* Check for same priority values */
#if (gMWS_BLEPriority_d == gMWS_802_15_4Priority_d)    || (gMWS_BLEPriority_d == gMWS_GENFSKPriority_d)   || \
    (gMWS_BLEPriority_d == gMWS_ANTPriority_d)         || (gMWS_802_15_4Priority_d == gMWS_GENFSKPriority_d) || \
    (gMWS_802_15_4Priority_d == gMWS_ANTPriority_d)    || (gMWS_GENFSKPriority_d == gMWS_ANTPriority_d)
#error MWS protocol priority values should not be the same!
#endif

#if !defined(gMWS_UseOverridableTimings) || (gMWS_UseOverridableTimings == 0)
/* Coexistence signals timings */
#ifndef gMWS_CoexPrioSignalTime_d
#define gMWS_CoexPrioSignalTime_d   (40) /* us */
#endif
        
#ifndef gMWS_CoexRfActiveAssertTime_d 
#define gMWS_CoexRfActiveAssertTime_d (100) /* us */
#endif

#ifndef gMWS_CoexConfirmWaitTime_d
#define gMWS_CoexConfirmWaitTime_d (90) /* us. Note: Should be lower than gMWS_CoexRfActiveAssertTime_d! */
#endif

/* Coexistence RF Deny pin polarity: 1 - for active on logic one; 0 - active on logic zero */
#ifndef gMWS_CoexRfDenyActiveState_d
#define gMWS_CoexRfDenyActiveState_d (1)
#endif

#if defined(gMWS_CoexGrantDelay) && (gMWS_CoexGrantDelay != 0)
/* delay in sampling the GRANT pin */
#define gMWS_CoexGrantPinSampleDelay    (gMWS_CoexGrantDelay) /* us */
#endif /* defined(gMWS_CoexGrantDelay) && (gMWS_CoexGrantDelay != 0) */

#else
extern volatile uint32_t gMWS_CoexPrioSignalTime_d;

#if (gMWS_Coex_Model_d == gMWS_Coex_Prio_Only_d)
    extern volatile uint32_t gMWS_CoexRfActiveAssertTime_d;

    #if defined(gMWS_CoexGrantDelay) && (gMWS_CoexGrantDelay != 0)
        extern volatile uint32_t gMWS_CoexGrantPinSampleDelay;
    #endif /* defined(gMWS_CoexGrantDelay) && (gMWS_CoexGrantDelay != 0) */

#endif /* gMWS_Coex_Model_d == gMWS_Coex_Prio_Only_d) */

extern volatile uint32_t gMWS_CoexConfirmWaitTime_d;

extern volatile uint32_t gMWS_CoexRfDenyActiveState_d;

#endif /* !defined(gMWS_UseOverridableTimings) || (gMWS_UseOverridableTimings == 0) */

#ifndef gMWS_CoexRxPrio_d
#define gMWS_CoexRxPrio_d (gMWS_HighPriority)
#endif

#ifndef gMWS_CoexTxPrio_d
#define gMWS_CoexTxPrio_d (gMWS_HighPriority)
#endif

/* Priority of the GPIO interrupt inside the PINT IRQ Handler */
#ifndef gMWS_GpioIsrPrio_d
#define gMWS_GpioIsrPrio_d gGpioIsrPrioNormal_c
#endif

/* The NVIC priority of the corresponding PINT interrupt 
 * NOTE: The value is divided by 2 in the call to GpioInstallIsr()
 *       in the body of MWS_CoexistenceInit() */
#ifndef gMWS_GpioNvicPrio_d
#define gMWS_GpioNvicPrio_d gGpioDefaultNvicPrio_c
#endif

/* Supported coexistence models */
#define gMWS_Coex_Status_Prio_d    0
#define gMWS_Coex_Prio_Only_d      1

/* Coexistence model to be handled */
#ifndef gMWS_Coex_Model_d
#define gMWS_Coex_Model_d    gMWS_Coex_Status_Prio_d
#endif

/* Coexistence FSCI configuration */
#ifndef gMWS_FsciEnabled_d
#define gMWS_FsciEnabled_d           (0)
#endif

/* Default FSCI interface used */
#ifndef gMWS_FsciInterface_d
#define gMWS_FsciInterface_d         (0)
#endif

/* Operation Groups */
#define gMWS_FsciReqOG_d             (0xC0)
#define gMWS_FsciCnfOG_d             (0xC1)

/* Commands */
#define mFsciMsgCoexEnableReq_c      (0x01) /* Fsci-CoexEnable.Request.  */
#define mFsciMsgCoexDisableReq_c     (0x02) /* Fsci-CoexDisable.Request. */
      
#if (gMWS_Coex_Model_d == gMWS_Coex_Prio_Only_d)
#define gMWS_RF_ACTIVE_PORT_d      gpioPort_C_c
#define gMWS_RF_ACTIVE_PIN_d       1
#define gMWS_RF_PRIORITY_PORT_d    gpioPort_C_c
#define gMWS_RF_PRIORITY_PIN_d     4
#endif      

#ifndef gWCI2_UseCoexistence_d
#define gWCI2_UseCoexistence_d 0
#endif

/* This cannot be changed, it's hard-wired. */
#define gWCI2_CoexRfDenyActiveState_d   (0) /* Active HI is GRANT, DENY is LO. */

#if defined(gWCI2_UseCoexistence_d) && (gWCI2_UseCoexistence_d == 1)
#ifndef gWCI2_CoexGrantPinSampleDelay
#define gWCI2_CoexGrantPinSampleDelay   100  /* usec */
#endif
#endif

#ifndef gWCI2_CoexPrioSignalTime_d
#define gWCI2_CoexPrioSignalTime_d      40 /* usec */
#endif

/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */
typedef enum
{
    gMWS_BLE_c,
    gMWS_802_15_4_c,
    gMWS_ANT_c,
    gMWS_GENFSK_c,
    gMWS_None_c /*! must be the last item */
}mwsProtocols_t;

typedef enum
{
  gMWS_Success_c,
  gMWS_Denied_c,
  gMWS_InvalidParameter_c,
  gMWS_Error_c
}mwsStatus_t;

typedef enum
{
    gMWS_Init_c,
    gMWS_Idle_c,
    gMWS_Active_c,
    gMWS_Release_c,
    gMWS_Abort_c,
    gMWS_GetInactivityDuration_c
}mwsEvents_t;

typedef enum
{
    gMWS_IdleState_c,
    gMWS_RxState_c,
    gMWS_TxState_c
}mwsRfState_t;

typedef enum
{
    gMWS_LowPriority,
    gMWS_HighPriority
}mwsRfSeqPriority_t;


#if gWCI2_UseCoexistence_d || (gMWS_Coex_Model_d == gMWS_Coex_Prio_Only_d)
typedef enum
{
  gMWS_PinInterruptFallingEdge_c,
  gMWS_PinInterruptRisingEdge_c,
  gMWS_PinInterruptEitherEdge_c  
} mwsPinInterruptMode_t;
#endif // gMWS_Coex_Model_d == gMWS_Coex_Prio_Only_d

typedef uint32_t ( *pfMwsCallback ) ( mwsEvents_t event );

#if defined (gMWS_EnableCoexistenceStats_d) && (gMWS_EnableCoexistenceStats_d) || \
    defined(gWCI2_EnableCoexistenceStats_d) && (gWCI2_EnableCoexistenceStats_d)
typedef struct
{
    uint32_t numTxRequests;
    uint32_t numTxGrantWait;
    uint32_t numTxGrantWaitTimeout;

    uint32_t numRxRequests;
    uint32_t numRxGrantImmediate;
    uint32_t numRxGrantWait;
    uint32_t numRxGrantWaitTimeout;

    uint32_t numReleases;
    uint32_t numAborts;
} mwsCoexStats_t;
#endif
/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief  This function will register a protocol stack into MWS
*
* \param[in]  protocol - One of the supported MWS protocols
* \param[in]  cb       - The callback function used by the MWS to signal events to 
*                        the protocol stack
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t MWS_Register    (mwsProtocols_t protocol, pfMwsCallback cb);

/*! *********************************************************************************
* \brief  This function try to acquire access to the XCVR for the specified protocol
*
* \param[in]  protocol - One of the supported MWS protocols
* \param[in]  force    - If set, the active protocol will be preempted
*
* \return  If access to the XCVR is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t MWS_Acquire     (mwsProtocols_t protocol, uint8_t force);

/*! *********************************************************************************
* \brief  This function will release access to the XCVR, and will notify other 
*         protocols that the resource is idle.
*
* \param[in]  protocol - One of the supported MWS protocols
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t MWS_Release     (mwsProtocols_t protocol);

/*! *********************************************************************************
* \brief  This function will notify other protocols that the specified protocol is 
*         Idle and the XCVR is unused.
*
* \param[in]  protocol - One of the supported MWS protocols
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t MWS_SignalIdle  (mwsProtocols_t protocol);

/*! *********************************************************************************
* \brief  Force the active protocol to Idle state.
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t MWS_Abort       (void);

/*! *********************************************************************************
* \brief  This function will poll all other protocols for their inactivity period,
*         and will return the minimum time until the first protocol needs to be serviced.
*
* \param[in]  currentProtocol - One of the supported MWS protocols
*
* \return  the minimum inactivity duration (in microseconds)
*
********************************************************************************** */
uint32_t MWS_GetInactivityDuration (mwsProtocols_t currentProtocol);

/*! *********************************************************************************
* \brief  Returns the protocol that is currently using the XCVR
*
* \return  One of the supported MWS protocols
*
********************************************************************************** */
mwsProtocols_t MWS_GetActiveProtocol (void);


/*! *********************************************************************************
* \brief  Initialize the MWS External Coexistence driver
*
* \param[in]  rfDenyPin   - Pointer to the GPIO input pin used to signal RF access 
*                           granted/denied. Set to NULL if controlled by hardware.
* \param[in]  rfActivePin - Pointer to the GPIO output pin used to signal RF activity
*                           Set to NULL if controlled by hardware.
* \param[in]  rfStatusPin - Pointer to the GPIO output pin to signal the RF activyty type
*                           Set to NULL if controlled by hardware.
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t MWS_CoexistenceInit(void *rfDenyPin, void *rfActivePin, void *rfStatusPin);

/*! *********************************************************************************
* \brief  This function will register a protocol stack into MWS Coexistence module
*
* \param[in]  protocol - One of the supported MWS protocols
*
* \param[in]  cb       - The callback function used by the MWS Coexistence to signal 
*                        events to the protocol stack
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t MWS_CoexistenceRegister (mwsProtocols_t protocol, pfMwsCallback cb);

/*! *********************************************************************************
* \brief  This function returns the state of RF Deny pin.
*
* \return  uint8_t !gMWS_CoexRfDenyActiveState_d - RF Deny Inactive,
*                   gMWS_CoexRfDenyActiveState_d - RF Deny Active
*
********************************************************************************** */
uint8_t MWS_CoexistenceDenyState(void);

/*! *********************************************************************************
* \brief  This function will register a protocol stack into MWS Coexistence module
*
* \param[in]  rxPrio - The priority of the RX sequence
* \param[in]  txPrio - The priority of the TX sequence
*
********************************************************************************** */
void MWS_CoexistenceSetPriority(mwsRfSeqPriority_t rxPrio, mwsRfSeqPriority_t txPrio);

/*! *********************************************************************************
* \brief  Request for permission to access the medium for the specified RF sequence
*
* \param[in]  newState - The RF sequence type
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t MWS_CoexistenceRequestAccess(mwsRfState_t newState);

/*! *********************************************************************************
* \brief  This function will signal the change of the RF state, and request for permission
*
* \param[in]  newState - The new state in which the XCVR will transition
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t MWS_CoexistenceChangeAccess(mwsRfState_t newState);

/*! *********************************************************************************
* \brief  Signal externally that the Radio is not using the medium anymore.
*
********************************************************************************** */
void MWS_CoexistenceReleaseAccess(void);

/*! *********************************************************************************
* \brief  Enable Coexistence signals.
*
********************************************************************************** */
void MWS_CoexistenceEnable (void);

/*! *********************************************************************************
* \brief  Disable Coexistence signals.
*
********************************************************************************** */
void MWS_CoexistenceDisable (void);

/*! *********************************************************************************
* \brief  MWS FSCI message handler
*
* \param[in]  pData - pointer to data message 
* \param[in]  param - pointer to additional parameters (if any)
* \param[in]  fsciInterface - FSCI interface used
*
* \return  None
*
********************************************************************************** */
#if gMWS_FsciEnabled_d
void MWS_FsciMsgHandler( void* pData, void* param, uint32_t fsciInterface );
/*! *********************************************************************************
* \brief  Initialization function that registers MWS opcode group and the 
*         corresponding message handler
*
* \return  None
*
********************************************************************************** */

void MWS_FsciInit( void );
#endif

/*! *********************************************************************************
* \brief  MWS state query helper
*
* \return  0 if coexistence is enabled, != 0 otherwise
*
********************************************************************************** */
uint8_t MWS_CoexistenceIsEnabled(void);

#if gMWS_UseCoexistence_d
#if gMWS_EnableCoexistenceStats_d && (gMWS_EnableCoexistenceStats_d == 1)
/*! *********************************************************************************
* \brief  MWS stats query helper
*
* * \param[in/out]  stats - pointer where the acquired statistics will be written
*
* \return  0 if stats were succesfully retrieved, != 0 otherwise
*
********************************************************************************** */
uint8_t MWS_GetCoexStats(mwsCoexStats_t *stats);
#endif
#endif

/*! *********************************************************************************
* \brief  Initialize the WCI2 External Coexistence driver
*
* \param[in]  rfReq       - Pointer to the GPIO output pin used to signal RF activity.
*
* \param[in]  rfTxDeny    - Pointer to the GPIO input pin used to signal RF TX Grant.
*
* \param[in]  rfRxDeny    - Pointer to the GPIO output pin to signal the RF RX Grant.
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceInit(void *rfReq, void *rfTxDeny, void *rfRxDeny);

/*! *********************************************************************************
* \brief  Request for permission to access the medium for the specified RF sequence
*
* \param[in]  newState - The RF sequence type
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceRequestAccess(mwsRfState_t);

/*! *********************************************************************************
* \brief  This function will register a protocol stack into WCI2 Coexistence module
*
* \param[in]  protocol - One of the supported WCI2 protocols
*
* \param[in]  cb       - The callback function used by the WCI2 Coexistence to signal
*                        events to the protocol stack
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceRegister(mwsProtocols_t protocol, pfMwsCallback cb);

/*! *********************************************************************************
* \brief  This function will signal the change of the RF state, and request for permission
*
* \param[in]  newState - The new state in which the XCVR will transition
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceChangeAccess(mwsRfState_t newState);

/*! *********************************************************************************
* \brief  Request for permission to access the medium for the specified RF sequence
*
* \param[in]  newState - The RF sequence type
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceRequestAccess(mwsRfState_t newState);

/*! *********************************************************************************
* \brief  This function will register a protocol stack into WCI2 Coexistence module
*
* \param[in]  rxPrio - The priority of the RX sequence
* \param[in]  txPrio - The priority of the TX sequence
*
********************************************************************************** */
void WCI2_CoexistenceSetPriority(mwsRfSeqPriority_t rxPrio, mwsRfSeqPriority_t txPrio);

/*! *********************************************************************************
* \brief  Enable Coexistence signals.
*
********************************************************************************** */
void WCI2_CoexistenceEnable(void);

/*! *********************************************************************************
* \brief  Disable Coexistence signals.
*
********************************************************************************** */
void WCI2_CoexistenceDisable (void);

/*! *********************************************************************************
* \brief  Signal externally that the Radio is not using the medium anymore.
*
********************************************************************************** */
void WCI2_CoexistenceReleaseAccess(void);

/*! *********************************************************************************
* \brief  This function will notify other protocols that the specified protocol is
*         Idle and the XCVR is unused.
*
* \param[in]  protocol - One of the supported WCI2 protocols
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t WCI2_SignalIdle(mwsProtocols_t protocol);

/*! *********************************************************************************
* \brief  MWS state query helper
*
* \return  0 if coexistence is enabled, != 0 otherwise
*
********************************************************************************** */
uint8_t WCI2_CoexistenceIsEnabled(void);

/* Define CoexistenceIsEnabled function to have unified call */
#if gWCI2_UseCoexistence_d
#undef MWS_CoexistenceIsEnabled
#define MWS_CoexistenceIsEnabled WCI2_CoexistenceIsEnabled
#endif

#if gWCI2_UseCoexistence_d
#if defined(gWCI2_EnableCoexistenceStats_d) && (gWCI2_EnableCoexistenceStats_d)
/*! *********************************************************************************
* \brief  Coex stats query helper
*
* * \param[in/out]  stats - pointer where the acquired statistics will be written
*
* \return  0 if stats were succesfully retrieved, != 0 otherwise
*
********************************************************************************** */
uint8_t WCI2_GetCoexStats(mwsCoexStats_t *stats);
#endif /* defined(gWCI2_EnableCoexistenceStats_d) && (gWCI2_EnableCoexistenceStats_d) */
#endif /* gWCI2_UseCoexistence_d */

#endif /* _MWS_H_ */
