/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* -------------------------------------------------------------------------- */
/*                                  Includes                                  */
/* -------------------------------------------------------------------------- */

#include "FunctionLib.h"
#include "fwk_platform_ifr.h"
#include "fsl_adapter_flash.h"
#include "fsl_os_abstraction.h"
#include "fwk_platform_definitions.h"
#include "ifr0map_struct.h"
/* DEVMGR_BD_ADDR_SZ / DEVMGR_154_ADDR_SZ used by the CFPA MAC accessors. */
#include "fwk_platform_devmgr.h"

/* -------------------------------------------------------------------------- */
/*                               Private macros                               */
/* -------------------------------------------------------------------------- */

#define DEVMGR_SECTOR_SIZE FSL_FEATURE_FLASH_PFLASH_SECTOR_SIZE
#define DEVMGR_PHRASE_SIZE FSL_FEATURE_FLASH_PFLASH_PHRASE_SIZE

#if 0
/* DEV_UPD_TYPE_CFPA never actually used - always use DEV_UPD_TYPE_CMPA - commenting out  */
#define DEV_UPD_TYPE_CFPA 0x55504446UL
#endif
#define DEV_UPD_TYPE_CMPA 0x5550444DUL

#define CUST_DEFINED_WORD_COUNT 1280U
#define CUST_DEFINED_BYTE_SIZE  (CUST_DEFINED_WORD_COUNT * sizeof(uint32_t))

/* Internal sentinel: block forever on the mutex */
#define DEVMGR_WAIT_FOREVER 0xFFFFFFFFU

#define CFOFF(f) offsetof(cfpa_zone_t, f)
#define CMOFF(f) offsetof(cmpa_zone_t, f)

/* -------------------------------------------------------------------------- */
/*                         Private type definitions                           */
/* -------------------------------------------------------------------------- */

/*!
 * @brief Union overlay - only one zone is loaded at a time.
 *
 * Static RAM cost = sizeof(cmpa_cust_defined_zone_t) ~ 5 KB (the largest member).
 */
typedef union
{
    cfpa_zone_t              cfpa;
    cmpa_zone_t              cmpa;
    cmpa_cust_defined_zone_t cmpa_cust;
} fbdc_zone_overlay_t;

/*! Runtime session state. */
typedef struct
{
    fbdc_zone_overlay_t overlay;
    fbdc_zone_id_t      zone_id;
    bool                is_open;
} fbdc_ctx_t;

/*! Compile-time zone descriptor. */
typedef struct
{
    uint32_t offset;
    uint32_t size;
    uint32_t upd_type;
} zone_desc_t;

/* -------------------------------------------------------------------------- */
/*                         Private memory declarations                        */
/* -------------------------------------------------------------------------- */

OSA_MUTEX_HANDLE_DEFINE(s_fbdcMutex);
static bool s_mutexCreated = false;
/*
 * Mutex ownership flag, kept distinct from s_ctx.is_open.
 *
 *   s_mutexHeld   : true while this module owns s_fbdcMutex (a transaction or a
 *                   scratch-sector erase is in progress).
 *   s_ctx.is_open : true only when a valid zone image is loaded in the RAM
 *                   overlay. Getters rely on this to decide whether to read the
 *                   overlay or the DEVCFG flash mirror.
 *
 * These two states are not equivalent: PLATFORM_DevMgrEraseSector() holds the
 * mutex without ever loading a zone into the overlay, so it must not set
 * is_open (otherwise a getter racing the erase window would read uninitialised
 * overlay data instead of the flash mirror).
 */
static bool       s_mutexHeld = false;
static fbdc_ctx_t s_ctx       = {.is_open = false};

static const zone_desc_t s_zoneDesc[] = {
    [kDevMgrZone_Cfpa] = {DEVMGR_CFPA_OFFSET, sizeof(cfpa_zone_t), DEV_UPD_TYPE_CMPA}, /* cannot be DEV_UPD_TYPE_CFPA*/
    [kDevMgrZone_Cmpa] = {DEVMGR_CMPA_OFFSET, sizeof(cmpa_zone_t), DEV_UPD_TYPE_CMPA},
    [kDevMgrZone_CmpaCustomer] = {DEVMGR_CMPA_CUST_DEFINED_OFFSET, sizeof(cmpa_cust_defined_zone_t), DEV_UPD_TYPE_CMPA},
};

