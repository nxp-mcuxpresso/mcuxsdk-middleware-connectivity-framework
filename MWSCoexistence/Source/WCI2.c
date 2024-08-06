/*! *********************************************************************************
* Copyright 2024 NXP
* All rights reserved.
*
* \file
*
* This is the source file for the Mobile Wireless Standard Interface.
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

/************************************************************************************
*************************************************************************************
* Includes
*************************************************************************************
************************************************************************************/
#include "MWS.h"
#include "fsl_os_abstraction.h"
#include "GPIO_Adapter.h"
#include "TimersManager.h"

#if (gWCI2_UseCoexistence_d) && (!gTimestamp_Enabled_d)
#error The WCI2 Coexistence uses the Timestamp service. Please enable the TMR Timestamp (gTimestamp_Enabled_d).
#endif
/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/
/* 38400 bps for 32MHz w/CTIMER prescaler = 32 (default) */
#define WCI2_BIT_TIME                   26u
#define WCI2_TYPE0_MSG_RX_SHIFT         3u
#define WCI2_TYPE0_MSG_TX_SHIFT         2u
#define WCI2_TYPE7_MSG_RX_PRIO_SHIFT    0u
#define WCI2_TYPE7_MSG_TX_PRIO_SHIFT    2u
/************************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
************************************************************************************/
#if gWCI2_Enabled_d || gWCI2_UseCoexistence_d
/*! *********************************************************************************
* \brief  MWS helper function used for chaining the active protocols, ordered by priority
*
* \param[in]  protocol - the protocol 
* \param[in]  priority - the priority to be set to above protocol
*
* \return  None
*
********************************************************************************** */
static void WCI2_SetPriority (mwsProtocols_t protocol, uint8_t priority);
#endif

#if gWCI2_UseCoexistence_d == 1
/*! *********************************************************************************
* \brief  Interrupt Service Routine for handling the changes of the RF Deny pin
*
* \return  None
*
********************************************************************************** */
static void rf_tx_grant_pin_changed(void);

/*! *********************************************************************************
* \brief  Interrupt Service Routine for handling the changes of the RF RX Grant pin
*
* \return  None
*
********************************************************************************** */
static void rf_rx_grant_pin_changed(void);

/*! *********************************************************************************
* \brief  Common processing for handling the changes of the RF TX/RX Grant pin
*
* \return  None
*
********************************************************************************** */
static void rf_grant_pin_changed_common(gpioInputPinConfig_t *pin);

#endif /* gWCI2_UseCoexistence_d == 1 */

/*! *********************************************************************************
* \brief  This function is used to enable the RF_DENY pin interrupt
*
* \param[in]  mode - pin interrupt mode to be set (rising edge, falling edge, etc)
*
* \return  None
*
********************************************************************************** */
static void WCI2_EnableRfDenyPinInterrupt(void);

/*! *********************************************************************************
* \brief  This function is used to disable the RF_DENY pin interrupt
*
* \return  None
*
********************************************************************************** */
static void WCI2_DisableRfDenyPinInterrupt(void);

#if gWCI2_UseCoexistence_d
static void request_grant(mwsRfState_t);
static void release_access(void);
static void bitbang_gpio(uint8_t);
#endif
/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
#if gWCI2_Enabled_d || gWCI2_UseCoexistence_d
/* Stores the id of the protocol with the Highest priority */
static mwsProtocols_t  mFirstMwsPrio = gMWS_None_c;
/* Stores the priority level for every protocol id */
static uint8_t mProtocolPriority[gMWS_None_c] =
{
    gMWS_BLEPriority_d, 
    gMWS_802_15_4Priority_d, 
    gMWS_ANTPriority_d, 
    gMWS_GENFSKPriority_d
};
/* Stores the id of the protocol with the next priority */
static mwsProtocols_t mProtocolNextPrio[gMWS_None_c] =
{
    gMWS_None_c, gMWS_None_c, gMWS_None_c, gMWS_None_c
};
#endif

#if gWCI2_Enabled_d
/* Stores the id of the protocol which uses the XCVR */
static mwsProtocols_t mActiveProtocol = gMWS_None_c;
/* Stores MWS callback functions for every protocol */
static pfMwsCallback mMwsCallbacks[gMWS_None_c] = {NULL, NULL, NULL, NULL};
static uint32_t mwsLockCount = 0;
#endif /* gWCI2_Enabled_d */

