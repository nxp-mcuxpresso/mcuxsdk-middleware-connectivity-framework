/* -------------------------------------------------------------------------- */
/*                      Copyright 2021-2023, 2025-2026 NXP                    */
/*                    SPDX-License-Identifier: BSD-3-Clause                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

/* Get BOARD_LL_32MHz_WAKEUP_ADVANCE_HSLOT if defined in board.h for 32MHz settings
 * BOARD_FRO32K_PPM_TARGET and BOARD_FRO32K_FILTER_SIZE for fro32k calibration settings */
#include "board_platform.h"

#include "fwk_hal_macros.h"
#include "fwk_config.h"
#include "fwk_platform_ble.h"
#include "fwk_platform_ics.h"
#include "FunctionLib.h"
#include "RNG_Interface.h"
#include "fwk_debug.h"
#include "controller_api.h"

#if defined(gPlatformUseHwParameter_d) && (gPlatformUseHwParameter_d > 0)
#include "HWParameter.h"
#endif

#if defined(gPlatformUseUniqueDeviceIdForBdAddr_d) && (gPlatformUseUniqueDeviceIdForBdAddr_d > 0)
#include "fwk_platform_definitions.h"
#include "fwk_platform.h"
#endif

#if defined(BOARD_FRO32K_PPM_TARGET) || defined(BOARD_FRO32K_FILTER_SIZE) || \
    defined(BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS) || defined(BOARD_FRO32K_TRIG_SAMPLE_NUMBER)
#include "fwk_sfc.h"
#endif

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */
#define PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE 3U
#define PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE  3U
#define PLATFORM_BLE_BD_ADDR_FULL_SIZE      6U

#define mBoardUidSize_c 16

#ifndef BD_ADDR_OUI
#define BD_ADDR_OUI 0x37U, 0x60U, 0x00U
#endif

/* 30 bit max value : timestamp is originally a 32bit integer counting quarter microseconds,
 * thence the loss of 2 bits when converted to microseconds.
 */
#define PLATFORM_BLE_TIMESTAMP_MAX 0x3fffffffUL        /* 30 bit max value : 0x3fffffffUL */
#define PLATFORM_BLE_SLOT_MAX      0x7fffffffUL        /* 31 bit max value :  */
#define PLATFORM_BLE_SLOT_USEC     625U                /* BLE slot is 625 usec */
#define PLATFORM_TSTMR_MASK        0xFFFFFFFFFFFFFFULL /* 56 bit max value */

/*!
 * \brief Default calibration settings for the FRO32K, can be overriden in board.h with BOARD_FRO32K_PPM_TARGET
 *        and BOARD_FRO32K_FILTER_SIZE
 */
#define PLATFORM_DEFAULT_FRO32K_PPM_TARGET                  200U
#define PLATFORM_DEFAULT_FRO32K_FILTER_SIZE                 128U
#define PLATFORM_DEFAULT_FRO32K_MAX_CALIBRATION_INTERVAL_MS 1000U
#define PLATFORM_DEFAULT_FRO32K_TRIG_SAMPLE_NUMBER          3U

/* -------------------------------------------------------------------------- */
/*                             Private prototypes                             */
/* -------------------------------------------------------------------------- */

/*!
 * \brief Generate new BD address
 *
 * \param[in] Provide pointer to buffer location when the BD address should be stored
 *
 */
STATIC void PLATFORM_GenerateNewBDAddr(uint8_t *bleDeviceAddress);

#ifdef BOARD_LL_32MHz_WAKEUP_ADVANCE_HSLOT
/*!
 * \brief Send to the NBU the value of BOARD_LL_32MHz_WAKEUP_ADVANCE_HSLOT to be setted on its side
 *
 */
STATIC int PLATFORM_SendWakeupDelay(uint8_t wakeupDelayToBeSendToNbu);
#endif

/*!
 * \brief Compute time elapsed since ll_ts (timestamp from Rx PDU descriptor) and time retrieved by core#0
 *         by means of Controller_GetTimestampEx API
 * \param[in]  ll_ts initial timestamp value (usec unit)
 * \param[in]  ll_slot_cnt BLE slot counter (625usec) value returned by Controller_GetTimestampEx
 * \param[in]  ll_slot_offset_usec microsecond offset in slot returned by Controller_GetTimestampEx.
 *             must be in the range [0..624].
 * \return number of microseconds elapsed between ll_ts and moment when Controller_GetTimestampEx was queried.
 */