#define ZONE_DESC_COUNT (sizeof(s_zoneDesc) / sizeof(s_zoneDesc[0]))

/* -------------------------------------------------------------------------- */
/*                              Private helpers                               */
/* -------------------------------------------------------------------------- */

static void s_EnsureMutexCreated(void)
{
    uint32_t regPrimask;

    regPrimask = DisableGlobalIRQ();
    if (!s_mutexCreated)
    {
        (void)OSA_MutexCreate(s_fbdcMutex);
        s_mutexCreated = true;
    }
    EnableGlobalIRQ(regPrimask);
}

static inline const void *s_DevcfgPtr(uint32_t offset)
{
    return (const void *)((uintptr_t)DEVMGR_IFR0_DEVCFG_BASE + (uintptr_t)offset);
}

static inline void *s_OverlayPtr(void)
{
    return (void *)&s_ctx.overlay;
}

/*!
 * @brief Atomically read a sub-range of a zone into a caller buffer.
 *
 * Selects the active data source - the RAM overlay when the matching zone is
 * open, otherwise the DEVCFG flash mirror - and copies @p size bytes at
 * @p foff. The source selection and the copy are performed inside a single
 * critical section so the read cannot race a concurrent
 * PLATFORM_DevMgrCommitZone()/PLATFORM_DevMgrAbortZone() clearing s_ctx.is_open
 * (or a subsequent PLATFORM_DevMgrOpenZone() reloading the overlay with a
 * different zone). A short interrupt-masked section is used rather than the
 * flash mutex, because getters are legitimately called by the same thread that
 * already holds an open zone and the mutex is not recursive.
 *
 * The critical section spans the copy, so callers reading a whole multi-KB zone
 * keep interrupts masked for the duration of that copy. These full-zone reads
 * are rare provisioning-time operations, so the latency cost is acceptable.
 */
static void s_ReadZone(fbdc_zone_id_t id, uint32_t foff, void *dst, size_t size)
{
    const uint8_t *src;
    OSA_SR_ALLOC();

    OSA_ENTER_CRITICAL();
    if (s_ctx.is_open && (s_ctx.zone_id == id))
    {
        src = (const uint8_t *)s_OverlayPtr();
    }
    else
    {
        src = (const uint8_t *)s_DevcfgPtr(s_zoneDesc[id].offset);
    }
    FLib_MemCpy(dst, src + foff, size);
    OSA_EXIT_CRITICAL();
}

/*!
 * Program and ECC-verify one phrase in the DEVMGR scratch sector.
 */
static int s_ProgPhrase(uint32_t off, const uint8_t *data)
{
    hal_flash_status_t st;
    uint8_t            rb[DEVMGR_PHRASE_SIZE];
    int                ret;

    do
    {
        st = HAL_FlashProgram(DEVMGR_IFR0_SCRATCH_BASE + off, DEVMGR_PHRASE_SIZE, (uint8_t *)data);
        if (st != kStatus_HAL_Flash_Success)
        {
            ret = DEVMGR_STATUS_ERR_FLASH;
            break;
        }

        st = HAL_FlashReadCheckEccFaults(DEVMGR_IFR0_SCRATCH_BASE + off, DEVMGR_PHRASE_SIZE, rb);
        if (st != kStatus_HAL_Flash_Success)
        {
            ret = DEVMGR_STATUS_ERR_VERIFY;
            break;
        }

        if (FLib_MemCmp(data, rb, DEVMGR_PHRASE_SIZE) == FALSE)
        {
            ret = DEVMGR_STATUS_ERR_VERIFY;
            break;
        }

        ret = DEVMGR_STATUS_OK;
    } while (false);

    return ret;
}

/*!
 * @brief Build one phrase buffer for the DEVMGR scratch sector.
 *
 * Copies the DEVCFG read-only mirror at @p phrase_off as the baseline, then
 * overlays any bytes of the modified zone [zone_off, zone_off + zone_sz) that
 * fall inside this phrase. Shared by both passes of s_CommitToFlash().
 */