#if gWCI2_UseCoexistence_d
/* Assume that the Coexistence GPIO pins are controlled by hardware */
static gpioInputPinConfig_t  *rf_rx_deny = NULL;
static gpioInputPinConfig_t *rf_tx_deny = NULL;
static gpioInputPinConfig_t  *rf_deny = NULL;
static gpioOutputPinConfig_t *rf_request   = NULL;

static mwsRfState_t mXcvrRfState;
static uint8_t mCoexFlags;
static uint8_t mCoexEnabled;
/* Stores Coexistence callback functions for every protocol */
static pfMwsCallback mCoexCallbacks[gMWS_None_c] = {NULL, NULL, NULL, NULL};

/* Signals that the MWS module is initialized or not */
static uint8_t wci2_initialized = 0;
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
static mwsCoexStats_t coexStats;
#endif
#endif /* gWCI2_UseCoexistence_d */

/************************************************************************************
*************************************************************************************
* Exported symbols
*************************************************************************************
************************************************************************************/
/*** Callback example:

uint32_t protocolCallback ( mwsEvents_t event )
{
    uint32_t status = 0;

    switch(event)
    {
    case gMWS_Init_c:
        status = protocolInittFunction();
        break;
    case gMWS_Active_c:
        status = protocolSetActiveFunction();
        break;
    case gMWS_Abort_c:
        status = protocolAbortFunction();
        break;
    case gMWS_Idle_c:
        status = protocolNotifyIdleFunction();
        break;
    case gMWS_GetInactivityDuration_c:
        status = protocolGetInactiveDurationFunction();
        break;
    default:
        status = gMWS_InvalidParameter_c;
        break;
    }
    return status;
}
*/

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

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
mwsStatus_t WCI2_Register(mwsProtocols_t protocol, pfMwsCallback cb)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_Enabled_d
    static uint8_t initialized = 0;
    
    if (!initialized)
    {
        mActiveProtocol = gMWS_None_c;
        mFirstMwsPrio = gMWS_None_c;
        mwsLockCount = 0;
        initialized = 1;
    }
    
    if ((protocol >= gMWS_None_c) || (NULL == cb))
    {
        status = gMWS_InvalidParameter_c;
    }
    else
    {
        if( NULL == mMwsCallbacks[protocol] )
        {
            mMwsCallbacks[protocol] = cb;
            WCI2_SetPriority(protocol, mProtocolPriority[protocol]);
            cb(gMWS_Init_c); /* Signal the protocol */
        }
        else
        {
            /* Already registered! Only update callback */
            mMwsCallbacks[protocol] = cb;
        }
    }
#endif
    return status;
}

/*! *********************************************************************************
* \brief  This function will poll all other protocols for their inactivity period, 
*         and will return the minimum time until the first protocol needs to be serviced.
*
* \param[in]  currentProtocol - One of the supported MWS protocols
*
* \return  the minimum inactivity duration (in microseconds)
*
********************************************************************************** */
uint32_t WCI2_GetInactivityDuration (mwsProtocols_t currentProtocol)
{
    uint32_t duration = 0xFFFFFFFFU;
#if gWCI2_Enabled_d
    uint32_t i, temp;

    /* Get the minimum inactivity duration from all protocols */
    for (i=0; i<NumberOfElements(mMwsCallbacks); i++)
    {
        if( (i != currentProtocol) && (mMwsCallbacks[i]) )
        {
            temp = mMwsCallbacks[i](gMWS_GetInactivityDuration_c);
            if( temp < duration )
            {
                duration = temp;
            }
        }
    }
#endif
    return duration;
}

