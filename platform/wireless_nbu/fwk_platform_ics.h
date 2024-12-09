/*
 * Copyright 2021, 2024 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWK_PLATFORM_ICS_H__
#define __FWK_PLATFORM_ICS_H__

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "EmbeddedTypes.h"

/* -------------------------------------------------------------------------- */
/*                                Public macros                               */
/* -------------------------------------------------------------------------- */

#define MAX_TAG_SZ        40
#define MAX_SHA_SZ        20
#define MAX_VARIANT_SZ    25
#define MAX_BUILD_TYPE_SZ 10

/* maximum size for API return parameters including status in bytes.
   should be same as the one defined in the NBU core */
#define NBU_API_MAX_RETURN_PARAM_LENGTH 39U

/* -------------------------------------------------------------------------- */
/*                           Public type definitions                          */
/* -------------------------------------------------------------------------- */

typedef PACKED_STRUCT
{
    uint8_t versionNumber[3];
    uint8_t repo_digest[MAX_SHA_SZ];
    char    repo_tag[MAX_TAG_SZ];
    char    variant[MAX_VARIANT_SZ];
    char    build_type[MAX_BUILD_TYPE_SZ];
    uint8_t versionBuildNo;
}
NbuInfo_t;

typedef PACKED_STRUCT NbuDbgMemInfo_tag
{
    uint8_t  poolId;
    uint8_t  reserved;
    uint16_t bufferSize;
}
NbuDbgMemInfo_t;

typedef enum
{
    gFwkSrvNbu2HostFirst_c                      = 0U,
    gFwkSrvNbuInitDone_c                        = 0x1U,
    gFwkSrvNbuVersionIndication_c               = 0x2U,
    gFwkSrvNbuApiIndication_c                   = 0x3U,
    gFwkSrvNbuMemFullIndication_c               = 0x4U,
    gFwkSrvHostSetLowPowerConstraint_c          = 0x5U,
    gFwkSrvHostReleaseLowPowerConstraint_c      = 0x6U,
    gFwkSrvFroNotification_c                    = 0x7U,
    gFwkSrvNbuIssueIndication_c                 = 0x8U,
    gFwkSrvNbuSecurityEventIndication_c         = 0x9U,
    gFwkSrvNbuRequestRngSeed_c                  = 0xAU,
    gFwkSrvNbuRequestNewTemperature_c           = 0xBU,
    gFwkSrvNbu2HostLast_c                       = 0xCU,
    gFwkSrvHost2NbuFirst_c                      = 0x80U,
    gFwkSrvNbuVersionRequest_c                  = 0x81U,
    gFwkSrvXtal32MTrimIndication_c              = 0x82U,
    gFwkSrvNbuApiRequest_c                      = 0x83U,
    gFwkTemperatureIndication_c                 = 0x84U, /*!< Receive Temperature value */
    gFwkSrvHostChipRevision_c                   = 0x85U, /*!< Receive Chip Revision value */
    gFwkSrvNbuSecureModeRequest_c               = 0x86U,
    gFwkSrvNbuWakeupDelayLpoCycle_c             = 0x87U, /*!< BLE timer wakeup delay in number of 3.2kHz period */
    gFwkSrvNbuUpdateFrequencyConstraintFromHost = 0x88U,
    gFwkSrvNbuSetRfSfcConfig_c                  = 0x89U,
    gFwkSrvFroEnableNotification_c              = 0x8AU,
    gFwkSrvRngReseed_c                          = 0x8BU,
    gFwkSrvHost2NbuLast_c                       = 0x8CU,
} eFwkSrvMsgType;

/* -------------------------------------------------------------------------- */
/*                         Public memory declarations                         */
/* -------------------------------------------------------------------------- */

extern const NbuInfo_t nbu_version;

/* -------------------------------------------------------------------------- */
/*                              Public prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Initialization of the PLATFORM Service channel
 *
 * \return int 0 if success, 1 if already initialized, negative value if error.
 */
int PLATFORM_FwkSrvInit(void);

/*!
 * \brief Send NBU version indication to other processor
 *
 * \return int 0 if success, -1 if no memory available, -2 if sending error.
 */
int PLATFORM_SendNbuVersionIndication(void);

/*!
 * \brief Send init done notification to other processor
 *
 * \return int 0 if success, -1 if no memory available, -2 if sending error.
 */
int PLATFORM_NotifyNbuInitDone(void);

/*!
 * \brief Send a SetLowPowerConstraint command to the host core
 *        Useful to disallow the host core to enter power gated modes so the NBU can access XBAR bus
 *
 * \param[in] mode low power mode ID, see fwk_platform_lowpower.h of host core for details
 * \return int int 0 if success, -1 if no memory available, -2 if sending error.
 */
int PLATFORM_FwkSrvSetLowPowerConstraint(uint8_t mode);

/*!
 * \brief Send a ReleaseLowPowerConstraint command to the host core
 *        Useful when the NBU doesn't need access to XBAR bus, so the host core can re-enter power gated modes
 *
 * \param[in] mode low power mode ID, see fwk_platform_lowpower.h of host core for details
 * \return int int 0 if success, -1 if no memory available, -2 if sending error.
 */
int PLATFORM_FwkSrvReleaseLowPowerConstraint(uint8_t mode);

/*!
 * \brief Free a buffer that has been previously allocated by a remote call to
 *        HAL_RpmsgAllocTxBuffer
 *
 * \param[in] pPacket Pointer to the memory to free
 */
void PLATFORM_FwkSrvFreeRxPacket(void *pPacket);

/*!
 * \brief Send to host SFC info for debug purpose
 *
 * \param[in] freq
 * \param[in] ppm_mean
 * \param[in] ppm
 */
int PLATFORM_FwkSrvFroInfo(uint16_t freq, int16_t ppm_mean, int16_t ppm, uint16_t fro_trim);

/*!
 * \brief Request new Seed for PRNG
 *
 * \return int 0 if success, -1 if no memory available, -2 if sending error.
 */
int PLATFORM_RequestRngSeedToHost(void);

/*!
 * \brief Request New temperature to Host
 *
 * \param[in] periodic_interval_ms
 * \return int 0 if success, -1 if no memory available, -2 if sending error.
 */
int PLATFORM_FwkSrvRequestNewTemperature(uint32_t periodic_interval_ms);

#if defined(gPlatformIcsNbuDeferredNbuApi2Idle_d) && (gPlatformIcsNbuDeferredNbuApi2Idle_d == 1)
/*!
 * \brief Send pending nbu api indication message that has been delayed by PLATFORM_FwkSrvSendNbuApiIndicationLater()
 *
 *\details Called from Idle to avoid calling it from ISR
 */
void PLATFORM_FwkSrvCheckAndSendNbuApiIndicationInIdle(void);
#endif

#endif /* __FWK_PLATFORM_ICS_H__ */