static void s_BuildPhraseBuf(
    uint32_t phrase_off, uint32_t zone_off, uint32_t zone_sz, const void *zone_data, uint8_t *buf)
{
    const uint32_t pe       = phrase_off + DEVMGR_PHRASE_SIZE;
    const uint32_t zone_end = zone_off + zone_sz;

    /* Baseline from DEVCFG mirror */
    FLib_MemCpy(buf, (const uint8_t *)(uintptr_t)(DEVMGR_IFR0_DEVCFG_BASE + phrase_off), DEVMGR_PHRASE_SIZE);

    /* Overlay modified zone bytes that fall in this phrase */
    if ((phrase_off < zone_end) && (pe > zone_off))
    {
        const uint32_t cs = (phrase_off > zone_off) ? phrase_off : zone_off;
        const uint32_t ce = (pe < zone_end) ? pe : zone_end;
        FLib_MemCpy(buf + (cs - phrase_off), (const uint8_t *)zone_data + (cs - zone_off), ce - cs);
    }
}

/*!
 * @brief Erase the DEVMGR scratch sector and reprogram phrase-by-phrase.
 *
 * For every phrase:
 *   baseline  = DEVCFG mirror
 *   overlay A = modified zone bytes (from zone_data)
 *   overlay B = UPDATE zone bytes  (from upd_zone, UPDATE phrase only)
 *
 * The UPDATE zone phrase is written LAST so the ROM only sees the trigger
 * after all data it refers to has been verified.
 */
static int s_CommitToFlash(const update_zone_t *upd_zone, uint32_t zone_off, const void *zone_data, uint32_t zone_sz)
{
    hal_flash_status_t hst;
    uint32_t           po;
    int                st;

    /* Phrase that contains the UPDATE zone (DEVMGR_UPD_OFFSET == 0 on KW43). */
    const uint32_t upd_ph  = (DEVMGR_UPD_OFFSET / DEVMGR_PHRASE_SIZE) * DEVMGR_PHRASE_SIZE;
    const uint32_t upd_pe  = upd_ph + DEVMGR_PHRASE_SIZE;
    const uint32_t upd_end = DEVMGR_UPD_OFFSET + (uint32_t)sizeof(update_zone_t);

    do
    {
        hst = HAL_FlashInit();
        if (hst != kStatus_HAL_Flash_Success)
        {
            st = DEVMGR_STATUS_ERR_FLASH;
            break;
        }

        hst = HAL_FlashEraseSector(DEVMGR_IFR0_SCRATCH_BASE, DEVMGR_SECTOR_SIZE);
        if (hst != kStatus_HAL_Flash_Success)
        {
            st = DEVMGR_STATUS_ERR_FLASH;
            break;
        }

        /* ---- Pass 1: all phrases except the UPDATE zone phrase ---- */
        st = DEVMGR_STATUS_OK;

        for (po = 0U; po < (DEVMGR_SECTOR_SIZE - DEVMGR_NFPA_SIZE); po += DEVMGR_PHRASE_SIZE)
        {
            uint8_t buf[DEVMGR_PHRASE_SIZE];

            if (po == upd_ph)
            {
                continue;
            } /* defer */

            s_BuildPhraseBuf(po, zone_off, zone_sz, zone_data, buf);

            st = s_ProgPhrase(po, buf);
            if (st != DEVMGR_STATUS_OK)
            {
                break;
            }
        }
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }

        /* ---- Pass 2: program UPDATE zone phrase last ---- */
        {
            uint8_t  buf[DEVMGR_PHRASE_SIZE];
            uint32_t cs;
            uint32_t ce;

            /* Baseline + any modified zone bytes that share this phrase. On the
             * KW43 map the UPDATE phrase (offset 0) and the data zones never
             * co-occupy a phrase, so this overlay is a no-op there; it is kept
             * for correctness on any future layout. */
            s_BuildPhraseBuf(upd_ph, zone_off, zone_sz, zone_data, buf);

            /* Overlay the UPDATE zone trigger bytes last. */
            cs = (upd_ph > DEVMGR_UPD_OFFSET) ? upd_ph : (uint32_t)DEVMGR_UPD_OFFSET;
            ce = (upd_pe < upd_end) ? upd_pe : upd_end;
            FLib_MemCpy(buf + (cs - upd_ph), (const uint8_t *)upd_zone + (cs - DEVMGR_UPD_OFFSET), ce - cs);

            st = s_ProgPhrase(upd_ph, buf);
        }
    } while (false);

    return st;
}

/*!
 * If the named zone is open: write field to overlay only.
 * If not open: open -> write -> commit.
 */