/*! *********************************************************************************
* \brief  This function try to acquire access to the XCVR for the specified protocol
*
* \param[in]  protocol - One of the supported MWS protocols
* \param[in]  force    - If set, the active protocol will be preempted
*
* \return  If access to the XCVR is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t WCI2_Acquire(mwsProtocols_t protocol, uint8_t force)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_Enabled_d

    OSA_InterruptDisable();
    
    if (gMWS_None_c == mActiveProtocol)
    {
        mActiveProtocol = protocol;
        mwsLockCount = 1;
        mMwsCallbacks[mActiveProtocol](gMWS_Active_c);
    }
    else
    {
        if (mActiveProtocol == protocol)
        { 
            mwsLockCount++;
        }
        else
        {
        /* Lower value means higher priority */
            if ((force)
#if gWCI2_UsePrioPreemption_d
                || (mProtocolPriority[mActiveProtocol] > 
                                                    mProtocolPriority[protocol])
#endif
            )
            {
                status = 
                    (mwsStatus_t)mMwsCallbacks[mActiveProtocol](gMWS_Abort_c);
                mActiveProtocol = protocol;
                mwsLockCount = 1;
                mMwsCallbacks[mActiveProtocol](gMWS_Active_c);
            }
            else
            {
                status = gMWS_Denied_c;
            }
        }
    }

    OSA_InterruptEnable();
#endif
    return status; 
}

/*! *********************************************************************************
* \brief  This function will release access to the XCVR, and will notify other 
*         protocols that the resource is idle.
*
* \param[in]  protocol - One of the supported MWS protocols
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t WCI2_Release(mwsProtocols_t protocol)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_Enabled_d

    if (mActiveProtocol != gMWS_None_c)
    {
        if (protocol == mActiveProtocol)
        {
            mwsLockCount--;
            
            if (0 == mwsLockCount)
            {
                mMwsCallbacks[mActiveProtocol](gMWS_Release_c);
                mActiveProtocol = gMWS_None_c;
                
                /* Notify other protocols that XCVR is Idle, based on the priority */
                status = WCI2_SignalIdle(protocol);
            }
        }
        else
        {
            /* Another protocol is using the XCVR */
            status = gMWS_Denied_c;
        }
    }
    else
    {
        status = WCI2_SignalIdle(protocol);
    }
#endif
    return status;
}

/*! *********************************************************************************
* \brief  Force the active protocol to Idle state.
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t WCI2_Abort(void)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_Enabled_d

    if (mActiveProtocol != gMWS_None_c)
    {
        if (mMwsCallbacks[mActiveProtocol](gMWS_Abort_c))
        {
            status = gMWS_Error_c;
        }
        mActiveProtocol = gMWS_None_c;
        mwsLockCount = 0;
    }
#endif
    return status;
}

/*! *********************************************************************************
* \brief  Returns the protocol that is currently using the XCVR
*
* \return  One of the supported MWS protocols
*
********************************************************************************** */
mwsProtocols_t WCI2_GetActiveProtocol (void)
{
#if gWCI2_Enabled_d
    return mActiveProtocol;
#else
    return gMWS_None_c;
#endif
}

/*! *********************************************************************************
* \brief  This function will notify other protocols that the specified protocol is 
*         Idle and the XCVR is unused.
*
* \param[in]  protocol - One of the supported MWS protocols
*
* \return  mwsStatus_t
*
********************************************************************************** */
mwsStatus_t WCI2_SignalIdle(mwsProtocols_t protocol)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_Enabled_d
    uint32_t i = mFirstMwsPrio;
    
    while (i != gMWS_None_c)
    {
        if (mActiveProtocol != gMWS_None_c)
        {
            break;
        }

        if ((i != protocol) && (NULL != mMwsCallbacks[i]))
        {
            if (mMwsCallbacks[i](gMWS_Idle_c))
            {
                status = gMWS_Error_c;
            }
        }
        i = mProtocolNextPrio[i];
    }