STATIC uint32_t PLATFORM_ComputeTimeDiffFromBleSlotAndSlotOffset(uint32_t ll_ts,
                                                                 uint32_t ll_slot_cnt,
                                                                 uint16_t ll_slot_offset_usec);

/*!
 * \brief Compute TSTMR ticks elapsed between call to Controller_GetTimestampEx and now.
 *
 * \param[in]  tstmr0 timestamp value returned by NBU when it treated the request from Controller_GetTimestampEx.
 *
 * \return number of microseconds elapsed between tstmr0 and now.
 */
STATIC uint32_t PLATFORM_ComputeTimeDiffNbu2HostTstmr(uint64_t tstmr0);

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */
STATIC const uint8_t gBD_ADDR_OUI_c[PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE] = {BD_ADDR_OUI};

#if defined(BOARD_FRO32K_PPM_TARGET) || defined(BOARD_FRO32K_FILTER_SIZE) || \
    defined(BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS) || defined(BOARD_FRO32K_TRIG_SAMPLE_NUMBER)
static const sfc_config_t sfcConfig = {
#ifdef BOARD_FRO32K_PPM_TARGET
    .ppmTarget = BOARD_FRO32K_PPM_TARGET,
#else
    .ppmTarget                = PLATFORM_DEFAULT_FRO32K_PPM_TARGET,
#endif /* BOARD_FRO32K_PPM_TARGET */
#ifdef BOARD_FRO32K_FILTER_SIZE
    .filterSize = BOARD_FRO32K_FILTER_SIZE,
#else
    .filterSize               = PLATFORM_DEFAULT_FRO32K_FILTER_SIZE,
#endif /* BOARD_FRO32K_FILTER_SIZE */
#ifdef BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS
    .maxCalibrationIntervalMs = BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS,
#else
    .maxCalibrationIntervalMs = PLATFORM_DEFAULT_FRO32K_MAX_CALIBRATION_INTERVAL_MS,
#endif /* BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS */
#ifdef BOARD_FRO32K_TRIG_SAMPLE_NUMBER
    .trigSampleNumber = BOARD_FRO32K_TRIG_SAMPLE_NUMBER
#else
    .trigSampleNumber         = PLATFORM_DEFAULT_FRO32K_TRIG_SAMPLE_NUMBER,
#endif /* BOARD_FRO32K_TRIG_SAMPLE_NUMBER */
};
#endif /* BOARD_FRO32K_PPM_TARGET || BOARD_FRO32K_FILTER_SIZE ||  BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS || \
          BOARD_FRO32K_TRIG_SAMPLE_NUMBER */

/* -------------------------------------------------------------------------- */
/*                              Public functions                              */
/* -------------------------------------------------------------------------- */

/*!
 * \brief retrieve BLE device address
 *
 * \param[out] bleDeviceAddress pointer to BLE device address bytes
 *
 */
void PLATFORM_GetBDAddr(uint8_t *bleDeviceAddress)
{
#if defined(gPlatformUseHwParameter_d) && (gPlatformUseHwParameter_d > 0)
    hardwareParameters_t *pHWParams = NULL;
    uint32_t              status;

    status = NV_ReadHWParameters(&pHWParams);

    /* FLib_MemCmpToVal mandatory to make sure BLE mac address is valid
     * because return status of NV_ReadHWParameters is 1 only at 1st read attempt */
    if ((status == gHWParameterSuccess_c) &&
        (FLib_MemCmpToVal((const void *)pHWParams->bluetooth_address, 0xFFU, PLATFORM_BLE_BD_ADDR_FULL_SIZE) == FALSE))
    {
        uint32_t regPrimask;

        regPrimask = DisableGlobalIRQ();
        FLib_MemCpy((void *)bleDeviceAddress, (const void *)pHWParams->bluetooth_address,
                    PLATFORM_BLE_BD_ADDR_FULL_SIZE);
        EnableGlobalIRQ(regPrimask);
    }
    else
    {
        uint32_t regPrimask;

        /* User can decide to use the device unique address or a random generated address with
         * gPlatformUseUniqueDeviceIdForBdAddr_d */
        PLATFORM_GenerateNewBDAddr(bleDeviceAddress);

        regPrimask = DisableGlobalIRQ();
        FLib_MemCpy((void *)pHWParams->bluetooth_address, (void *)bleDeviceAddress, PLATFORM_BLE_BD_ADDR_FULL_SIZE);

        (void)NV_WriteHWParameters();
        EnableGlobalIRQ(regPrimask);
    }
#else
    static uint8_t bdAddr[PLATFORM_BLE_BD_ADDR_FULL_SIZE] = {0U};
    if (FLib_MemCmpToVal((const void *)bdAddr, 0x0U, PLATFORM_BLE_BD_ADDR_FULL_SIZE) == TRUE)
    {
        PLATFORM_GenerateNewBDAddr(bdAddr);
    }
    FLib_MemCpy((void *)bleDeviceAddress, (void *)bdAddr, PLATFORM_BLE_BD_ADDR_FULL_SIZE);
#endif
}