static int s_SetField(fbdc_zone_id_t id, uint32_t foff, const void *pval, size_t size)
{
    int st;

    do
    {
        /* Zone is already opened by caller, just update overlay */
        if (s_ctx.is_open && (s_ctx.zone_id == id))
        {
            FLib_MemCpy((uint8_t *)s_OverlayPtr() + foff, pval, size);
            st = DEVMGR_STATUS_OK;
            break;
        }
        /* Was not opened yet, so do it now and commit */
        st = PLATFORM_DevMgrOpenZone(id, osaWaitForever_c);
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }
        FLib_MemCpy((uint8_t *)s_OverlayPtr() + foff, pval, size);
        st = PLATFORM_DevMgrCommitZone();
    } while (false);

    return st;
}

static int s_SetField32(fbdc_zone_id_t id, uint32_t foff, uint32_t val)
{
    int st;

    do
    {
        /* Zone is already opened by caller, just update overlay */
        if (s_ctx.is_open && (s_ctx.zone_id == id))
        {
            FLib_MemCpy((uint8_t *)s_OverlayPtr() + foff, &val, sizeof(uint32_t));
            st = DEVMGR_STATUS_OK;
            break;
        }
        /* Was not opened yet, so do it now and commit */
        st = PLATFORM_DevMgrOpenZone(id, DEVMGR_WAIT_FOREVER);
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }
        FLib_MemCpy((uint8_t *)s_OverlayPtr() + foff, &val, sizeof(uint32_t));
        st = PLATFORM_DevMgrCommitZone();
    } while (false);

    return st;
}

/*!
 * Read a 32-bit field from overlay (if zone open) or flash mirror. The
 * source-selection + copy is atomic (see s_ReadZone).
 */
static uint32_t s_GetField32(fbdc_zone_id_t id, uint32_t foff)
{
    uint32_t val;

    s_ReadZone(id, foff, &val, sizeof(uint32_t));
    return val;
}

/*!
 * Read a field from overlay (if zone open) or flash mirror. The
 * source-selection + copy is atomic (see s_ReadZone).
 */
static void s_GetField(fbdc_zone_id_t id, uint32_t foff, void *pval, size_t size)
{
    s_ReadZone(id, foff, pval, size);
}

/* ========================================================================== */
/*                    Transactional open / commit / abort                     */
/* ========================================================================== */

int PLATFORM_DevMgrOpenZone(fbdc_zone_id_t zone_id, uint32_t timeout_ms)
{
    osa_status_t osa_st;
    uint32_t     osa_to;
    int          st;

    do
    {
        if ((uint32_t)zone_id >= ZONE_DESC_COUNT)
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }

        /* If the requested zone is already open, just return success */
        if (s_ctx.is_open && (s_ctx.zone_id == zone_id))
        {
            st = DEVMGR_STATUS_OK;
            break;
        }

        /* If a different zone is open, return error */
        if (s_ctx.is_open && (s_ctx.zone_id != zone_id))
        {
            st = DEVMGR_STATUS_ERR_STATE;
            break;
        }

        s_EnsureMutexCreated();

        osa_to = (timeout_ms == osaWaitForever_c) ? osaWaitForever_c : timeout_ms;
        osa_st = OSA_MutexLock(s_fbdcMutex, osa_to);
        if (osa_st != KOSA_StatusSuccess)
        {
            st = DEVMGR_STATUS_ERR_BUSY;
            break;
        }

        s_mutexHeld = true;

        /* Read from DEVCFG mirror into overlay buffer, then mark the overlay
         * valid. is_open is set only after the overlay has been populated. */
        FLib_MemCpy(s_OverlayPtr(), s_DevcfgPtr(s_zoneDesc[zone_id].offset), s_zoneDesc[zone_id].size);
        s_ctx.zone_id = zone_id;
        s_ctx.is_open = true;
        st            = DEVMGR_STATUS_OK;
    } while (false);

    return st;
}

int PLATFORM_DevMgrCommitZone(void)
{
    int           st;
    update_zone_t upd;

    do
    {
        if (!s_ctx.is_open)
        {
            st = DEVMGR_STATUS_ERR_STATE;
            break;
        }

        FLib_MemSet(&upd, 0, sizeof(upd));
        upd.upd_type = DEV_UPD_TYPE_CMPA; /* even if CFPA update DEV_UPD_TYPE_CFPA cannot work if CMPA is not still
                                             blank so always combine both  */

        st = s_CommitToFlash(&upd, s_zoneDesc[s_ctx.zone_id].offset, s_OverlayPtr(), s_zoneDesc[s_ctx.zone_id].size);

        s_ctx.is_open = false;
        s_mutexHeld   = false;
        (void)OSA_MutexUnlock(s_fbdcMutex);
        if (st == DEVMGR_STATUS_OK)
        {
            /* Force reboot to let DEVCFG update happen */
            NVIC_SystemReset();
        }
    } while (false);

    return st;
}

