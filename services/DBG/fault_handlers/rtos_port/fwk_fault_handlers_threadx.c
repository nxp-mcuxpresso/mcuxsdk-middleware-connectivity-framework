/*
 * Copyright 2024-2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdbool.h>
#include "fwk_fault_handlers_rtos_port.h"
#include "fwk_fault_handlers_utils.h"
#include "tx_api.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */
extern TX_THREAD *_tx_thread_created_ptr;
extern TX_THREAD *_tx_thread_current_ptr;
extern ULONG      _tx_thread_created_count;

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */
void sys_dump_task_stats(void)
{
    /* Check if there are any threads created */
    if ((_tx_thread_created_ptr == TX_NULL) || (_tx_thread_created_count == 0U))
    {
        PRINTF("No threads created in the system\r\n");
    }
    else
    {
        static const char thread_state_char[] = {
            'R', /* TX_READY 0*/
            'C', /* TX_COMPLETED 1*/
            'T', /* TX_TERMINATED 2*/
            'S', /* TX_SUSPENDED 3*/
            'Z', /* TX_SLEEP 4*/
            'Q', /* TX_QUEUE_SUSP 5*/
            'M', /* TX_SEMAPHORE_SUSP 6*/
            'E', /* TX_EVENT_FLAG 7*/
            'B', /* TX_BLOCK_MEMORY 8*/
            'Y', /* TX_BYTE_MEMORY 9*/
            'I', /* TX_IO_DRIVER 10*/
            'F', /* TX_FILE 11*/
            'P', /* TX_TCP_IP 12*/
            'U', /* TX_MUTEX_SUSP 13*/
            'X'  /* Unknown/Invalid 14*/
        };

        TX_THREAD *thread_ptr;
        ULONG      thread_count = 0U;
        UINT       thread_state_index;

        PRINTF("==========THREADX TASK STATUS==========\r\n");

        PRINTF("%s\t%s  %s  %s     %s   %s\r\n", "Thread Name", "State", "PRI", "RunCount", "StackStart", "Stackend");

        /* Iterate through the circular list of threads */
        thread_ptr = _tx_thread_created_ptr;

        do
        {
            /* Map ThreadX states to our character array */
            thread_state_index = thread_ptr->tx_thread_state;
            if (thread_state_index >= (sizeof(thread_state_char) - 1U))
            {
                thread_state_index = sizeof(thread_state_char) - 1U; /* Unknown */
            }

            PRINTF("%s%s\t%c\t%d\t%d\t0x%x\t0x%x\r\n", thread_ptr->tx_thread_name,
                   (strlen(thread_ptr->tx_thread_name) < 8U) ? "\t" : "", /* Adjust tabulation */
                   thread_state_char[thread_state_index], (int)thread_ptr->tx_thread_priority,
                   (int)thread_ptr->tx_thread_run_count, (unsigned int)thread_ptr->tx_thread_stack_start,
                   (unsigned int)thread_ptr->tx_thread_stack_end);

            thread_count++;
            thread_ptr = thread_ptr->tx_thread_created_next;
        } while ((thread_ptr != _tx_thread_created_ptr) && (thread_count < _tx_thread_created_count));

        PRINTF("\r\n==========ThreadX Thread Callstacks========== (code start=0x%x code end=0x%x)\r\n",
               (unsigned int)_TEXT_START, (unsigned int)_TEXT_END);

        /* Iterate through threads for callstack dump */
        thread_ptr   = _tx_thread_created_ptr;
        thread_count = 0U;

        do
        {
            PRINTF("%s : \r\n", thread_ptr->tx_thread_name);
            sys_dump_callstack((unsigned int)thread_ptr->tx_thread_stack_ptr,
                               (unsigned int)thread_ptr->tx_thread_stack_end);
            thread_count++;
            thread_ptr = thread_ptr->tx_thread_created_next;
        } while ((thread_ptr != _tx_thread_created_ptr) && (thread_count < _tx_thread_created_count));
    }
}

int sys_get_current_task_info(dbg_thread_info *thread_info)
{
    int status = 0;

    do
    {
        if (thread_info == NULL)
        {
            status = -1;
            break;
        }

        if (_tx_thread_current_ptr == NULL)
        {
            status = -2;
            break;
        }

        thread_info->thread_entry_addr = (uint32_t)_tx_thread_current_ptr->tx_thread_entry;
        if (_tx_thread_current_ptr->tx_thread_name != NULL)
        {
            /* Copy up to 7 characters and ensure null termination */
            strncpy(thread_info->thread_name, _tx_thread_current_ptr->tx_thread_name, 7);
        }
        else
        {
            /* No thread name available, set default */
            strncpy(thread_info->thread_name, "UNKNOWN", 7);
        }
        thread_info->thread_name[7] = '\0';
    } while (false);

    return status;
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
