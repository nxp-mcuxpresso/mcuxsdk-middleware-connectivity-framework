/*
 * Copyright 2025-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWK_DEBUG_NBU_H__
#define __FWK_DEBUG_NBU_H__

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdint.h>
#include <stdbool.h>
#include "fwk_debug_struct.h"
#include "fwk_platform_ble.h"

/* -------------------------------------------------------------------------- */
/*                               Public macros                                */
/* -------------------------------------------------------------------------- */
/* Debug buffer identifiers */
#define NBUDBG_BUFFER_ID_DEBUG_STRUCT (0x00U)
#define NBUDBG_BUFFER_ID_RAM_LOG      (0x01U) /* Not supported yet */
#define NBUDBG_BUFFER_ID_STALL_EVENT  (0x02U)

/* HCI vendor event configuration bitmask */
#define NBUDBG_HCI_EVENT_NONE         (0U)
#define NBUDBG_HCI_EVENT_DEBUG_STRUCT (1U << 0U)
#define NBUDBG_HCI_EVENT_RAM_LOG      (1U << 1U)
#define NBUDBG_HCI_EVENT_STALL_EVENT  (1U << 2U)
#define NBUDBG_HCI_EVENT_ALL          (NBUDBG_HCI_EVENT_DEBUG_STRUCT | NBUDBG_HCI_EVENT_RAM_LOG | NBUDBG_HCI_EVENT_STALL_EVENT)

/* -------------------------------------------------------------------------- */
/*                           Public type definitions                          */
/* -------------------------------------------------------------------------- */
/*!
 * \brief NBU debug event id
 *
 */
typedef struct nbu_dbg_context
{
    uint8_t nbu_error_count;
    uint8_t nbu_warning_count;
    uint8_t nbu_is_halted; /* 1 -> halted*/
} nbu_dbg_context_t;

typedef void (*nbu_dbg_system_cb_t)(const nbu_dbg_context_t *nbu_event);

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/**
 * \brief Check NBU state for faults, asserts, and warnings
 *
 * When a new condition is detected, the registered callback (via
 * NBUDBG_RegisterNbuDebugNotificationCb) will be invoked.
 *
 * \note Not thread-safe, this function must be called from a single context only (e.g., IDLE task).
 * \note NBU shall be using fault handlers to be able to check its status.
 * \note Typical usage: Call periodically from the IDLE task to monitor NBU health.
 */
void NBUDBG_StateCheck(void);

/*!
 * \brief Register a NBU debug notification callback. Will be called upon NBU fault/warning detection.
 *
 * \param[in] cb callback to be registered
 */
void NBUDBG_RegisterNbuDebugNotificationCb(nbu_dbg_system_cb_t cb);

/*!
 * \brief Extract NBU debug information.
 *
 * This function can dump NBU debug information through HCI vendor events based on
 * the configuration set via NBUDBG_ConfigureHciVendorEvent(). If a non-NULL pointer is provided,
 * the debug structure will also be copied to the output parameter.
 *
 * \param[out] debug_struct pointer to structure to be filled with the extracted
 *                          debug structure from NBU. Can be NULL if only HCI
 *                          event transmission is needed.
 *
 * \return int 0 if success, negative value if error
 */
int NBUDBG_StructDump(nbu_debug_struct_t *debug_info);

/*!
 * \brief Configure HCI vendor event transmission for debug information
 *
 * \details This function configures which debug information should be transmitted
 *          as HCI vendor events when NBUDBG_StructDump() is called. Users can
 *          selectively enable debug structure, RAM log, or both using bitmask flags.
 *          The feature is disabled by default to avoid unwanted HCI traffic.
 *
 * \param[in] config_mask Bitmask to configure HCI vendor event transmission:
 *                        - NBUDBG_HCI_EVENT_NONE: Disable all HCI vendor events
 *                        - NBUDBG_HCI_EVENT_DEBUG_STRUCT: Enable debug structure transmission
 *                        - NBUDBG_HCI_EVENT_RAM_LOG: Enable RAM log transmission
 *                        - NBUDBG_HCI_EVENT_ALL: Enable both debug structure and RAM log
 *                        Multiple flags can be OR'ed together.
 */
void NBUDBG_ConfigureHciVendorEvent(uint32_t config_mask);

/*!
 * \brief Register HCI logging callback
 *
 * \details This function registers the HCI logging callback with the platform.
 *          The callback will be invoked for all HCI packets (TX and RX).
 *
 * \param[in] cb callback to be registered, NULL to unregister
 */
void NBUDBG_RegisterHciLogCallback(platform_hci_log_cb_t cb);

#endif /*  __FWK_DEBUG_NBU_H__ */