void PLATFORM_DevMgrAbortZone(void)
{
    if (s_ctx.is_open)
    {
        s_ctx.is_open = false;
        s_mutexHeld   = false;
        (void)OSA_MutexUnlock(s_fbdcMutex);
    }
}

int PLATFORM_DevMgrEraseSector(void)
{
    osa_status_t       osa_st;
    hal_flash_status_t hst;
    int                st;

    do
    {
        s_EnsureMutexCreated();

        /* Reject if a transaction is in progress or the mutex is otherwise
         * held: an erase must own the scratch sector exclusively. */
        if (s_ctx.is_open || s_mutexHeld)
        {
            st = DEVMGR_STATUS_ERR_STATE;
            break;
        }

        osa_st = OSA_MutexLock(s_fbdcMutex, osaWaitForever_c);
        if (osa_st != KOSA_StatusSuccess)
        {
            st = DEVMGR_STATUS_ERR_BUSY;
            break;
        }

        /* Take ownership only. No zone is loaded into the overlay here, so
         * is_open stays false: getters racing this erase window must fall back
         * to the DEVCFG flash mirror, not read stale overlay data. */
        s_mutexHeld = true;

        hst = HAL_FlashInit();
        if (hst != kStatus_HAL_Flash_Success)
        {
            st = DEVMGR_STATUS_ERR_FLASH;
            break;
        }

        hst = HAL_FlashEraseSector(DEVMGR_IFR0_SCRATCH_BASE, DEVMGR_SECTOR_SIZE);
        if (hst != kStatus_HAL_Flash_Success)
        {
            st = DEVMGR_STATUS_ERR_FLASH;
            break;
        }
        st = DEVMGR_STATUS_OK;
    } while (false);

    if (s_mutexHeld)
    {
        s_mutexHeld = false;
        (void)OSA_MutexUnlock(s_fbdcMutex);
    }

    return st;
}

/* ========================================================================== */
/*                             CFPA implementation                            */
/* ========================================================================== */

int PLATFORM_DevMgrGetCfpaZone(cfpa_zone_t *dst)
{
    int st;

    do
    {
        if (dst == NULL)
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        s_ReadZone(kDevMgrZone_Cfpa, 0U, dst, sizeof(cfpa_zone_t));
        st = DEVMGR_STATUS_OK;

    } while (false);

    return st;
}

int PLATFORM_DevMgrSetCfpaZone(const cfpa_zone_t *src)
{
    int st;

    do
    {
        if (src == NULL)
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        st = PLATFORM_DevMgrOpenZone(kDevMgrZone_Cfpa, osaWaitForever_c);
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }
        FLib_MemCpy(&s_ctx.overlay.cfpa, src, sizeof(cfpa_zone_t));
        st = PLATFORM_DevMgrCommitZone();
    } while (false);

    return st;
}

uint32_t PLATFORM_DevMgrGetCfpaHeader(void)
{
    return s_GetField32(kDevMgrZone_Cfpa, CFOFF(header));
}

int PLATFORM_DevMgrGetCfpaCustBleMacAddr(uint8_t *buf, uint8_t len)
{
    int st;
    do
    {
        if ((buf == NULL) || (len != DEVMGR_BD_ADDR_SZ))
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        s_GetField(kDevMgrZone_Cfpa, CFOFF(cust_ble_mac_addr), buf, len);
        st = DEVMGR_STATUS_OK;
    } while (false);

    return st;
}

int PLATFORM_DevMgrGetCfpaCust802154MacAddr(uint8_t *buf, uint8_t len)
{
    int st;
    do
    {
        if ((buf == NULL) || (len != DEVMGR_154_ADDR_SZ))
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        s_GetField(kDevMgrZone_Cfpa, CFOFF(cust_802_15_4_mac_addr), buf, len);
        st = DEVMGR_STATUS_OK;
    } while (false);

    return st;
}

