/*
 * Copyright 2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_nbu_dbg.h"
#include "fwk_platform.h"
#include "fwk_debug_struct.h"
#include "fwk_platform_dbg.h"
#include "fwk_platform_ble.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
#ifndef IS_NBU_STUCK_MAX_TIMER_US
#define IS_NBU_STUCK_MAX_TIMER_US 10000000U /* 10s */
#endif

/* HCI event generated on debug struct dump request */
/* HCI packet structure constants */
#define HCI_VENDOR_SPECIFIC_EVENT_CODE (0xFFU)

#define HCI_VENDOR_SUBEVENT_FATAL_ERROR   (0xF1U)
#define HCI_MAX_VENDOR_EVENT_PAYLOAD_SIZE (255U)
#define HCI_EVENT_BUFFER_SIZE             (HCI_MAX_VENDOR_EVENT_PAYLOAD_SIZE + 2U) /* event code, length, payload */

/* HCI vendor event packet offsets */
#define HCI_EVENT_CODE_OFFSET               (0U)
#define HCI_PARAMETER_LENGTH_OFFSET         (1U)
#define HCI_VENDOR_SUBEVENT_CODE_OFFSET     (2U)
#define HCI_VENDOR_BUFFER_ID_OFFSET         (3U)
#define HCI_VENDOR_LAST_SEGMENT_FLAG_OFFSET (4U)
#define HCI_VENDOR_DATA_OFFSET              (5U)

#define HCI_MAX_VENDOR_PAYLOAD_HEADER_SIZE (3U) /* subevent, buffer id, last segment flag */
#define HCI_MAX_VENDOR_DATA_SIZE           (HCI_MAX_VENDOR_EVENT_PAYLOAD_SIZE - HCI_MAX_VENDOR_PAYLOAD_HEADER_SIZE)

/* Vendor event flags */
#define HCI_VENDOR_SEGMENT_NOT_LAST (0U)
#define HCI_VENDOR_SEGMENT_IS_LAST  (1U)

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */
extern uint8_t m_sqram_debug_start[];
extern uint8_t dbg_ext_logging_start[];
extern uint8_t dbg_ext_logging_end[];

nbu_debug_struct_t *debug_struct = (nbu_debug_struct_t *)(m_sqram_debug_start);

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
static nbu_dbg_system_cb_t nbu_dbg_system_cb = (nbu_dbg_system_cb_t)NULL;
/* Configuration bitmask for HCI vendor event feature */
static uint32_t nbu_dbg_hci_vendor_event_config = NBUDBG_HCI_EVENT_NONE;
/* Static buffer for HCI vendor event packet transmission */
static uint8_t nbudbg_hci_vndr_evt[HCI_EVENT_BUFFER_SIZE]; /* Packet type not included */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */
/*!
 * Send debug buffer as HCI vendor event
 *
 * This function fragments any debug buffer into multiple HCI vendor
 * events if necessary. The receiver must concatenate packets until
 * the "last segment" flag is set.
 *
 * buffer_id: Identifier for the type of debug buffer being sent
 * data: Pointer to the debug buffer to be transmitted
 * length: Size of the debug buffer in bytes
 *
 * return int 0 if success, negative value if error (-1: Invalid parameter)
 */
