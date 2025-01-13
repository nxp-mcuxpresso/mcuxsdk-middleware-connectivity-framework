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

#ifndef CONFIG_FWK_SYSWORKQ_STACK_SIZE
#define CONFIG_FWK_SYSWORKQ_STACK_SIZE 256U
#endif

#ifndef CONFIG_FWK_SYSWORKQ_PRIO
#if USE_RTOS
#define CONFIG_FWK_SYSWORKQ_PRIO PRIORITY_RTOS_TO_OSA(2U)
#else
#define CONFIG_FWK_SYSWORKQ_PRIO (OSA_TASK_PRIORITY_MIN - 2U)
#endif
#endif

/* -------------------------------------------------------------------------- */
/*                               Private memory                               */
/* -------------------------------------------------------------------------- */

static fwk_workq_t sysworkq;

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int WORKQ_InitSysWorkQ(void)
{
    return WORKQ_Start(&sysworkq, CONFIG_FWK_SYSWORKQ_STACK_SIZE, CONFIG_FWK_SYSWORKQ_PRIO);
}

int WORKQ_Submit(fwk_work_t *work)
{
    return WORKQ_SubmitToQueue(&sysworkq, work);
}
