/*
 * Copyright 2024-2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "stdint.h"

/* -------------------------------------------------------------------------- */
/*                               Public macros                                */
/* -------------------------------------------------------------------------- */
/* Maximum number of tasks to display/track during fault analysis */
#ifndef SYS_DEBUG_MAX_TASKS_NB
#define SYS_DEBUG_MAX_TASKS_NB 15
#endif

/* -------------------------------------------------------------------------- */
/*                           Public type definition                           */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint32_t thread_entry_addr; /* Task entry function / handler addr on FreeRTOS */
    char     thread_name[8];    /* First caracteres of the thread name - NULL terminating */
} dbg_thread_info;

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */
/**
 * \brief Dumps comprehensive task/thread statistics and call stacks for debugging
 *
 * This function provides detailed information about tasks/threads in the system,
 * including their current state, priority, runtime statistics, and stack information.
 * It also generates call stacks for each task/thread to help with debugging.
 *
 * \note This function is only available when using FreeRTOS or ThreadX RTOS
 * \warning If SYS_DEBUG_MAX_TASKS_NB is too small, the task list may be incomplete
 *
 * \param[out] current_thread Pointer to current thread info
 */
void sys_dump_task_stats(void);

/*!
 * \brief Get information about the currently running task
 *
 * \param[out] thread_info Pointer to dbg_thread_info structure to be filled
 *
 * \return 0 if successful, negative otherwise
 */
int sys_get_current_task_info(dbg_thread_info *thread_info);
