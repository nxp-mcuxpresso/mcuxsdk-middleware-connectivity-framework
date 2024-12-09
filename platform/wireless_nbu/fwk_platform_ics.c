/*
 * Copyright 2021, 2024 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */
#include <stdbool.h>

#include "fsl_common.h"
#include "fwk_platform_ics.h"
#include "fwk_platform.h"
#include "fwk_platform_sensors.h"
#include "fwk_platform_lowpower.h"
#include "FunctionLib.h"
#include "fsl_adapter_rpmsg.h"
#include "fwk_rf_sfc.h"
#include "fwk_debug.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                              Public prototypes                             */
/* -------------------------------------------------------------------------- */

/* declared here to avoid including ble_controller header file */
extern uint32_t Controller_HandleNbuApiReq(uint8_t *api_return, uint8_t *data, uint32_t data_len);
extern bool     Controller_EnableSecurityFeature();

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

static int                       PLATFORM_FwkSrvSendPacket(eFwkSrvMsgType msg_type, void *msg, uint16_t msg_lg);
static hal_rpmsg_return_status_t PLATFORM_FwkSrv_RxCallBack(void *param, uint8_t *data, uint32_t len);
static bool                      FwkSrv_MsgTypeInExpectedSet(uint8_t msg_type);
#if defined(gPlatformIcsNbuDeferredNbuApi2Idle_d) && (gPlatformIcsNbuDeferredNbuApi2Idle_d == 1)
static void PLATFORM_FwkSrvSendNbuApiIndicationLater(void *msg, uint16_t msg_lg);

static uint32_t platformNbuApiIndication[32];
static uint16_t platformLastNbuApiIndicationSize = 0U;
static bool     platformNbuApiIndicationPending  = 0U;
#endif

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

static RPMSG_HANDLE_DEFINE(fwkRpmsgHandle);
const static hal_rpmsg_config_t fwkRpmsgConfig = {
    .local_addr  = 100,
    .remote_addr = 110,
};

__attribute__((weak)) void RNG_SetExternalSeed(uint8_t *external_seed);
__attribute__((weak)) void RNG_SetExternalSeed(uint8_t *external_seed)
{
    (void)external_seed; /* Stub of the RNG_SetExternalSeed() function */
}

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

int PLATFORM_FwkSrvInit(void)
{
    int result = 0;

    static bool_t mFwkSrvInit = FALSE;

    do
    {
        if (mFwkSrvInit == TRUE)
        {
            result = 1;
            break;
        }
        if (kStatus_HAL_RpmsgSuccess !=
            HAL_RpmsgInit((hal_rpmsg_handle_t)fwkRpmsgHandle, (hal_rpmsg_config_t *)&fwkRpmsgConfig))
        {
            result = -1;
            break;
        }

        if (kStatus_HAL_RpmsgSuccess !=
            HAL_RpmsgInstallRxCallback((hal_rpmsg_handle_t)fwkRpmsgHandle, PLATFORM_FwkSrv_RxCallBack, NULL))
        {
            result = -2;
            break;
        }
        /* Flag initialization on module */
        mFwkSrvInit = TRUE;
    } while (false);

    return result;
}

int PLATFORM_SendNbuVersionIndication(void)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuVersionIndication_c, (void *)&nbu_version, sizeof(NbuInfo_t));
}

int PLATFORM_NotifyNbuInitDone(void)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuInitDone_c, (void *)NULL, 0);
}

int PLATFORM_NotifyNbuMemFull(unsigned short poolId, uint16_t bufferSize)
{
    NbuDbgMemInfo_t memInfo;
    memInfo.poolId     = poolId;
    memInfo.bufferSize = bufferSize;

    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuMemFullIndication_c, (void *)&memInfo, sizeof(memInfo));
}

int PLATFORM_FwkSrvSetLowPowerConstraint(uint8_t mode)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvHostSetLowPowerConstraint_c, (void *)&mode, sizeof(mode));
}

int PLATFORM_FwkSrvReleaseLowPowerConstraint(uint8_t mode)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvHostReleaseLowPowerConstraint_c, (void *)&mode, sizeof(mode));
}

int PLATFORM_NotifyNbuIssue(void)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuIssueIndication_c, (void *)NULL, 0);
}