static int NBUDBG_SendHciEvent(uint8_t buffer_id, const uint8_t *data, uint32_t length)
{
    uint32_t bytes_to_be_sent = length;
    int      ret              = 0;

    do
    {
        /* Validate input parameter */
        if ((data == NULL) || (length == 0U))
        {
            ret = -1;
            break;
        }

        /* Initialize HCI packet header fields that remain constant */
        nbudbg_hci_vndr_evt[HCI_EVENT_CODE_OFFSET]               = HCI_VENDOR_SPECIFIC_EVENT_CODE;
        nbudbg_hci_vndr_evt[HCI_VENDOR_SUBEVENT_CODE_OFFSET]     = HCI_VENDOR_SUBEVENT_FATAL_ERROR;
        nbudbg_hci_vndr_evt[HCI_VENDOR_BUFFER_ID_OFFSET]         = buffer_id;
        nbudbg_hci_vndr_evt[HCI_VENDOR_LAST_SEGMENT_FLAG_OFFSET] = HCI_VENDOR_SEGMENT_NOT_LAST;

        /* Fragment and send debug data */
        while (bytes_to_be_sent > 0U)
        {
            /* Calculate payload size for this segment */
            uint8_t payload_len = MIN(HCI_MAX_VENDOR_DATA_SIZE, bytes_to_be_sent);

            /* Update packet header for this segment */
            nbudbg_hci_vndr_evt[HCI_PARAMETER_LENGTH_OFFSET] = payload_len + HCI_MAX_VENDOR_PAYLOAD_HEADER_SIZE;

            /* Mark last segment */
            if (bytes_to_be_sent == payload_len)
            {
                nbudbg_hci_vndr_evt[HCI_VENDOR_LAST_SEGMENT_FLAG_OFFSET] = HCI_VENDOR_SEGMENT_IS_LAST;
            }

            /* Copy payload data */
            (void)memcpy(&nbudbg_hci_vndr_evt[HCI_VENDOR_DATA_OFFSET], data, payload_len);

            /* Send HCI vendor event */
            (void)PLATFORM_SendHciVendorEvent(nbudbg_hci_vndr_evt, payload_len + HCI_VENDOR_DATA_OFFSET);

            /* Update for next segment */
            bytes_to_be_sent -= payload_len;
            data = &data[payload_len];
        }
    } while (false);

    return ret;
}

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

void NBUDBG_StateCheck(void)
{
    nbu_dbg_context_t nbu_event = {0U};

    (void)PLATFORM_IsNbuWarningSet(&nbu_event.nbu_warning_count);
    nbu_event.nbu_error_count = (uint8_t)PLATFORM_IsNbuFaultSet();

    extern bool PLATFORM_IsNbuStuck(uint32_t nbuWatchdogDurationInUs);
    if (PLATFORM_IsNbuStuck(IS_NBU_STUCK_MAX_TIMER_US))
    {
        nbu_event.nbu_is_halted = 1U;
    }
    /* Check if any debug condition is detected and notify via callback */
    if ((nbu_event.nbu_error_count > 0U) || (nbu_event.nbu_warning_count > 0U) || (nbu_event.nbu_is_halted > 0U))
    {
        if (nbu_dbg_system_cb != NULL)
        {
            nbu_dbg_system_cb(&nbu_event);
        }
    }
}

void NBUDBG_RegisterNbuDebugNotificationCb(nbu_dbg_system_cb_t cb)
{
    nbu_dbg_system_cb = cb;
}

int NBUDBG_StructDump(nbu_debug_struct_t *debug_info)
{
    int      status             = 0;
    uint32_t nbu_dbg_struct_len = (uint32_t)sizeof(nbu_debug_struct_t);
    uint8_t *ram_log_ptr;
    uint32_t ram_log_len;

    /* Need to wake NBU domain to access the debug structure */
    PLATFORM_RemoteActiveReq();
    /* Send debug struct if enabled */
    if ((nbu_dbg_hci_vendor_event_config & NBUDBG_HCI_EVENT_DEBUG_STRUCT) != 0U)
    {
        /* Send debug information as HCI vendor event */
        (void)NBUDBG_SendHciEvent(NBUDBG_BUFFER_ID_DEBUG_STRUCT, (uint8_t *)(void *)debug_struct, nbu_dbg_struct_len);
    }
    /* Send RAM log if enabled */
    if ((nbu_dbg_hci_vendor_event_config & NBUDBG_HCI_EVENT_RAM_LOG) != 0U)
    {
        ram_log_ptr = dbg_ext_logging_start;
        ram_log_len = (uint32_t)(dbg_ext_logging_end - dbg_ext_logging_start + 1U);

        /* Dump RAMLOG buffer as HCI vendor event */
        (void)NBUDBG_SendHciEvent(NBUDBG_BUFFER_ID_RAM_LOG, ram_log_ptr, ram_log_len);
    }

    if (debug_info != NULL)
    {
        (void)memcpy(debug_info, debug_struct, nbu_dbg_struct_len);
    }
    PLATFORM_RemoteActiveRel();

    return status;
}

void NBUDBG_ConfigureHciVendorEvent(uint32_t config_mask)
{
    nbu_dbg_hci_vendor_event_config = config_mask;
}

void NBUDBG_RegisterHciLogCallback(platform_hci_log_cb_t cb)
{
    PLATFORM_RegisterHciLogCallback(cb);
}