uint8_t PLATFORM_DevMgrGetCfpaXtal32kHzCfg(void)
{
    uint32_t tmp;
    tmp = s_GetField32(kDevMgrZone_Cfpa, CFOFF(xtal_32kHz_cfg));
    return (uint8_t)(tmp & 0xffU);
}

uint8_t PLATFORM_DevMgrGetCfpaXtal32MHzCfg(void)
{
    uint32_t tmp;
    tmp = s_GetField32(kDevMgrZone_Cfpa, CFOFF(xtal_32MHz_cfg));
    return (uint8_t)(tmp & 0xffU);
}

int PLATFORM_DevMgrSetCfpaCustBleMacAddr(uint8_t *buf, uint8_t len)
{
    int st = DEVMGR_STATUS_ERR_PARAM;
    if ((buf != NULL) && (len == DEVMGR_BD_ADDR_SZ))
    {
        st = s_SetField(kDevMgrZone_Cfpa, CFOFF(cust_ble_mac_addr), buf, len);
    }
    return st;
}

int PLATFORM_DevMgrSetCfpaCust802154MacAddr(uint8_t *buf, uint8_t len)
{
    int st = DEVMGR_STATUS_ERR_PARAM;
    if ((buf != NULL) && (len == DEVMGR_154_ADDR_SZ))
    {
        st = s_SetField(kDevMgrZone_Cfpa, CFOFF(cust_802_15_4_mac_addr), buf, len);
    }
    return st;
}

int PLATFORM_DevMgrSetCfpaXtal32kHzCfg(uint8_t val)
{
    uint32_t tmp = (uint32_t)val;
    return s_SetField32(kDevMgrZone_Cfpa, CFOFF(xtal_32kHz_cfg), tmp);
}

int PLATFORM_DevMgrSetCfpaXtal32MHzCfg(uint8_t val)
{
    uint32_t tmp = (uint32_t)val;
    return s_SetField32(kDevMgrZone_Cfpa, CFOFF(xtal_32MHz_cfg), tmp);
}

/* ========================================================================== */
/*                             CMPA implementation                            */
/* ========================================================================== */

int PLATFORM_DevMgrGetCmpaZone(cmpa_zone_t *dst)
{
    int st = DEVMGR_STATUS_ERR_PARAM;
    if (dst != NULL)
    {
        s_ReadZone(kDevMgrZone_Cmpa, 0U, dst, sizeof(cmpa_zone_t));
        st = DEVMGR_STATUS_OK;
    }
    return st;
}

int PLATFORM_DevMgrSetCmpaZone(const cmpa_zone_t *src)
{
    int st;
    do
    {
        if (src == NULL)
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        st = PLATFORM_DevMgrOpenZone(kDevMgrZone_Cmpa, DEVMGR_WAIT_FOREVER);
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }
        FLib_MemCpy(&s_ctx.overlay.cmpa, src, sizeof(cmpa_zone_t));
        st = PLATFORM_DevMgrCommitZone();
    } while (false);
    return st;
}

uint32_t PLATFORM_DevMgrGetCmpaNbuRecoveryImgAddr(void)
{
    return s_GetField32(kDevMgrZone_Cmpa, CMOFF(nbu_recovery_img_addr));
}
uint32_t PLATFORM_DevMgrGetCmpaNbuUpdateImgAddr(void)
{
    return s_GetField32(kDevMgrZone_Cmpa, CMOFF(nbu_update_img_addr));
}
uint32_t PLATFORM_DevMgrGetCmpaCpu0RecoveryImgAddr(void)
{
    return s_GetField32(kDevMgrZone_Cmpa, CMOFF(cpu0_recovery_img_addr));
}
uint32_t PLATFORM_DevMgrGetCmpaCpu0UpdateImgAddr(void)
{
    return s_GetField32(kDevMgrZone_Cmpa, CMOFF(cpu0_update_img_addr));
}

int PLATFORM_DevMgrSetCmpaNbuRecoveryImgAddr(uint32_t val)
{
    return s_SetField32(kDevMgrZone_Cmpa, CMOFF(nbu_recovery_img_addr), val);
}
int PLATFORM_DevMgrSetCmpaNbuUpdateImgAddr(uint32_t val)
{
    return s_SetField32(kDevMgrZone_Cmpa, CMOFF(nbu_update_img_addr), val);
}
int PLATFORM_DevMgrSetCmpaCpu0RecoveryImgAddr(uint32_t val)
{
    return s_SetField32(kDevMgrZone_Cmpa, CMOFF(cpu0_recovery_img_addr), val);
}
int PLATFORM_DevMgrSetCmpaCpu0UpdateImgAddr(uint32_t val)
{
    return s_SetField32(kDevMgrZone_Cmpa, CMOFF(cpu0_update_img_addr), val);
}