int32_t PLATFORM_EnableBleSecureKeyManagement(void)
{
    int32_t ret = 0;
    /* Send intercore gFwkSrvNbuSecureModeRequest_c message to NBU*/
    ret = PLATFORM_FwkSrvSendPacket(gFwkSrvNbuSecureModeRequest_c, NULL, 0U);

    return ret;
}

uint64_t PLATFORM_GetDeltaTimeStamp(uint32_t controllerTimestamp)
{
    uint64_t delta = 0ULL;

    do
    {
        uint64_t tstmr0         = 0ULL; /* TSTMR timestamp at which NBU returned HSLOT and quarter microsec */
        uint32_t ll_timing_slot = 0UL;  /* NBU HSLOT counter converted to count of slots */
        uint16_t ll_timing_us   = 0U;
        uint32_t tstmr_delta_us;
        uint32_t ll_time_diff;

        /* coverity[assume] controllerTimestamp <= PLATFORM_BLE_TIMESTAMP_MAX */
        /* controllerTimestamp is guaranteed to be < 2^30 by design */
        if (controllerTimestamp > PLATFORM_BLE_TIMESTAMP_MAX)
        {
            break;
        }
        if (Controller_GetTimestampEx(&ll_timing_slot, &ll_timing_us, &tstmr0) != KOSA_StatusSuccess)
        {
            break;
        }
        /* Sanitize returned timestamp value, although not really useful since overflow would happen after 2284 years */
        tstmr0 &= PLATFORM_TSTMR_MASK;

        /* Controller_GetTimestampEx returns slot counter after conversion from half-slot, so a number smaller than
         * 2^31. Likewise number of microseconds is bounded by 625.
         * In order to have an arithmetic compatible with NBU, convert usec back to quarter usec and slots to half-slots.
         */
        ll_time_diff =
            PLATFORM_ComputeTimeDiffFromBleSlotAndSlotOffset(controllerTimestamp, ll_timing_slot, ll_timing_us);
        if (ll_time_diff == UINT32_MAX)
        {
            delta = 0ULL;
            break;
        }

        /* Time differences are only ever computed over relatively small numbers that fit in a 32 bit variable
         */
        tstmr_delta_us = PLATFORM_ComputeTimeDiffNbu2HostTstmr(tstmr0);
        if (tstmr_delta_us == UINT32_MAX)
        {
            delta = 0ULL;
            break;
        }
        delta = (uint64_t)tstmr_delta_us + (uint64_t)ll_time_diff;
    } while (false);

    /* if delta time difference is 0 it points out an error */
    return delta;
}

int PLATFORM_InitWakeUpDelay(void)
{
    int ret = 0;

#ifdef BOARD_LL_32MHz_WAKEUP_ADVANCE_HSLOT
    ret = PLATFORM_SendWakeupDelay(BOARD_LL_32MHz_WAKEUP_ADVANCE_HSLOT);
#endif

    return ret;
}

int PLATFORM_InitSfc(void)
{
#if defined(BOARD_FRO32K_PPM_TARGET) || defined(BOARD_FRO32K_FILTER_SIZE) || \
    defined(BOARD_FRO32K_MAX_CALIBRATION_INTERVAL_MS) || defined(BOARD_FRO32K_TRIG_SAMPLE_NUMBER)
    PLATFORM_FwkSrvSetRfSfcConfig((void *)&sfcConfig, (uint16_t)sizeof(sfc_config_t));
#endif
    return 0;
}

