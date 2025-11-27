/*!
 * Copyright 2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file fwk_sysworkq.c
 * \brief Implements the system workqueue which is a workqueue available to any application.
 *
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "fwk_workq.h"
#include "fsl_os_abstraction.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

#if CONFIG_FWK_SYSWORKQ_PRIO >= OSA_TASK_PRIORITY_MIN
#error CONFIG_FWK_SYSWORKQ_PRIO must be numerically less than OSA_TASK_PRIORITY_MIN. \
       Lower numeric values indicate higher priority, and OSA_TASK_PRIORITY_MIN represents the IDLE task priority.
#endif

/* -------------------------------------------------------------------------- */
/*                               Private memory                               */
/* -------------------------------------------------------------------------- */

static fwk_workq_t sysworkq;
FWK_WORKQ_THREAD_DEFINE(sysworkq_thread, CONFIG_FWK_SYSWORKQ_PRIO, CONFIG_FWK_SYSWORKQ_STACK_SIZE);

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int WORKQ_InitSysWorkQ(void)
{
    return WORKQ_Start(&sysworkq, FWK_WORKQ_THREAD(sysworkq_thread));
}

int WORKQ_Submit(fwk_work_t *work)
{
    return WORKQ_SubmitToQueue(&sysworkq, work);
}