/* ========================================================================== */
/*               CMPA Customer Defined zone implementation                    */
/* ========================================================================== */

int PLATFORM_DevMgrGetCmpaCustomerDefinedZone(cmpa_cust_defined_zone_t *dst)
{
    int st = DEVMGR_STATUS_ERR_PARAM;
    if (dst != NULL)
    {
        s_ReadZone(kDevMgrZone_CmpaCustomer, 0U, dst, sizeof(cmpa_cust_defined_zone_t));
        st = DEVMGR_STATUS_OK;
    }
    return st;
}

int PLATFORM_DevMgrSetCmpaCustomerDefinedZone(const cmpa_cust_defined_zone_t *src)
{
    int st;
    do
    {
        if (src == NULL)
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        st = PLATFORM_DevMgrOpenZone(kDevMgrZone_CmpaCustomer, DEVMGR_WAIT_FOREVER);
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }
        FLib_MemCpy(&s_ctx.overlay.cmpa_cust, src, sizeof(cmpa_cust_defined_zone_t));
        st = PLATFORM_DevMgrCommitZone();
    } while (false);
    return st;
}

int PLATFORM_DevMgrGetCmpaCustomerDefinedWord(uint16_t idx, uint32_t *val)
{
    int st = DEVMGR_STATUS_ERR_PARAM;

    if ((idx < CUST_DEFINED_WORD_COUNT) && (val != NULL))
    {
        *val = s_GetField32(kDevMgrZone_CmpaCustomer,
                            offsetof(cmpa_cust_defined_zone_t, customer_defined[0]) + (uint32_t)idx * sizeof(uint32_t));
        st   = DEVMGR_STATUS_OK;
    }
    return st;
}

int PLATFORM_DevMgrSetCmpaCustomerDefinedWord(uint16_t idx, uint32_t val)
{
    int st = DEVMGR_STATUS_ERR_PARAM;

    if (idx < CUST_DEFINED_WORD_COUNT)
    {
        st = s_SetField32(kDevMgrZone_CmpaCustomer,
                          offsetof(cmpa_cust_defined_zone_t, customer_defined[0]) + (uint32_t)idx * sizeof(uint32_t),
                          val);
    }
    return st;
}

int PLATFORM_DevMgrGetCmpaCustomerDefinedBytes(uint16_t byte_offset, uint8_t *buf, uint16_t len)
{
    int st = DEVMGR_STATUS_ERR_PARAM;

    if ((buf != NULL) && (len != 0U) && ((uint32_t)byte_offset + (uint32_t)len <= CUST_DEFINED_BYTE_SIZE))
    {
        /* byte_offset is relative to the zone start, not the sector start */
        s_ReadZone(kDevMgrZone_CmpaCustomer, byte_offset, buf, len);
        st = DEVMGR_STATUS_OK;
    }

    return st;
}

int PLATFORM_DevMgrSetCmpaCustomerDefinedBytes(uint16_t byte_offset, const uint8_t *buf, uint16_t len)
{
    int st;
    do
    {
        if ((buf == NULL) || (len == 0U) || ((uint32_t)byte_offset + (uint32_t)len > CUST_DEFINED_BYTE_SIZE))
        {
            st = DEVMGR_STATUS_ERR_PARAM;
            break;
        }
        if (s_ctx.is_open && (s_ctx.zone_id == kDevMgrZone_CmpaCustomer))
        {
            /* Zone already open: update overlay only */
            FLib_MemCpy((uint8_t *)s_OverlayPtr() + byte_offset, buf, len);
            st = DEVMGR_STATUS_OK;
            break;
        }
        st = PLATFORM_DevMgrOpenZone(kDevMgrZone_CmpaCustomer, DEVMGR_WAIT_FOREVER);
        if (st != DEVMGR_STATUS_OK)
        {
            break;
        }
        FLib_MemCpy((uint8_t *)s_OverlayPtr() + byte_offset, buf, len);
        st = PLATFORM_DevMgrCommitZone();
    } while (false);
    return st;
}