#endif
    return status;
}

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
mwsStatus_t WCI2_CoexistenceInit(void *rfReq, void *rfTxDeny, void *rfRxDeny)
{
    mwsStatus_t status = gMWS_Success_c;

#if gWCI2_UseCoexistence_d
    rf_request = (gpioOutputPinConfig_t *)rfReq;
    rf_tx_deny = (gpioInputPinConfig_t *)rfTxDeny;
    rf_rx_deny   = (gpioInputPinConfig_t *)rfRxDeny;

    /* Polarity is fixed: HI = GRANT, LO = DENY */
    rf_tx_deny->interruptModeSelect = pinInt_FallingEdge_c;
    rf_rx_deny->interruptModeSelect = pinInt_FallingEdge_c;
    
    GpioOutputPinInit(rf_request, 1);
    GpioInputPinInit(rf_tx_deny, 1);
    GpioInputPinInit(rf_rx_deny, 1);
    
    if (!wci2_initialized)
    {
        wci2_initialized = 1;

        WCI2_CoexistenceSetPriority(gMWS_CoexRxPrio_d, gMWS_CoexTxPrio_d);
        WCI2_CoexistenceEnable();

        /* We need TS service since ReleaseAccess bitbangs the GPIO */
        TMR_TimeStampInit();
        WCI2_CoexistenceReleaseAccess();


        /* RF Confirm signal must be handled by Software */
        rf_deny = rf_tx_deny;
        WCI2_DisableRfDenyPinInterrupt();
        if (gpio_success != GpioInstallIsr(rf_tx_grant_pin_changed, 
                                           gMWS_GpioIsrPrio_d,
                                           gMWS_GpioNvicPrio_d >> 1,
                                           rf_tx_deny))
        {
            status = gMWS_InvalidParameter_c;
        }
        rf_deny = rf_rx_deny;
        WCI2_DisableRfDenyPinInterrupt();
        if (gpio_success != GpioInstallIsr(rf_rx_grant_pin_changed, 
                                           gMWS_GpioIsrPrio_d,
                                           gMWS_GpioNvicPrio_d >> 1,
                                           rf_rx_deny));
    }
#endif
    return status;
}

/*! *********************************************************************************
* \brief  Enable Coexistence signals.
*
********************************************************************************** */
void WCI2_CoexistenceEnable (void)
{
#if gWCI2_UseCoexistence_d
    OSA_InterruptDisable();

    mCoexEnabled = 1;

    OSA_InterruptEnable();
#endif
}

/*! *********************************************************************************
* \brief  Disable Coexistence signals.
*
********************************************************************************** */
void WCI2_CoexistenceDisable (void)
{
#if gWCI2_UseCoexistence_d
    OSA_InterruptDisable();
    WCI2_CoexistenceReleaseAccess();

    mCoexEnabled = 0;
    OSA_InterruptEnable();
#endif
}

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
mwsStatus_t WCI2_CoexistenceRegister(mwsProtocols_t protocol, pfMwsCallback cb)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_UseCoexistence_d
    
    if( (protocol >= gMWS_None_c) || (NULL == cb) )
    {
        status = gMWS_InvalidParameter_c;
    }
    else
    {
        if (NULL == mCoexCallbacks[protocol])
        {
            mCoexCallbacks[protocol] = cb;
            WCI2_SetPriority(protocol, mProtocolPriority[protocol]);
            cb(gMWS_Init_c); /* Signal the protocol */

            if ((GpioReadPinInput(rf_tx_deny) == gWCI2_CoexRfDenyActiveState_d) ||
                (GpioReadPinInput(rf_rx_deny) == gWCI2_CoexRfDenyActiveState_d))
            {
                cb(gMWS_Abort_c);
            }
            else
            {
                cb(gMWS_Idle_c);
            }
        }
        else
        {
            /* Already registered! Only update callback */
            mCoexCallbacks[protocol] = cb;
        }
    }
#endif    
    return status;
}

/*! *********************************************************************************
* \brief  This function returns the state of RF Deny pin.
*
* \return  uint8_t !gWCI2_CoexRfDenyActiveState_d - RF Deny Inactive,
*                   gWCI2_CoexRfDenyActiveState_d - RF Deny Active
*
********************************************************************************** */
uint8_t WCI2_CoexistenceDenyState(void)
{
    uint8_t retVal = !gWCI2_CoexRfDenyActiveState_d;
#if gWCI2_UseCoexistence_d
    if ((GpioReadPinInput(rf_tx_deny) == gWCI2_CoexRfDenyActiveState_d) ||
        (GpioReadPinInput(rf_rx_deny) == gWCI2_CoexRfDenyActiveState_d))
    {
        retVal = gWCI2_CoexRfDenyActiveState_d;
    }
#endif
    return retVal;
}