/*
 * Deprecated API.
 */
bool PLATFORM_CheckNextBleConnectivityActivity(void)
{
    /* Verify whether NBU core#1 has raised a fault report */
    DBG_LOG_DUMP();
    /* core#1 unconstrained by core#0 as far as flash modifications are concerned, so return true */
    return true;
}

/* -------------------------------------------------------------------------- */
/*                              Private functions                             */
/* -------------------------------------------------------------------------- */

STATIC void PLATFORM_GenerateNewBDAddr(uint8_t *bleDeviceAddress)
{
    uint8_t macAddr[PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE] = {0U};

#if defined(gPlatformUseUniqueDeviceIdForBdAddr_d) && (gPlatformUseUniqueDeviceIdForBdAddr_d == 1)
    /* First need to activate the radio clock */
    PLATFORM_RemoteActiveReq();
    uint32_t uid_lsb = RADIO_CTRL->UID_LSB;
    PLATFORM_RemoteActiveRel();

    /* The UID is read out as 0 if clock is uninitalized.
     * Even when initialized UID may have been left as 0xffffffff */
    if ((uid_lsb != 0U) && (uid_lsb != UINT32_MAX))
    {
        for (int i = 0; i < PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE; i++)
        {
            macAddr[i] = (uint8_t)uid_lsb & 0xffU;
            uid_lsb >>= 8;
        }
        /* Set 3 LSB from mac address */
        FLib_MemCpy((void *)bleDeviceAddress, (const void *)macAddr, PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE);

        /* Set 3 MSB from OUI */
        FLib_MemCpy((void *)&bleDeviceAddress[PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE], (const void *)gBD_ADDR_OUI_c,
                    PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE);
    }
    else
#elif defined(gPlatformUseUniqueDeviceIdForBdAddr_d) && (gPlatformUseUniqueDeviceIdForBdAddr_d == 2)
    if (FLib_MemCmpToVal((const void *)(uint32_t *)IFR_BLE_BD_ADDR, 0xFFU, PLATFORM_BLE_BD_ADDR_FULL_SIZE) == false)
    {
        const uint8_t *ifr_bd_addr = (const uint8_t *)IFR_BLE_BD_ADDR;
        /* Copy BLE BD address from dedicated IFR0 section */
        /* Equivalent of FLib_MemCpyReverseOrder but no guarantee that bleDeviceAddress is 32 bit aligned */
        for (uint32_t i = 0U; i < PLATFORM_BLE_BD_ADDR_FULL_SIZE; i++)
        {
            bleDeviceAddress[i] = ifr_bd_addr[PLATFORM_BLE_BD_ADDR_FULL_SIZE - 1U - i];
        }
    }
    else
#endif
    {
        int ret;

        ret = RNG_Init();
        assert(ret == 0);
        (void)ret;

#ifndef FWK_RNG_DEPRECATED_API
        ret = RNG_GetPseudoRandomData(macAddr, PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE, NULL);
#else
        RNG_SetPseudoRandomNoSeed(NULL);
        ret = RNG_GetPseudoRandomNo(macAddr, PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE, NULL);
#endif
        assert(ret == (int32_t)PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE);
        (void)ret;

        /* Set 3 LSB from mac address */
        FLib_MemCpy((void *)bleDeviceAddress, (const void *)macAddr, PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE);

        /* Set 3 MSB from OUI */
        FLib_MemCpy((void *)&bleDeviceAddress[PLATFORM_BLE_BD_ADDR_RAND_PART_SIZE], (const void *)gBD_ADDR_OUI_c,
                    PLATFORM_BLE_BD_ADDR_OUI_PART_SIZE);
    }
}

#ifdef BOARD_LL_32MHz_WAKEUP_ADVANCE_HSLOT
STATIC int PLATFORM_SendWakeupDelay(uint8_t wakeupDelayToBeSendToNbu)
{
    return PLATFORM_FwkSrvSendPacket(gFwkSrvNbuWakeupDelayLpoCycle_c, &wakeupDelayToBeSendToNbu,
                                     (uint16_t)sizeof(wakeupDelayToBeSendToNbu));
}
#endif

