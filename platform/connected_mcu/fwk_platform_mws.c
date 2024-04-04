/* -------------------------------------------------------------------------- */
/*                             Copyright 2022 NXP                             */
/*                            All rights reserved.                            */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include "fsl_common.h"
#include "fwk_platform_mws.h"
#include "fsl_adapter_rpmsg.h"

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

static RPMSG_HANDLE_DEFINE(mwsRpmsgHandle);
static const hal_rpmsg_config_t mwsRpmsgConfig = {
    .local_addr  = 80,
    .remote_addr = 70,
};

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */
void PLATFORM_InitMwsRpmsg(void)
{
    hal_rpmsg_config_t mws_rpmsg_config = mwsRpmsgConfig;
    if (kStatus_HAL_RpmsgSuccess != HAL_RpmsgInit((hal_rpmsg_handle_t)mwsRpmsgHandle, &mws_rpmsg_config))
    {
        assert(0);
    }
    return;
}

hal_rpmsg_handle_t PLATFORM_GetMwsRpmsgHandle(void)
{
    return (hal_rpmsg_handle_t)mwsRpmsgHandle;
}