/*! *********************************************************************************
* \brief  This function will register a protocol stack into MWS Coexistence module
*
* \param[in]  rxPrio - The priority of the RX sequence
* \param[in]  txPrio - The priority of the TX sequence
*
********************************************************************************** */
void WCI2_CoexistenceSetPriority(mwsRfSeqPriority_t rxPrio, mwsRfSeqPriority_t txPrio)
{
#if gWCI2_UseCoexistence_d
    OSA_InterruptDisable();

    if (rxPrio == gMWS_HighPriority)
    {
        mCoexFlags |= (1 << gMWS_RxState_c);
    }
    else
    {
        mCoexFlags &= ~(1 << gMWS_RxState_c);
    }
    
    if (txPrio == gMWS_HighPriority )
    {
        mCoexFlags |= 1 << gMWS_TxState_c;
    }
    else
    {
        mCoexFlags &= ~(1 << gMWS_TxState_c);
    }

    OSA_InterruptEnable();
#endif
}

/*! *********************************************************************************
* \brief  Request for permission to access the medium for the specified RF sequence
*
* \param[in]  newState - The RF sequence type
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceRequestAccess(mwsRfState_t newState)
{
    mwsStatus_t status = gMWS_Success_c;
#if gWCI2_UseCoexistence_d
    uint64_t timestamp;
    uint8_t askForGrant = 1;
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
    uint32_t *numRequests = NULL;
    uint32_t *numGrantWait = NULL;
    uint32_t *numGrantWaitTo = NULL;
#endif

    rf_deny = rf_rx_deny;
    if ((newState == gMWS_RxState_c) && /* doing an RX */
        ((mCoexFlags & (1 << newState)) == 0)) /* with low priority */
    {
        askForGrant = 0;
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
        coexStats.numRxRequests++;
        coexStats.numRxGrantImmediate++;
#endif
    }

    if (mCoexEnabled && askForGrant)
    {
        OSA_InterruptDisable();

        if (newState == gMWS_RxState_c)
        {
            rf_deny = rf_rx_deny;
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
            numRequests = &coexStats.numRxRequests;
            numGrantWait = &coexStats.numRxGrantWait;
            numGrantWaitTo = &coexStats.numRxGrantWaitTimeout;
#endif
        }
        else
        {
            rf_deny = rf_tx_deny;
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
            numRequests = &coexStats.numTxRequests;
            numGrantWait = &coexStats.numTxGrantWait;
            numGrantWaitTo = &coexStats.numTxGrantWaitTimeout;
#endif
        }

        request_grant(newState);

#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
        *numRequests++;
#endif
        timestamp = TMR_GetTimestamp();

        OSA_InterruptEnable();

        /* Wait for confirm signal */
        status = gMWS_Denied_c; /* assume access is denied */

        while ((TMR_GetTimestamp() - timestamp) < gWCI2_CoexGrantPinSampleDelay);

        if(GpioReadPinInput(rf_deny) != gWCI2_CoexRfDenyActiveState_d)
        {
            /* deny is when line goes from HIGH to LOW */
            WCI2_EnableRfDenyPinInterrupt();
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
            *numGrantWait++;
#endif
            status = gMWS_Success_c;
            mXcvrRfState = newState;
        }


        if (status != gMWS_Success_c)
        {
            WCI2_CoexistenceReleaseAccess();
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
            *numGrantWaitTo++;
#endif
        }
    }
#endif // gWCI2_UseCoexistence_d
    return status;
}

/*! *********************************************************************************
* \brief  This function will signal the change of the RF state, and request for permission
*
* \param[in]  newState - The new state in which the XCVR will transition
*
* \return  If RF access is not granted, gMWS_Denied_c is returned.
*
********************************************************************************** */
mwsStatus_t WCI2_CoexistenceChangeAccess(mwsRfState_t newState)
{
    mwsStatus_t status = gMWS_Success_c;

#if gWCI2_UseCoexistence_d
    if( mCoexEnabled )
    {
        if( mXcvrRfState == gMWS_IdleState_c )
        {
            status = gMWS_Denied_c;
        }
        else
        {
            mXcvrRfState = newState;

            request_grant(newState);
        }
    }
#endif
    return status;
}