/*!
 * \brief Compute TSTMR ticks elapsed between call to Controller_GetTimestampEx and now.
 *
 * \param[in]  tstmr0 timestamp value returned by NBU when it treated the request from Controller_GetTimestampEx.
 *
 * \return number of microseconds elapsed between tstmr0 and now.
 */
STATIC uint32_t PLATFORM_ComputeTimeDiffNbu2HostTstmr(uint64_t tstmr0)
{
    uint64_t tstmr_delta_us, now;

    /* Get current TSTMR value */
    now = PLATFORM_GetTimeStamp();
    /* Compute difference with timestamp reported by NBU when queried by Controller_GetTimestampEx */
    tstmr_delta_us = PLATFORM_GetTimeStampDeltaUs(tstmr0, now);

    /* Disregard large values because between the time reported by the call to Controller_GetTimestampEx
     * and the moment it gets treated, only a small delay can have elapsed. */
    if (tstmr_delta_us > UINT32_MAX)
    {
        tstmr_delta_us = UINT32_MAX;
    }

    tstmr_delta_us &= 0xffffffffULL;
    return (uint32_t)tstmr_delta_us;
}

/*!
 * \brief Compute time elapsed since ll_ts (timestamp from Rx PDU descriptor) and time retrieved by core#0
 *         by means of Controller_GetTimestampEx API
 * \param[in]  ll_ts initial timestamp value (usec unit)
 * \param[in]  ll_slot_cnt BLE slot counter (625usec) value returned by Controller_GetTimestampEx
 * \param[in]  ll_slot_offset_usec microsecond offset in slot returned by Controller_GetTimestampEx.
 *             must be in the range [0..624].
 * \return number of microseconds elapsed between ll_ts and moment when Controller_GetTimestampEx was queried.
 */
STATIC uint32_t PLATFORM_ComputeTimeDiffFromBleSlotAndSlotOffset(uint32_t ll_ts,
                                                                 uint32_t ll_slot_cnt,
                                                                 uint16_t ll_slot_offset_usec)
{
    /* Controller_GetTimestampEx returns slot counter after conversion from half-slot, so a number smaller than
     * 2^31. Likewise number of microseconds is bounded by 625.
     * In order to have an arithmetic compatible with NBU, convert usec back to quarter usec and slots to half-slots.
     */
    uint64_t tmp;
    uint32_t time_diff;
    uint32_t curr_ll_ts;
    uint32_t ll_timing_qus;

    /* Sanitize all parameters */
    if ((ll_slot_offset_usec >= PLATFORM_BLE_SLOT_USEC) || (ll_slot_cnt > PLATFORM_BLE_SLOT_MAX) ||
        (ll_ts > PLATFORM_BLE_TIMESTAMP_MAX))
    {
        time_diff = ~0UL; /* Return max value to indicate error */
    }
    else
    {
        /* coverity[assume] ll_slot_cnt <= PLATFORM_BLE_SLOT_MAX  */
        ll_slot_cnt &= PLATFORM_BLE_SLOT_MAX;

        /* Convert offset to quarter microseconds */
        ll_timing_qus = 4U * (uint32_t)ll_slot_offset_usec;

        /* Compute total time in quarter microseconds */
        tmp = (uint64_t)ll_slot_cnt * 2500ULL; /* 625us * 4 */
        tmp += (uint64_t)(ll_timing_qus);
        /* tmp number of quarter microseconds is in the range [0..0x4e200000000], which fits in 41 bits */
        /* Convert back to usec */
        tmp >>= 2;
        /* mask out upper 34 MSB bits */
        tmp &= (uint64_t)PLATFORM_BLE_TIMESTAMP_MAX;

        curr_ll_ts = (uint32_t)tmp;

        /* Compute time difference, handling wraparound */
        if (ll_ts <= curr_ll_ts)
        {
            /* coverity[overflow:FALSE] */
            /* the condition (ll_ts <= curr_ll_ts) guarantees that this expression remains positive */
            time_diff = (curr_ll_ts - ll_ts);
        }
        else
        {
            /* wrap occurred */
            uint64_t extended_diff = (uint64_t)PLATFORM_BLE_TIMESTAMP_MAX + 1ULL + curr_ll_ts - ll_ts;
            time_diff              = (uint32_t)extended_diff;
        }
    }
    /* coverity[return_overflow:FALSE] */
    return time_diff;
}