int PLATFORM_NotifySecurityEvents(uint32_t securityEventBitmask)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuSecurityEventIndication_c, (void *)&securityEventBitmask,
                                     sizeof(uint32_t));
}

void PLATFORM_FwkSrvFreeRxPacket(void *pPacket)
{
    HAL_RpmsgFreeRxBuffer(fwkRpmsgHandle, (uint8_t *)pPacket - 4U);
}

int PLATFORM_FwkSrvFroInfo(uint16_t freq, int16_t ppm_mean, int16_t ppm, uint16_t fro_trim)
{
    uint8_t tab[8];
    tab[0] = (uint8_t)freq;
    tab[1] = (uint8_t)(freq >> 8U);
    tab[2] = (uint8_t)ppm_mean;
    tab[3] = (uint8_t)(ppm_mean >> 8U);
    tab[4] = (uint8_t)ppm;
    tab[5] = (uint8_t)(ppm >> 8U);
    tab[6] = (uint8_t)fro_trim;
    tab[7] = (uint8_t)(fro_trim >> 8U);
    return (PLATFORM_FwkSrvSendPacket(gFwkSrvFroNotification_c, tab, 8 * sizeof(tab)));
}

int PLATFORM_RequestRngSeedToHost(void)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuRequestRngSeed_c, (void *)NULL, 0);
}

int PLATFORM_FwkSrvRequestNewTemperature(uint32_t periodic_interval_ms)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuRequestNewTemperature_c, (void *)&periodic_interval_ms,
                                     sizeof(uint32_t));
}

#if defined(gPlatformIcsNbuDeferredNbuApi2Idle_d) && (gPlatformIcsNbuDeferredNbuApi2Idle_d == 1)
void PLATFORM_FwkSrvCheckAndSendNbuApiIndicationInIdle(void)
{
    uint32_t irqMask = 0U;

    /* Interrupt masking should not be necessary as the NbuApiIndication function is blocking on host side just added by
     * security */
    irqMask = PLATFORM_SET_INTERRUPT_MASK();

    if (platformNbuApiIndicationPending > 0U)
    {
        PLATFORM_FwkSrvSendPacket(gFwkSrvNbuApiIndication_c, platformNbuApiIndication,
                                  platformLastNbuApiIndicationSize);
        platformNbuApiIndicationPending--;
    }
    PLATFORM_CLEAR_INTERRUPT_MASK(irqMask);
}
#endif

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */
static int PLATFORM_FwkSrvSendPacket(eFwkSrvMsgType msg_type, void *msg, uint16_t msg_lg)
{
    uint8_t *buf    = NULL;
    int      result = 0;
    uint32_t sz     = ((uint32_t)msg_lg + 1);

    PWR_DBG_LOG("fwk Send Pkt %x type=%d sz=%d", msg, msg_type, msg_lg);

    /* Request SOC XBAR access */
    PLATFORM_RemoteActiveReqWithoutDelay();

    do
    {
        buf = HAL_RpmsgAllocTxBuffer((hal_rpmsg_handle_t)fwkRpmsgHandle, sz);
        if (NULL == buf)
        {
            result = -1;
            break;
        }
        buf[0] = (uint8_t)msg_type;
        if (msg != NULL && msg_lg != 0)
        {
            FLib_MemCpy(&buf[1], (uint8_t *)msg, msg_lg);
        }

        if (kStatus_HAL_RpmsgSuccess != HAL_RpmsgNoCopySend((hal_rpmsg_handle_t)fwkRpmsgHandle, buf, (uint32_t)sz))
        {
            result = -2;
            break;
        }
    } while (false);

    /* Release SOC XBAR access */
    PLATFORM_RemoteActiveRel();

    return result;
}