/*! *********************************************************************************
* \brief  Signal externally that the Radio is not using the medium anymore.
*
********************************************************************************** */
void WCI2_CoexistenceReleaseAccess(void)
{
#if gWCI2_UseCoexistence_d
    if (mCoexEnabled)
    {
        /* Disable the RF_DENY pin interrupt */
        WCI2_DisableRfDenyPinInterrupt();

        OSA_InterruptDisable();
        mXcvrRfState = gMWS_IdleState_c;
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
        coexStats.numReleases++;
#endif

        release_access();

        OSA_InterruptEnable();
    }

#endif
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/

/*! *********************************************************************************
* \brief  This function is used to enable the RF_DENY pin interrupt
*
* \param[in]  mode - pin interrupt mode to be set (rising edge, falling edge, etc)
*
* \return  None
*
********************************************************************************** */
static void WCI2_EnableRfDenyPinInterrupt(void)
{
#if gWCI2_UseCoexistence_d
    if (rf_deny)
    {
        rf_deny->interruptModeSelect = pinInt_FallingEdge_c;
        GpioSetPinInterrupt(rf_deny->gpioPort, rf_deny->gpioPin, rf_deny->interruptModeSelect);
    }
#endif
}

/*! *********************************************************************************
* \brief  This function is used to disable the RF_DENY pin interrupt
*
* \return  None
*
********************************************************************************** */
static void WCI2_DisableRfDenyPinInterrupt(void)
{
#if gWCI2_UseCoexistence_d
    if (rf_deny)
    {
        rf_deny->interruptModeSelect = pinInt_Disabled_c;
        GpioSetPinInterrupt(rf_deny->gpioPort, rf_deny->gpioPin, rf_deny->interruptModeSelect);
    }
#endif
}

/*! *********************************************************************************
* \brief  MWS helper function used for chaining the active protocols, ordered by priority
*
* \param[in]  protocol - the protocol 
* \param[in]  priority - the priority to be set to the above protocol
*
* \return  None
*
********************************************************************************** */
#if gWCI2_Enabled_d || gWCI2_UseCoexistence_d
static void WCI2_SetPriority (mwsProtocols_t protocol, uint8_t priority)
{
    mwsProtocols_t i;
    
    if ((mFirstMwsPrio == gMWS_None_c) || 
        (priority <= mProtocolPriority[mFirstMwsPrio]))
    {
        /* Insert at the begining of the list */
        mProtocolNextPrio[protocol] = mFirstMwsPrio;
        mFirstMwsPrio = protocol;
    }
    else
    {
        i = mFirstMwsPrio;
        
        while (i != gMWS_None_c)
        {
            if (mProtocolNextPrio[i] == gMWS_None_c)
            {
                /* Insert at the end of the list */
                mProtocolNextPrio[protocol] = gMWS_None_c;
                mProtocolNextPrio[i] = protocol;
                i = gMWS_None_c;
            }
            else
            {
                if (priority <= mProtocolPriority[mProtocolNextPrio[i]])
                {
                    mProtocolNextPrio[protocol] = mProtocolNextPrio[i];
                    mProtocolNextPrio[i] = protocol;
                    i = gMWS_None_c;
                }
                else
                {
                    i = mProtocolNextPrio[i];
                }
            }
        }
    }
}
#endif

/*! *********************************************************************************
* \brief  Interrupt Service Routine for handling the changes of the RF Deny pin
*
* \return  None
*
********************************************************************************** */
#if gWCI2_UseCoexistence_d
static void rf_tx_grant_pin_changed(void)
{
    rf_grant_pin_changed_common(rf_tx_deny);
}

static void rf_rx_grant_pin_changed(void)
{
    rf_grant_pin_changed_common(rf_rx_deny);
}

static void rf_grant_pin_changed_common(gpioInputPinConfig_t *pin)
{
    uint32_t i;

    GpioClearPinIntFlag(pin);

    if (mCoexEnabled)
    {
        if (GpioReadPinInput(pin) == gWCI2_CoexRfDenyActiveState_d)
        {
            /* Abort all protocols */
            for (i=0; i<gMWS_None_c; i++)
            {
                if (mCoexCallbacks[i])
                {
                    mCoexCallbacks[i](gMWS_Abort_c);
                }
            }
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
            coexStats.numAborts++;
#endif
        }
        else
        {
            i = mFirstMwsPrio;
            
            while (i != gMWS_None_c)
            {
                if (NULL != mCoexCallbacks[i])
                {
                    mCoexCallbacks[i](gMWS_Idle_c);
                }
                i = mProtocolNextPrio[i];
            }
        }
    }
}
#endif /* gWCI2_UseCoexistence_d */

#if gWCI2_UseCoexistence_d
/*! *********************************************************************************
* \brief  MWS state query helper
*
* \return  0 if coexistence is enabled, != 0 otherwise
*
********************************************************************************** */
uint8_t WCI2_CoexistenceIsEnabled(void)
{
    return mCoexEnabled;
}
#endif /* gWCI2_UseCoexistence_d */

#if gWCI2_UseCoexistence_d
#if gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1)
/*! *********************************************************************************
* \brief  MWS stats query helper
*
* * \param[in/out]  stats - pointer where the acquired statistics will be written
*
* \return  0 if stats were succesfully retrieved, != 0 otherwise
*
********************************************************************************** */
uint8_t WCI2_GetCoexStats(mwsCoexStats_t *stats)
{
    mwsStatus_t status = gMWS_Error_c;
    if (stats)
    {
        status = gMWS_Success_c;

        memset(stats, 0, sizeof(mwsCoexStats_t));

        stats->numTxRequests = coexStats.numTxRequests;
        stats->numTxGrantWait = coexStats.numTxGrantWait;
        stats->numTxGrantWaitTimeout = coexStats.numTxGrantWaitTimeout;

        stats->numRxRequests = coexStats.numRxRequests;
        stats->numRxGrantImmediate = coexStats.numRxGrantImmediate;
        stats->numRxGrantWait = coexStats.numRxGrantWait;
        stats->numRxGrantWaitTimeout = coexStats.numRxGrantWaitTimeout;

        stats->numReleases = coexStats.numReleases;
        stats->numAborts = coexStats.numAborts;
    }
    return (uint8_t)status;
}
#endif /*  gWCI2_EnableCoexistenceStats_d && (gWCI2_EnableCoexistenceStats_d == 1) */

static void request_grant(mwsRfState_t newState)
{
    uint64_t timestamp;
    uint8_t type7_msg = 0b11100000;
    uint8_t type0_msg = 0b00000000;

    /* Send "Request Prio" */
    if (newState == gMWS_RxState_c)
    {
        type0_msg |= 1 << WCI2_TYPE0_MSG_RX_SHIFT;
    }
    else
    {
        type0_msg |= 1 << WCI2_TYPE0_MSG_TX_SHIFT;
    }

    type7_msg |= (mCoexFlags & (1 << gMWS_RxState_c)) << 
                                                WCI2_TYPE7_MSG_RX_PRIO_SHIFT;
    type7_msg |= (mCoexFlags & (1 << gMWS_TxState_c)) << 
                                                WCI2_TYPE7_MSG_TX_PRIO_SHIFT;

    OSA_InterruptDisable();

    bitbang_gpio(type7_msg);
    timestamp = TMR_GetTimestamp();
    while ((TMR_GetTimestamp() - timestamp) < gWCI2_CoexPrioSignalTime_d);
    bitbang_gpio(type0_msg);

    OSA_InterruptEnable();
}

static void release_access()
{
    uint8_t type7_msg = 0b11100000;
    bitbang_gpio(type7_msg);
}

static void bitbang_gpio(uint8_t byte_to_send)
{
    uint32_t ts, bit;

    OSA_InterruptDisable();
    for (bit = 0; bit < 10; bit++)
    {
        //ts = (uint32_t)TMR_GetTimestamp();
        ts = CTIMER1->TC;
        switch (bit)
        {
            case 0:
                /* Start bit */
                GpioClearPinOutput(rf_request);
                break;

            case 9:
                /* Stop bit */
                GpioSetPinOutput(rf_request);
                break;

            default:
                if (byte_to_send & (1 << (bit - 1)))
                {
                    GpioSetPinOutput(rf_request);
                }
                else
                {
                    GpioClearPinOutput(rf_request);
                }
                break;
        }
        do { } while ((CTIMER1->TC - ts) < WCI2_BIT_TIME);
    }
    OSA_InterruptEnable();
}
#endif /* gWCI2_UseCoexistence_d */