static hal_rpmsg_return_status_t PLATFORM_FwkSrv_RxCallBack(void *param, uint8_t *data, uint32_t len)
{
    hal_rpmsg_return_status_t res = kStatus_HAL_RL_RELEASE;
    uint8_t                   msg_type;
    uint32_t                  data_len;

    msg_type = data[0];
    data_len = len - 1U;

    PWR_DBG_LOG("fwk Rcv Pkt %x type=%d sz=%d", data, msg_type, data_len);

    if (FwkSrv_MsgTypeInExpectedSet(msg_type))
    {
        switch (msg_type)
        {
            case gFwkSrvNbuVersionRequest_c:
                PLATFORM_SendNbuVersionIndication();
                break;

            case gFwkSrvXtal32MTrimIndication_c:
                PLATFORM_SetXtal32MhzTrim(data[1]);
                break;

            case gFwkSrvNbuApiRequest_c:
            {
                uint32_t nb_returns;
                uint8_t  ind[1U + NBU_API_MAX_RETURN_PARAM_LENGTH]; /* rpmsg status + API status + API outputs */
                uint16_t ind_len;

                /* execute the API */
                nb_returns = Controller_HandleNbuApiReq(&ind[1], data + 1U, len - 1U);
                assert((nb_returns > 0U) && (nb_returns <= NBU_API_MAX_RETURN_PARAM_LENGTH));

                /* indication message: 1 byte rpmsg status followed by 4 bytes api status */
                if (nb_returns != 0)
                {
                    ind[0U] = 1U; // rpmsg success, API could still fail
                    ind_len = 1U + nb_returns;
                }
                else
                {
                    ind[0U] = 0U; // rpmsg failure: API does not exist etc.
                    ind_len = 1U;
                }
#if defined(gPlatformIcsNbuDeferredNbuApi2Idle_d) && (gPlatformIcsNbuDeferredNbuApi2Idle_d == 1)
                /* Delay the moment we send the message over the rpmsg to avoid calling it from ISR which is not
                 * supported */
                PLATFORM_FwkSrvSendNbuApiIndicationLater((void *)&ind[0U], ind_len);
#else
                PLATFORM_FwkSrvSendPacket(gFwkSrvNbuApiIndication_c, (void *)&ind[0U], ind_len);
#endif
            }
            break;

            case gFwkTemperatureIndication_c:
            {
                /* data[0] is the API id */
                /* data[1-4] is the temperature as signed 32 bits - little endian */
                int32_t temperature = *((int32_t *)&data[1]);
                PLATFORM_SaveTemperatureValue(temperature);
            }
            break;

            case gFwkSrvHostChipRevision_c:
                PLATFORM_SetChipRevision(data[1]);
                break;

            case gFwkSrvNbuSecureModeRequest_c:
            {
#if defined(DEBUG)
                bool rmpsg_status;

                /* execute the API */
                rmpsg_status = Controller_EnableSecurityFeature();
                assert(rmpsg_status);
                (void)rmpsg_status;
#else  /*defined(DEBUG)*/
                (void)Controller_EnableSecurityFeature();
#endif /*defined(DEBUG)*/
            }
            break;
            case gFwkSrvNbuWakeupDelayLpoCycle_c:
                PLATFORM_SetWakeupDelay(data[1]);
                break;

            case gFwkSrvNbuUpdateFrequencyConstraintFromHost:
                PLATFORM_SetFrequencyConstraintFromHost(data[1]);
                break;

            case gFwkSrvNbuSetRfSfcConfig_c:
            {
                sfc_config_t sfc_config;
                uint32_t     size_to_copy = (data_len < sizeof(sfc_config_t)) ? data_len : sizeof(sfc_config_t);

                memset(&sfc_config, 0U, sizeof(sfc_config_t));
                memcpy(&sfc_config, (sfc_config_t *)&data[1], size_to_copy);
                SFC_UpdateConfig((const sfc_config_t *)&sfc_config);
            }
            break;
            case gFwkSrvFroEnableNotification_c:
                SFC_EnableNotification(data[1]);
                break;
            case gFwkSrvRngReseed_c:
                RNG_SetExternalSeed(&data[1]);
                break;
            default:
                break;
        }
    }
    return res;
}

#if defined(gPlatformIcsNbuDeferredNbuApi2Idle_d) && (gPlatformIcsNbuDeferredNbuApi2Idle_d == 1)
static void PLATFORM_FwkSrvSendNbuApiIndicationLater(void *msg, uint16_t msg_lg)
{
    platformLastNbuApiIndicationSize = msg_lg;
    memcpy(&platformNbuApiIndication, msg, platformLastNbuApiIndicationSize);
    platformNbuApiIndicationPending++;
}
#endif

static bool FwkSrv_MsgTypeInExpectedSet(uint8_t msg_type)
{
    return (msg_type > gFwkSrvHost2NbuFirst_c && msg_type < gFwkSrvHost2NbuLast_c);
}
