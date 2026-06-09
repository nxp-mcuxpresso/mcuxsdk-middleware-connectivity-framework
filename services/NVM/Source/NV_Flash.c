/*
 * Copyright 2015 Freescale
 * Copyright 2016-2017, 2019-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "EmbeddedTypes.h"
#include "NV_Flash.h"
#include "fsl_adapter_flash.h"
#include "FunctionLib.h"
#include "fsl_os_abstraction.h"

#if defined gNvDebugEnabled_d && (gNvDebugEnabled_d > 0)
#include <stdio.h> /* required for sprintf */
#include "fsl_debug_console.h"
#endif

#if gUnmirroredFeatureSet_d
#include "fsl_component_mem_manager.h"
#endif

#if ((defined(gNvmEnableFSCIMonitoring_c)) && (gNvmEnableFSCIMonitoring_c > 0U))
#if !((defined(gFsciIncluded_c)) && (gFsciIncluded_c > 0U))
#error "NVM FSCI monitoring requires gFsciIncluded_c to be set to TRUE"
#endif
#endif

#if (gNvmEnableFSCIRequests_c || gNvmEnableFSCIMonitoring_c)
#include "NV_FsciCommands.h"
#endif

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerJitter_c)
#include "RNG_Interface.h"
#endif
#include "fsl_component_timer_manager.h"
#endif

#if (gNvmEnableFSCIMonitoring_c)
#define FSCI_NV_VIRT_PAGE_ERASE_MONITOR(_cond_, _status_) FSCI_MsgNVPageEraseMonitoring(_cond_, (uint8_t)_status_)
#define FSCI_NV_WRITE_MONITOR(_id__, _elt_idx_, _all_)    FSCI_MsgNVWriteMonitoring(_id__, _elt_idx_, _all_)
#define FSCI_NV_RESTORE_MONITOR(_id_, _bstart_, _status_) FSCI_MsgNVRestoreMonitoring(_id_, _bstart_, (uint8_t)_status_)
#define FSCI_NV_VIRT_PAGE_MONITOR(_bstart_, _status_)     FSCI_MsgNVVirtualPageMonitoring(_bstart_, (uint8_t)_status_)
#else
#define FSCI_NV_VIRT_PAGE_ERASE_MONITOR(_cond_, _status_)
#define FSCI_NV_WRITE_MONITOR(_id__, _elt_idx_, _all_)
#define FSCI_NV_RESTORE_MONITOR(_id_, _bstart_, _status_)
#define FSCI_NV_VIRT_PAGE_MONITOR(_bstart_, _status_)
#endif

/*****************************************************************************
 *****************************************************************************
 * Private macros
 *****************************************************************************
 *****************************************************************************/
#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerJitter_c)
#define GET_RND_NB(W) (void)RNG_GetPseudoRandomData((uint8_t *)W, 4u, NULL)
#endif

#if gNvStorageIncluded_d

/*
 * Name: gNvVirtualPagesCount_c
 * Description: the count of virtual pages used
 */
#define gNvVirtualPagesCount_c 2U /* DO NOT MODIFY */

/*
 * Name: gNvGuardValue_c
 * Description: self explanatory
 */
#define gNvGuardValue_c 0xFFFFFFFFFFFFFFFFuL

/*
 * Name: gNvFirstMetaOffset_c
 * Description: the offset of the first meta
 */
#if gNvUseExtendedFeatureSet_d
#define gNvFirstMetaOffset_c (sizeof(NVM_TableInfo_t) + (uint32_t)mNvTableSizeInFlash + sizeof(NVM_TableInfo_t))
#else
#define gNvFirstMetaOffset_c (sizeof(NVM_TableInfo_t))
#endif

/*
 * Name: gNvErasedFlashCellValue_c
 * Description: self explanatory
 */
#define gNvErasedFlashCellValue_c 0xFFU

#endif /* gNvStorageIncluded_d */

#if defined(FWK_UNIT_TEST)
#define NVM_STATIC
#define NVM_PUBLIC
#else
#if (!defined(GCOV_DO_COVERAGE) || (GCOV_DO_COVERAGE == 0))
#define NVM_STATIC static
#define NVM_PUBLIC
#else
#define NVM_STATIC __WEAK
#define NVM_PUBLIC __WEAK
#endif
#endif

/*
 * Increment index modulo gNvPendingSavesQueueSize_c
 */
#define INCREMENT_Q_INDEX(x)                               \
    {                                                      \
        if (++(x) == (uint16_t)gNvPendingSavesQueueSize_c) \
        {                                                  \
            (x) = 0U;                                      \
        }                                                  \
    }

#define IS_OFFSET_32BIT_ALIGNED(x) (((x) & ((uint16_t)(sizeof(uint32_t) - 1U))) == 0U)

/*
 * Return gFirstVirtualPage_c(0) if x==1 (gSecondVirtualPage_c) and vice-versa.
 */
#define OTHER_PAGE_ID(x) (NVM_VirtualPageID_t)(uint8_t)(((uint8_t)(x) ^ 1U) & 1U)

#define INT_FLASH_PHRASE_SZ_LOG2 4U /* Internal Flash phrase is 16 bytes */

#define ROUND_FLOOR(_X_, _SHIFT_) ((((uint32_t)_X_) >> (_SHIFT_)) << (_SHIFT_))

#if defined(__GNUC__)
#ifndef gNvmErasePartitionWhenFlashing_c
#define gNvmErasePartitionWhenFlashing_c 0U
#endif /* gNvmErasePartitionWhenFlashing_c */
#endif /*  defined(__GNUC__) */
/*****************************************************************************
 *****************************************************************************
 * Private type definitions
 *****************************************************************************
 *****************************************************************************/
typedef union NVM_TableAndEntryInfo_tag
{
    NVM_TableInfo_t tableInfo;
    NVM_EntryInfo_t entryInfo;
} NVM_TableAndEntryInfo_t;

/*****************************************************************************
 *****************************************************************************
 * Private prototypes
 *****************************************************************************
 *****************************************************************************/
#if gNvStorageIncluded_d
#if gNvUseExtendedFeatureSet_d && gNvTableKeptInRam_d
/******************************************************************************
 * Name: __NvRegisterTableEntry
 * Description: The function tries to register a new table entry within an
 *              existing NV table. If the NV table contained an erased (invalid)
 *              entry, the entry will be overwritten with a new one (provided
 *              by the mean of this function arguments)
 * Parameter(s): [IN] ptrData - generic pointer to RAM data to be registered
 *                              within the NV storage system
 *               [IN] uniqueId - an unique ID of the table entry
 *               [IN] elemCount - how many elements the table entry contains
 *               [IN] elemSize - the size of an element
 *               [IN] overwrite - if an existing table entry shall be
 *                                overwritten
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvRegisterTableEntry(void            *ptrData,
                                               NvTableEntryId_t uniqueId,
                                               uint16_t         elemCount,
                                               uint16_t         elemSize,
                                               uint16_t         dataEntryType,
                                               bool_t           overwrite);

/******************************************************************************
 * Name: __NvEraseEntryFromStorage
 * Description: The function removes a table entry within the existing NV
 *              table. The RAM table must be updated previously.
 * Parameter(s): [IN] entryId - the entry id of the entry that is removed
 *               [IN] tableEntryIndex - the index of the entry in the ram table
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvEraseEntryFromStorage(uint16_t entryId, uint16_t tableEntryIndex);

#endif /* gNvUseExtendedFeatureSet_d && gNvTableKeptInRam_d */

/******************************************************************************
 * Name: InitNVMConfig
 * Description: Initialises the hal driver, and gets the active page.
 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void InitNVMConfig(void);

/******************************************************************************
 * Name: __NvAtomicSave
 * Description: The function performs an atomic save of the entire NV table
 *              to the storage system. The operation is performed
 *              in place (atomic).
 * Parameter(s): -
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *         gNVM_PointerOutOfRange_c - if the pointer is out of range
 *         gNVM_InvalidTableEntry_c - if the table entry is not valid
 *         gNVM_MetaInfoWriteError_c - meta tag couldn't be written
 *         gNVM_RecordWriteError_c - record couldn't be written
 *         gNVM_CriticalSectionActive_c - the module is in critical section
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvAtomicSave(void);

/******************************************************************************
 * Name: __NvSyncSave
 * Description: The function saves the pointed element or the entire table
 *              entry to the storage system. The save operation is not
 *              performed on the idle task but within this function call.
 * Parameter(s): [IN] ptrData - a pointer to data to be saved
 *               [IN] saveAll - specifies if the entire table entry shall be
 *                              saved or only the pointed element
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *         gNVM_PointerOutOfRange_c - if the pointer is out of range
 *         gNVM_InvalidTableEntry_c - if the table entry is not valid
 *         gNVM_MetaInfoWriteError_c - meta tag couldn't be written
 *         gNVM_RecordWriteError_c - record couldn't be written
 *         gNVM_CriticalSectionActive_c - the module is in critical section
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvSyncSave(void *ptrData, bool_t saveAll);

/******************************************************************************
 * Name: __NvFormat
 * Description: Format the NV storage system. The function erases both virtual
 *              pages and then writes the page counter to active page.
 * Parameter(s): -
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_FormatFailure_c - if the format operation fails
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_CriticalSectionActive_c - if the system has entered in a
 *                                        critical section
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvFormat(void);
/******************************************************************************
 * Name: __NvIdle
 * Description: Called from the idle task (bare-metal) or NVM_Task (MQX,
 *              FreeRTOS) to process the pending saves, erase or copy
 *              operations.
 * Parameters: -
 * Return: Number of operations executed.
 ******************************************************************************/
NVM_STATIC int __NvIdle(void);
/******************************************************************************
 * Name: __NvIsDataSetDirty
 * Description: return TRUE if the element pointed by ptrData is dirty
 * Parameters: [IN] ptrData - pointer to data to be checked
 * Return: TRUE if the element is dirty, FALSE otherwise
 ******************************************************************************/
bool_t __NvIsDataSetDirty(void *ptrData);
/******************************************************************************
 * Name: __NvRestoreDataSet
 * Description: copy the most recent version of the element/table entry pointed
 *              by ptrData from NVM storage system to RAM memory
 * Parameter(s): [IN] ptrData - pointer to data (element) to be restored
 *               [IN] restoreAll - if FALSE restores a single element
 *                               - if TRUE restores an entire table entry
 * Return: status of the restore operation
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvRestoreDataSet(void *ptrData, bool_t restoreAll);

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
/******************************************************************************
 * Name: __NvTimerTick
 * Description: Called from the idle task to process save-on-interval requests
 * Parameters: [IN] countTick - enable/disable tick count
 * Return: FALSE if the timer tick counters for all data sets have reached
 *         zero. In this case, the timer can be turned off.
 *         TRUE if any of the data sets' timer tick counters have not yet
 *         counted down to zero. In this case, the timer should be active
 ******************************************************************************/
NVM_STATIC bool_t __NvTimerTick(bool_t countTick);
#endif

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnIdleCount_c)
/******************************************************************************
 * Name: __NvSaveOnCount
 * Description: Decrement the counter. Once it reaches 0, the next call to
 *              NvIdle() will save the entire table entry (all elements).
 * Parameters: [IN] ptrData - pointer to data to be saved
 * Return: NVM_OK_c - if operation completed successfully
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_STATIC NVM_Status_t __NvSaveOnCount(void *ptrData);
#endif

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
/******************************************************************************
 * Name: __NvSaveOnInterval
 * Description:  save no more often than a given time interval. If it has
 *               been at least that long since the last save,
 *               this function will cause a save the next time the idle
 *               task runs.
 * Parameters: [IN] ptrData - pointer to data to be saved
 * NOTE: this function saves all the element of the table entry pointed by
 *       ptrData
 * Return: NVM_OK_c - if operation completed successfully
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_STATIC NVM_Status_t __NvSaveOnInterval(void *ptrData);
#endif

/******************************************************************************
 * Name: __NvSaveOnIdle
 * Description: Save the data pointed by ptrData on the next call to NvIdle()
 * Parameter(s): [IN] ptrData - pointer to data to be saved
 *               [IN] saveAll - specify if all the elements from the NVM table
 *                              entry shall be saved
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_Error_c - in case of error(s)
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_STATIC NVM_Status_t __NvSaveOnIdle(void *ptrData, bool_t saveAll);

/******************************************************************************
 * Name: __NvModuleInit
 * Description: Initialize the NV storage module
 * Parameter(s): -
 * Return: gNVM_ModuleAlreadyInitialized_c - if the module is already
 *                                           initialized
 *         gNVM_InvalidSectorsCount_c - if the sector count configured in the
 *                                      project linker file is invalid
 *         gNVM_MetaNotFound_c - if no meta information was found
 *         gNVM_OK_c - module was successfully initialized
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvModuleInit(bool_t flashInit);

#if gUnmirroredFeatureSet_d

/******************************************************************************
 * Name: __NvmMoveToRam
 * Description: Move from NVM to Ram
 * Parameter(s):  ppData     double pointer to the entity to be moved from flash to RAM
 * Return: pointer to Ram location
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvmMoveToRam(void **ppData);

/******************************************************************************
 * Name: __NvmErase
 * Description: Erase from NVM an unmirrored dataset
 * Parameter(s):  ppData     double pointer to the entity to be erased
 * Return: pointer to Ram location
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvmErase(void **ppData);

/******************************************************************************
 * Name: NvIsNVMFlashAddress
 * Description: check if the address is in Flash
 * Parameter(s): [IN] address
 *
 * Return: TRUE if the table entry is in Flash / FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvIsNVMFlashAddress(void *address);

/******************************************************************************
 * Name: __NvmRestoreUnmirrored
 * Description: Restores all unmirrored entries with gNVM_NotMirroredInRamAutoRestore_c at init
 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void __NvmRestoreUnmirrored(void);
#endif

/******************************************************************************
 * Name: NvInitPendingSavesQueue
 * Description: Initialize the pending saves queue
 * Parameters: none
 * Return: none
 ******************************************************************************/
NVM_STATIC void NvInitPendingSavesQueue(void);

/******************************************************************************
 * Name: NvPushPendingSave
 * Description: Add a new pending save to the queue tail
 * Parameters: [IN] data - data to be saved
 * Return: TRUE if the push operation succeeded, FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvPushPendingSave(NVM_TableEntryInfo_t data);

/******************************************************************************
 * Name: NvGetPendingSaveHead
 * Description: Retrieves the head element from the pending saves queue leaving=
 *              it at head position. Also see NvPopPendingSave.
 * Parameters: [OUT] pData - pointer to the location where data will be placed
 * Return: TRUE if the get head operation succeeded, FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvGetPendingSaveHead(NVM_TableEntryInfo_t *pData);

/******************************************************************************
 * Name: NvRemovePendingSaveHead
 * Description: Consume pending save queue head by incrementing its head index.
 *              Also see NvPopPendingSave.
 * Parameters: none
 * Return: none
 ******************************************************************************/
NVM_STATIC void NvRemovePendingSaveHead(void);

/******************************************************************************
 * Name: NvLookAheadInPendingSaveQueue
 * Description: Search through pending save queue if an update is pending on the
 *              element designated by an id and index.
 *
 * Parameters: [IN] searched_id
 *             [IN] searched_index
 *
 * Return: op_type of pending operation OP_NONE if not found,
 *         OP_SAVE_ALL or OP_SAVE_SINGLE if found
 ******************************************************************************/
NVM_STATIC uint8_t NvLookAheadInPendingSaveQueue(uint16_t searched_id, uint16_t searched_index);

/******************************************************************************
 * Name: NvPopPendingSave
 * Description: Retrieves the head element from the pending saves queue
 * Parameters: [OUT] pData - pointer to the location where data will be placed
 * Return: TRUE if the pop operation succeeded, FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvPopPendingSave(NVM_TableEntryInfo_t *pData);

/******************************************************************************
 * Name: NvGetPendingSavesCount
 * Description: self explanatory
 * Parameters: none
 * Return: Number of pending saves
 ******************************************************************************/
NVM_STATIC uint16_t NvGetPendingSavesCount(void);

/******************************************************************************
 * Name: NvUpdateSize
 * Description: Updates the size to be a multiple of 4 or 8 depending on the flash controller
 * Parameter(s): [IN] size - size to be updated
 * Return: the computed size
 *****************************************************************************/
NVM_STATIC uint16_t NvUpdateSize(uint16_t size);

/******************************************************************************
 * Name: NvEraseVirtualPage
 * Description: erase the specified page
 * Parameter(s): [IN] pageID - the ID of the page to be erased
 * Return: gNVM_InvalidPageID_c - if the page ID is not valid
 *         gNVM_SectorEraseFail_c - if the page cannot be erased
 *         gNVM_OK_c - if operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvEraseVirtualPage(NVM_VirtualPageID_t pageID);

/******************************************************************************
 * Name: NvInitStorageSystem
 * Description: Initialize the storage system, retrieve the active page and
 *              the page counter. Called once by NvModuleInit() function.
 * Return: -
 *****************************************************************************/
NVM_STATIC void NvInitStorageSystem(void);

/******************************************************************************
 * Name: NvVirtualPageBlankCheck
 * Description: checks if the specified page is blank (erased)
 * Parameter(s): [IN] pageID - the ID of the page to be checked
 * Return: gNVM_PageIsNotBlank_c - if the page is not blank
 *         gNVM_OK_c - if the page is blank (erased)
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvVirtualPageBlankCheck(NVM_VirtualPageID_t pageID);

/******************************************************************************
 * Name: NvUpdateLastMetaInfoAddress
 * Description: retrieve and store (update) the last meta information address
 * Parameter(s): -
 * Return: gNVM_MetaNotFound_c - if no meta information has been found
 *         gNVM_OK_c - if the meta was found and stored (updated)
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvUpdateLastMetaInfoAddress(void);

/******************************************************************************
 * Name: NvGetMetaInfo
 * Description: get meta information based on the meta information address
 * Parameter(s): [IN] pageID - the ID of the page
 *               [IN] metaInfoOffset - meta information offset
 *               [OUT] pMetaInfo - a pointer to a memory location where the
 *                                 requested meta information will be stored
 * Return: gNVM_InvalidPageID_c - if the active page is not valid
 *         gNVM_AddressOutOfRange_c - if the provided address is out of range
 *         gNVM_MetaInfoInvalidError_c - MIT is invalid
 *         gNVM_MetaNotFound_c - contains guard value
 *         gNVM_EccFault_c - if ECC fault was raised when reading MIT
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetMetaInfo(NVM_VirtualPageID_t   pageId,
                                      uint32_t              metaInfoOffset,
                                      NVM_RecordMetaInfo_t *pMetaInfo);

/******************************************************************************
 * Name: NvGetPageFreeSpace
 * Description: return the page free space, in bytes
 * Parameter(s): [OUT] ptrFreeSpace - a pointer to a memory location where the
 *                                    page free space will be stored
 *               [IN] blank_check_req: if set to TRUE, a blank check will be performed over the assumed free space
 * Return: gNVM_InvalidPageID_c - if the active page is not valid
 *         gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PageIsEmpty_c - if the page is empty
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetPageFreeSpace(uint32_t *ptrFreeSpace, bool_t blank_check_req);

/******************************************************************************
 * Name: NvIsRecordCopied
 * Description: Checks if a record or an entire table entry is already copied.
 *              Called by page copy function.
 * Parameter(s): [IN] pageId - the ID of the page where to perform the check
 *               [IN] metaInf - a pointer to source page meta information tag
 * Return: TRUE if the element is already copied, FALSE otherwise
 *****************************************************************************/
NVM_STATIC bool_t NvIsRecordCopied(NVM_VirtualPageID_t pageId, NVM_RecordMetaInfo_t *metaInf);

/*!
 * \brief Multiply size of single element by number to figure out size or offset.
 *
 * The value is calculated as: val = (elt_index * elt_size), result must fit in a uint16_t short int
 *
 *
 * \param[in]  nb           number of elements
 * \param[in]  elt_size     Size in bytes of one element.
 * \param[in]  max_val      Maximum acceptable value.
 * \param[out]  pval        Receives the computed 16-bit value.
 *
 * \retval  0   Success; *pval contains the computed value.
 * \retval -1   Overflow detected; the result would exceed UINT16_MAX or mNvTotalPageSize.
 */
/* HIS_CALLING: NvMultEltSzByNb is a safety helper called by many functions by design. */
NVM_STATIC int NvMultEltSzByNb(uint16_t nb, uint16_t elt_size, uint32_t max_val, uint16_t *pval);

/*!
 * \brief Compute the flash offset of a single NVM dataset element.
 *
 * The offset is calculated as:
 *   elt_offset = (elt_index * elt_size) + inner_offset + rec_offset
 *
 * All intermediate additions are checked for 16-bit overflow. If any
 * addition would wrap beyond UINT16_MAX the function returns an error
 * without writing to *elt_offset.
 *
 * \param[in]  elt_index    Zero-based index of the element within the dataset.
 * \param[in]  elt_size     Size in bytes of one element. When the dataset is
 *                          being copied without fragmentation this may be 0,
 *                          in which case the index contribution is 0.
 * \param[in]  inner_offset Byte offset within the element (e.g. partial-write
 *                          start position).
 * \param[in]  rec_offset   Base offset of the NVM record in the page.
 * \param[out] elt_offset   Receives the computed 16-bit flash offset on
 *                          success. Not written on error.
 *
 * \retval  0   Success; *elt_offset contains the computed offset.
 * \retval -1   Overflow detected; the result would exceed UINT16_MAX.
 */
NVM_STATIC int NvComputeEltOffset(
    uint16_t elt_index, uint16_t elt_size, uint16_t inner_offset, uint16_t rec_offset, uint16_t *elt_offset);

/******************************************************************************
 * Name: NvInternalCopy
 * Description: Performs a copy of an record / entire table entry
 * Parameter(s): [IN] dstRecOffset - destination record address
 *               [IN] dstMetaOffset - destination meta address
 *               [IN] srcMetaInfo - source meta information
 *               [IN] srcTblEntryIdx - source table entry index
 *               [IN] size - bytes to copy
 * Return: gNVM_InvalidPageID_c - if the source or destination page is not
 *                                valid
 *         gNVM_MetaInfoWriteError_c - if the meta information couldn't be
 *                                     written
 *         gNVM_RecordWriteError_c - if the record couldn't be written
 *         gNVM_Error_c - in case of error(s)
 *         gNVM_OK_c - page copy completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalCopy(uint16_t              dstRecOffset,
                                       uint16_t              dstMetaOffset,
                                       NVM_RecordMetaInfo_t *srcMetaInfo,
                                       uint16_t              srcTblEntryIdx,
                                       uint16_t              size);

#if gNvFragmentation_Enabled_d
/******************************************************************************
 * Name: NvGetTblEntryMetaOffsetFromId
 * Description: Gets the table entry meta offset based on table entry ID
 * Parameter(s): [IN] searchOffset - the search start offset
 *               [IN] dataEntryId - table entry ID
 * Return: the value of the meta offset.
 *****************************************************************************/
NVM_STATIC uint16_t NvGetTblEntryMetaOffsetFromId(uint16_t searchOffset, uint16_t dataEntryId);

/******************************************************************************
 * Name: NvInternalDefragmentedCopy
 * Description: Performs defragmentation and copy from the source page to
 *              the destination one
 * Parameter(s): [IN] srcMetaOffset - source page meta address
 *               [IN] srcTblEntryIdx - source page table entry index
 *               [IN] dstMetaOffset - destination meta address
 *               [IN] dstRecordOffset - destination record address (to copy to)
 *               [IN] ownerRecordMetaInfo - pointer to the location of a full dataset save
 * Return: the status of the operation
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalDefragmentedCopy(uint32_t srcMetaOffset,
                                                   uint16_t srcTblEntryIdx,
                                                   uint32_t dstMetaOffset,
                                                   uint32_t dstRecordOffset,
                                                   uint16_t ownerRecordMetaInfoOffset);
#endif /* #if gNvFragmentation_Enabled_d */

#if gNvUseExtendedFeatureSet_d
NVM_STATIC void NvInitializeEntryInfo(NVM_EntryInfo_t *p_entry, uint8_t val);
#endif
NVM_STATIC bool_t NvRecordMetaInfoIsBlank(NVM_RecordMetaInfo_t *p_mit);

NVM_STATIC int NvAddOffsetToAddr(uint32_t base_addr, uint32_t offset, uint32_t *addr);

#if defined gNvDebugEnabled_d && (gNvDebugEnabled_d > 0)
/******************************************************************************
 * Name: NV_ShowPageMetas
 * Description: Dump NVM page entry table meta data.
 * Parameter(s): [IN] page_id - page whose meta data are to be dumped.
 *               [IN] ecc_checks TRUE if ECC detection is required
 * Return: -
 *****************************************************************************/
NVM_STATIC void NV_ShowPageMetas(NVM_VirtualPageID_t page_id, bool_t ecc_checks);
/******************************************************************************
 * Name: NV_ShowPageTableInfo
 * Description: Dump NVM page entry table meta data.
 * Parameter(s): [IN] page_id - page whose table info is to be dumped.
 *               [IN] ecc_checks TRUE if ECC detection is required
 * Return: -
 *****************************************************************************/
NVM_STATIC void NV_ShowPageTableInfo(NVM_VirtualPageID_t page_id, bool_t ecc_checks);
/******************************************************************************
 * Name: NvFlashDump
 * Description: Dump flash contents to the debug console.
 *              Contents are read with ECC check. If the dumped 16 byte area contains
 *              an ECC error, 'xx' are displayed the line preceding the fault.
 *              Otherwise the data are dumped in lines of 16 hex bytes, one per flash phrase.
 * Parameter(s): [IN] ptr - dump start pointer
 *               [IN] data_size number of byte to display
 * Return: -
 *****************************************************************************/
NVM_STATIC void NvFlashDump(uint8_t *ptr, uint16_t data_size);
#endif

/******************************************************************************
 * Name: NvCopyPage
 * Description: Copy the active page content to the mirror page. Only the
 *              latest table entries / elements are copied. A merge operation
 *              is performed before copy if an entry has single elements
 *              saved priori and newer than the table entry. If one or more
 *              elements were singular saved and the NV page doesn't has a
 *              full table entry saved, then the elements are copied as they
 *              are.
 * Parameter(s): [IN] skipEntryId - the entry ID to be skipped when page
 *                                  copy is performed
 * Return: gNVM_InvalidPageID_c - if the source or destination page is not
 *                                valid
 *         gNVM_MetaInfoWriteError_c - if the meta information couldn't be
 *                                     written
 *         gNVM_RecordWriteError_c - if the record couldn't be written
 *         gNVM_Error_c - in case of error(s)
 *         gNVM_OK_c - page copy completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvCopyPage(NvTableEntryId_t skipEntryId);

/******************************************************************************
 * Name: NvModuleSwitchPage
 * Description: Perform page copy operation to switch between NVM virtual pages.
 * Wraps the potential multiple attempts in case of ECC fault. Otherwise just calls NvCopyPage.
 *
 * Parameter(s): [IN] skipEntryId - the entry ID to be skipped when page
 *                                  copy is performed *
 * Return: gNVM_EccFault_c - if an ECC fault is raised during the page copy, need to try again
 *         gNVM_OK_c - page copy operation was successful.
 *
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvModuleSwitchPage(NvTableEntryId_t skipEntryId);

/******************************************************************************
 * Name: NvInternalFormat
 * Description: Format the NV storage system. The function erases in place both
 *              virtual pages and then writes the page counter value to first
 *              virtual page. The provided page counter value is automatically
 *              incremented and then written to first (active) virtual page.
 * Parameter(s): [IN] pageCounterValue - the page counter value that will
 *                                       be incremented and then written to
 *                                       active page
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_FormatFailure_c - if the format operation fails
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalFormat(uint32_t pageCounterValue);

/******************************************************************************
 * Name: NvSaveRamTable
 * Description: Saves the NV table
 * Parameter(s): [IN] pageId - the virtual page ID where the table will be
 *                             saved
 * Return: gNVM_OK_c if table saved successfully, other values otherwise
 *  ******************************************************************************/
NVM_STATIC NVM_Status_t NvSaveRamTable(NVM_VirtualPageID_t pageId);

/******************************************************************************
 * Name: NvSetMetaInfo
 * Description: Writes MIT fields and computes optional checksum
 * Parameter(s): [IN] metaInfo - pointer on MIT structure to be filled
 *               [IN] entryId - the entry ID
 *               [IN] eltIndex element index
 *               [IN] recordOffset - offset of the record data in the page - must be 32 bit aligned
 *               [IN] vb_val - validation byte value, gValidationByteSingleRecord_c or gValidationByteAllRecords_c
 * Return: -
 *  ******************************************************************************/
NVM_STATIC void NvSetMetaInfo(
    NVM_RecordMetaInfo_t *metaInfo, uint16_t entryId, uint16_t eltIndex, uint16_t recordOffset, uint8_t vb_val);

#if (defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d != 0))
/******************************************************************************
 * Name: NvCalculateChecksum
 * Description: Compute checksum over MIT fields up to padding field.
 *
 * Note: When setting the checksum, the checksum field itself must be set to 0xffffffffU.
 * A checksum is computed by XORing first 3 32-bit words in the MIT structure.
 * A valid value is expected to be 0xffffffffU after XORing with the checksum field.
 *
 * Parameter(s): [IN] metaInfo - pointer on MIT structure
 *
 * Return: checksum value on 32 bits
 *  ******************************************************************************/
NVM_STATIC uint32_t NvCalculateChecksum(NVM_RecordMetaInfo_t *metaInfo);
#endif

#if gNvUseExtendedFeatureSet_d

/******************************************************************************
 * Name: NvGetFlashTableSize
 * Description: Retrieves the size of the NV tableS
 * Parameter(s): -
 * Return: the NV table size
 ******************************************************************************/
NVM_STATIC uint16_t NvGetFlashTableSize(void);

/******************************************************************************
 * Name: NvIsRamTableUpdated
 * Description: Checks if the the NV table from RAM memory has changed since
 *              last system reset (e.g. via an OTA transfer)
 * Parameter(s): -
 * Return: TRUE if the NV RAM table has been changed / FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvIsRamTableUpdated(void);

/******************************************************************************
 * Name: NvGetTableEntry
 * Description: get the NV table entry information stored on FLASH memory
 * Parameter(s): [IN] tblEntryId - table entry ID
 *               [OUT] pDataEntry - a pointer to a memory location where the
 *                                  entry information will be stored
 * Return: TRUE if the has been found / FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvGetTableEntry(uint16_t tblEntryId, NVM_DataEntry_t *pDataEntry);
#endif /* gNvUseExtendedFeatureSet_d */

/******************************************************************************
 * Name: NvGetEntryFromDataPtr
 * Description: get table and element indexes based on a generic pointer address
 * Parameter(s): [IN] pData - a pointer to a NVM RAM table
 *               [OUT] pIndex - a pointer to a memory location where the
 *                              requested indexed will be stored
 * Return: gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PointerOutOfRange_c - if the provided pointer cannot be founded
 *                                    within the RAM table
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetEntryFromDataPtr(void *pData, NVM_TableEntryInfo_t *pIndex);

/******************************************************************************
 * Name: NvGetTableEntryIndexFromDataPtr
 * Description: get table and element indexes based on a generic pointer address
 * Parameter(s): [IN] pData - a pointer to a NVM RAM table
 *               [OUT] pIndex - a pointer to a memory location where the
 *                              requested indexed will be stored
 *               [OUT] pTableEntryIdx - a pointer to a memory location where the
 *                              requested TableEntry Idx will be stored
 * Return: gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PointerOutOfRange_c - if the provided pointer cannot be founded
 *                                    within the RAM table
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetTableEntryIndexFromDataPtr(void                 *pData,
                                                        NVM_TableEntryInfo_t *pIndex,
                                                        uint16_t             *pTableEntryIdx);
/******************************************************************************
 * Name: NvWriteRecord
 * Description: writes a record
 * Parameter(s): [IN] tblIndexes - a pointer to table and element indexes
 * Return: gNVM_InvalidPageID_c - if the active page is not valid
 *         gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_MetaInfoWriteError_c - if the meta information couldn't be
 *                                     written
 *         gNVM_RecordWriteError_c - if the record couldn't be written
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvWriteRecord(NVM_TableEntryInfo_t *tblIndexes);

/******************************************************************************
 * Name: NvRestoreData
 * Description: restore an element from NVM storage to its original RAM location
 * Parameter(s): [IN] tblIdx - pointer to table and element indexes
 * Return: gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PageIsEmpty_c - if page is empty
 *         gNVM_Error_c - in case of error(s)
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvRestoreData(NVM_TableEntryInfo_t *tblIdx);

/******************************************************************************
 * Name: NvGetTableEntryIndex
 * Description: get the table entry index from the provided ID
 * Parameter(s): [IN] entryId - the ID of the table entry
 * Return: table entry index of gNvInvalidTableEntryIndex_c
 *****************************************************************************/
NVM_STATIC uint16_t NvGetTableEntryIndexFromId(NvTableEntryId_t entryId);

/******************************************************************************
 * Name: NvAddSaveRequestToQueue
 * Description: Add save request to save requests queue; if the request is
 *              already stored, ignore the current request
 * Parameter(s): [IN] ptrTblIdx - pointer to table index
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_SaveRequestRejected_c - if the request couldn't be queued
 ******************************************************************************/
NVM_STATIC NVM_Status_t NvAddSaveRequestToQueue(NVM_TableEntryInfo_t *ptrTblIdx);

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerJitter_c)
/******************************************************************************
 * Name: GetRandomRange
 * Description: Returns a random number between 'low' and 'high'
 * Parameter(s): [IN] low, high - generated number range
 * Return: 0..255
 ******************************************************************************/
NVM_STATIC uint8_t GetRandomRange(uint8_t low, uint8_t high);
#endif

#if gNvDualImageSupport_d
/******************************************************************************
 * Name: NvGetEntryInfoNeedToAddInNVM
 * Description:
 * Parameter(s):
 * Return: number of entries
 ******************************************************************************/
NVM_STATIC uint32_t NvGetEntryInfoNeedToAddInNVM(void);
#endif
/******************************************************************************
 * Name: NV_FlashRead
 * Description: Reads flash contents copying to RAM storage.
 *
 * Parameter(s): flash_addr destination address in flash
 *               size length to be read
 *               ram_buf destination in RAM to copy flash to.
 *               check_ecc_fault if TRUE the data is read disabling ECC bus faults but notifying of error.
 *               otherwise plain memcpy from flash to RAM.
 * Return: status gNVM_OK_c if OK, gNVM_EccFault_c in case of ECC error.
 ******************************************************************************/
NVM_STATIC NVM_Status_t NV_FlashRead(uint32_t flash_addr, uint8_t *ram_buf, size_t size, bool_t check_ecc_fault);
/******************************************************************************
 * Name: NV_PartitionReadAtOffset
 * Description: Reads partition contents from partition offset - see  NV_FlashRead.
 *
 * Parameter(s): pg_id virtual partition to access.
 *               pg_offset source offset in NV partition
 *               ram_buf destination in RAM to copy flash to.
 *               otherwise plain memcpy from flash to RAM.
 *                size length to be read
 * Return: status gNVM_OK_c if OK, gNVM_EccFault_c in case of ECC error.
 ******************************************************************************/
/* HIS_CALLING: NV_PartitionReadAtOffset is a low-level I/O helper called by many functions by design. */
NVM_STATIC NVM_Status_t NV_PartitionReadAtOffset(NVM_VirtualPageID_t pg_id,
                                                 uint32_t            pg_offset,
                                                 uint8_t            *ram_buf,
                                                 size_t              size);

/******************************************************************************
 * Name: NV_PartitionProgramAtOffset
 * Description: Write data at partition offset - see  NV_FlashProgram .
 *
 * Parameter(s): pg_id virtual partition to access.
 *               pg_offset destination offset in  NV partition
 *               ram_buf source buffer in RAM to program into flash.
 *               otherwise plain memcpy from flash to RAM.
 *  *             size length to be read

 * Return: status gNVM_OK_c if OK, gNVM_EccFault_c in case of ECC error.
 ******************************************************************************/
NVM_STATIC NVM_Status_t NV_PartitionProgramAtOffset(NVM_VirtualPageID_t pg_id,
                                                    uint32_t            pg_offset,
                                                    uint8_t            *ram_buf,
                                                    size_t              size);

/******************************************************************************
 * Name: NV_PartitionProgramUnalignedAtOffset
 *
 *  Description: Write data at partition offset - see  NV_FlashProgram .
 *
 * Parameter(s): pg_id virtual partition to access.
 *               pg_offset destination offset in  NV partition
 *               ram_buf source buffer in RAM to program into flash.
 *               otherwise plain memcpy from flash to RAM.
 *               size length to be read
 * Return: status gNVM_OK_c if OK, gNVM_EccFault_c in case of ECC error.
 ******************************************************************************/
NVM_STATIC NVM_Status_t NV_PartitionProgramUnalignedAtOffset(NVM_VirtualPageID_t pg_id,
                                                             uint32_t            pg_offset,
                                                             size_t              size,
                                                             uint8_t            *ram_buf);

/******************************************************************************
 * Name: NV_PartitionBlankCheckAtOffset
 *
 *  Description: Perform blank check of area with NV partition.
 *
 * Parameter(s): pg_id virtual partition to access.
 *               offset offset to check in partition
 *               len : length to check.
 * Return: TRUE if blank, FALSE otherwise.
 ******************************************************************************/
NVM_STATIC bool_t NV_PartitionBlankCheckAtOffset(NVM_VirtualPageID_t pg_id, uint32_t offset, uint32_t len);

/******************************************************************************
 * Name: NV_FlashProgram
 * Description: Calls HAL_FlashProgram and verifies operation reading back
 *              flash content
 * Parameter(s): flash_addr destination address in flash. Must be phrase aligned.
 *               size length to be written
 *               ram_buf source from which data are read and written to flash
 *               catch_ecc_faults if TRUE the data is read back catching ECC faults
 * Return: status gNVM_OK_c if OK, gNVM_MetaInfoWriteError_c in case of error.
 ******************************************************************************/
NVM_STATIC NVM_Status_t NV_FlashProgram(uint32_t flash_addr, size_t size, uint8_t *ram_buf, bool_t catch_ecc_faults);

/******************************************************************************
 * Name: NV_FlashProgramUnaligned
 * Description: Calls HAL_FlashProgramUnaligned and verifies operation reading
 *              back flash content
 * Parameter(s): flash_addr destination address in flash
 *               size length to be written
 *               ram_buf source buffer from which data are read and written to flash
 *               catch_ecc_faults if TRUE the data is read back catching ECC faults
 * Return: status gNVM_OK_c if OK, gNVM_RecordWriteError_c in case of error.
 ******************************************************************************/
NVM_STATIC NVM_Status_t NV_FlashProgramUnaligned(uint32_t flash_addr,
                                                 size_t   size,
                                                 uint8_t *ram_buf,
                                                 bool_t   catch_ecc_faults);

#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
/*  */
/******************************************************************************
 * Name: Seek for ECC faults within address range
 * Description: Calls HAL_FlashProgramUnaligned and verifies operation reading
 *              back flash content
 * Parameter(s): [IN] start_addr start of examined range
 *               [IN] size  range length in bytes
 * Return: address of first ECC fault detected if any, 0U if no error found
 ******************************************************************************/
NVM_STATIC uint32_t NV_SweepRangeForEccFaults(uint32_t start_addr, uint32_t size);
#endif

#endif /* gNvStorageIncluded_d */

/*****************************************************************************
 *****************************************************************************
 * Private memory declarations
 *****************************************************************************
 *****************************************************************************/

#if gNvStorageIncluded_d

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerJitter_c)
NVM_STATIC uint8_t mNvmUseTimerJitter = TRUE;
#endif

/*
 * Name: mNvActivePageId
 * Description: variable that holds the ID of the active page
 */
NVM_STATIC NVM_VirtualPageID_t mNvActivePageId;

/*
 * Name: mNvPageCounter
 * Description: page counter, used to validate the entire virtual page
 *              and also to provide statistical information about
 *              how many times the virtual page was erased
 */
NVM_STATIC uint32_t mNvPageCounter = 0U;

/*
 * Name: mNvVirtualPageProperty
 * Description: virtual page properties
 */
NVM_STATIC NVM_VirtualPageProperties_t mNvVirtualPageProperty[gNvVirtualPagesCount_c];

/*
 * Name: mNvTotalPageSize
 * Description: Size of NVM virtual page in bytes.
 *              Limited to 64kB, and multiple of flash sector size.
 * Note: The NVM partition is twice as big.
 */
NVM_STATIC uint32_t mNvTotalPageSize;

/*
 * Name: mNvCopyOperationIsPending
 * Description: a flag that a indicates that a page copy operation is requested
 */
NVM_STATIC bool_t mNvCopyOperationIsPending = FALSE;

/*
 * Name: mNvErasePgCmdStatus
 * Description: a data structure used to erase a virtual page. The erase of a
 *              virtual page is performed in idle task, in a sector-by-sector
 *              manner. When the idle task runs, if the erase pending flag is
 *              set, only one flash sector will be erased. Therefore, the
 *              virtual page will be entirely erased after several runs of
 *              idle task
 */
NVM_STATIC NVM_ErasePageCmdStatus_t mNvErasePgCmdStatus;

/*
 * Name: mNvFlashConfigInitialised
 * Description: variable that holds the hal driver and active page initialisation status
 */
NVM_STATIC bool_t mNvFlashConfigInitialised = FALSE;

#if (defined gNvSalvageFromEccFault_d) && (gNvSalvageFromEccFault_d > 0)
NVM_STATIC NVM_EccFaultNotifyCb_t nv_fault_report_cb = NULL;
#endif

/*
 * Name: maNvRecordsCpyIdx
 * Description: An array that stores the indexes of the records already copied;
 *              Used by the defragmentation process.
 */
#if gNvFragmentation_Enabled_d
NVM_STATIC uint16_t maNvRecordsCpyOffsets[gNvRecordsCopiedBufferSize_c];
#endif /* gNvFragmentation_Enabled_d */

#if defined gNvmMetaCheckSum_d
/*
 * Name: mNvMetaInfoChecksumEnabled
 * Description: Protect MIT with checksum for detection of potential corruption
 *     during flash operations. When enabled, checksum is calculated and stored
 */
NVM_STATIC int mNvMetaInfoChecksumEnabled = gNvmMetaCheckSum_d;
#endif /* gNvmMetaCheckSum_d */

#if gNvUseExtendedFeatureSet_d
/*
 * Name: mNvTableSizeInFlash
 * Description: the size of the NV table stored in the FLASH memory
 */
NVM_STATIC uint16_t mNvTableSizeInFlash = 0U;

/*
 * Name: mNvTableMarker
 * Description: FLASH NV table marker, used only for code readability
 *              (when applying the sizeof() operator to it)
 */
NVM_STATIC uint16_t mNvTableMarker = gNvTableMarker_c;

/*
 * Name: mNvTableMarker
 * Description: FLASH NV application version, used for determining when table upgrade
 *              happened
 */
NVM_STATIC uint16_t mNvFlashTableVersion = gNvFlashTableVersion_c;

/*
 * Name: mNvTableUpdated
 * Description: boolean flag used to mark if the NV table from the RAM memory
 *              has been changed. Set (or left untouched) only at module initialization,
 *              when the existing NV FLASH table (if any) is compared against
 *              the NV RAM table.
 */
NVM_STATIC bool_t mNvTableUpdated;

#endif /* gNvUseExtendedFeatureSet_d */

/*
 * Name: mNvModuleInitialized
 * Description: variable that holds the NVM initialisation status
 */
NVM_STATIC bool_t mNvModuleInitialized = FALSE;

/*
 * Name: mNvMutexCreated
 * Description: variable that holds the NVM mutex created state.
 * mostly concerns unit tests, when emulating reset.
 */
NVM_STATIC bool_t mNvMutexCreated = FALSE;

/*
 * Name: mNvCriticalSectionFlag
 * Description: If this counter is != 0, do not save to NV Storage
 */
NVM_STATIC uint8_t mNvCriticalSectionFlag = 0U;

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
/*
 * Name: gNvMinimumTicksBetweenSaves
 * Description: Minimum number of calls to NvTimerTick() between saves of a given data set
 */
NVM_STATIC NvSaveInterval_t mNvMinimumTicksBetweenSaves = gNvMinimumTicksBetweenSaves_c;
#endif

/*
 * Name: gNvCountsBetweenSaves
 * Description: Minimum number of calls to NvSaveOnIdle() between saves of a given data set
 */
NVM_STATIC NvSaveCounter_t mNvCountsBetweenSaves = gNvCountsBetweenSaves_c;

/*
 * Name: mNvPendingSavesQueue
 * Description: a queue used for storing information about the pending saves
 */
NVM_STATIC NVM_SaveQueue_t mNvPendingSavesQueue;

/*
 * Name: maDatasetInfo
 * Description: Data set info table
 */
NVM_STATIC NVM_DatasetInfo_t maDatasetInfo[gNvTableEntriesCountMax_c];

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
/*
 * Name: mNvSaveOnIntervalEvent
 * Description: flag used to signal an 'SaveOnInterval' event
 */
NVM_STATIC bool_t mNvSaveOnIntervalEvent;

/*
 * Name: mNvLastTimestampValue
 * Description: the value of the last timestamp used by the Save-On-Interval functionality
 */
NVM_STATIC uint64_t mNvLastTimestampValue = 0ULL;
#endif

/*
 * Name: mNVMMutexId
 * Description: mutex used to ensure NVM functions thread switch safety
 */
#if !gNvDebugEnabled_d
NVM_STATIC
#endif
OSA_MUTEX_HANDLE_DEFINE(mNVMMutexId);

/*
 * Name: mNvIdleTaskId
 * Description: stores the Id of the task which hosts NvIdle function.
 */
NVM_STATIC osa_task_handle_t mNvIdleTaskId = NULL;

/*
 * Name: eraseNVMFirst
 * Description: byte used to the force the erasure of the first sector of
 *              the first virtual page (thus invalidating the entire page)
 *              via IAR flashloader. Below section must be defined in the
 *              linker configuration file (*.icf)
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma section                            = "fEraseNVM"
#pragma location                           = "fEraseNVM"
NVM_STATIC const uint32_t eraseNVMFirst[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};

/*
 * Name: eraseNVMSecond
 * Description: byte used to the force the erasure of the first sector of
 *              the second virtual page (thus invalidating the entire page)
 *              via IAR flashloader. Below section must be defined in the
 *              linker configuration file (*.icf)
 */
#pragma section  = "sEraseNVM"
#pragma location = "sEraseNVM"

NVM_STATIC const uint32_t eraseNVMSecond[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};

#endif /* __IAR_SYSTEMS_ICC__  */
#if defined(__GNUC__)
#if gNvmErasePartitionWhenFlashing_c
/*
 * Name: eraseNVMFirst
 * Description: byte used to the force the erasure of the first sector of
 *              the first virtual page (thus invalidating the entire page)
 *              via IAR flashloader. Below section must be defined in the
 *              linker configuration file (*.icf)
 */

NVM_STATIC const uint32_t eraseNVMFirst[4]
    __attribute__((used, section("fEraseNVM"))) = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};

/*
 * Name: eraseNVMSecond
 * Description: byte used to the force the erasure of the first sector of
 *              the second virtual page (thus invalidating the entire page)
 *              via IAR flashloader. Below section must be defined in the
 *              linker configuration file (*.icf)
 */

NVM_STATIC const uint32_t eraseNVMSecond[4]
    __attribute__((used, section("sEraseNVM"))) = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
#endif /* gNvmErasePartitionWhenFlashing_c */
#endif /* __GNUC__  */
#endif /* gNvStorageIncluded_d */

/*****************************************************************************
 *****************************************************************************
 * Public memory declarations
 *****************************************************************************
 *****************************************************************************/

#if gNvStorageIncluded_d
#if (!(defined(__CC_ARM) || defined(__UVISION_VERSION) || defined(__ARMCC_VERSION)))
/*
 * Name: NV_STORAGE_START_ADDRESS
 * Description: NV_STORAGE_START_ADDRESS from linker command file is used by this code
 *              as Raw Sector Start Address.
 */
extern uint32_t NV_STORAGE_START_ADDRESS[];

/*
 * Name: NV_STORAGE_END_ADDRESS
 * Description: NV_STORAGE_END_ADDRESS from linker command file is used by this code
 *              as Raw Sector End Address.
 */
extern uint32_t NV_STORAGE_END_ADDRESS[];

/*
 * Name: NV_STORAGE_SECTOR_SIZE
 * Description: external symbol from linker command file, it represents the size
 *              of a FLASH sector
 */
extern uint32_t NV_STORAGE_SECTOR_SIZE[];

/*
 * Name:  NV_STORAGE_MAX_SECTORS
 * Description: external symbol from linker command file, it represents the sectors
 *              count used by the ENVM storage system; it has to be a multiple of 2
 */
extern uint32_t NV_STORAGE_MAX_SECTORS[];
#else

extern uint32_t Image$$NVM_region$$ZI$$Base[];
extern uint32_t Image$$NVM_region$$ZI$$Limit[];
extern uint32_t Image$$NVM_region$$Length;

#define NV_STORAGE_START_ADDRESS (Image$$NVM_region$$ZI$$Base)
#define NV_STORAGE_END_ADDRESS   (Image$$NVM_region$$ZI$$Limit)
#define NVM_LENGTH               ((uint32_t)((uint8_t *)NV_STORAGE_END_ADDRESS) - (uint32_t)((uint8_t *)NV_STORAGE_START_ADDRESS))
#define NV_STORAGE_SECTOR_SIZE   FSL_FEATURE_FLASH_SECTOR_SIZE_BYTES
#define NV_STORAGE_MAX_SECTORS   (NVM_LENGTH / NV_STORAGE_SECTOR_SIZE)
#endif /* __CC_ARM */

/*
 * Name:  pNVM_DataTable
 * Description: Pointer to NVM table. The table itself can be stored in FLASH (default)
 *              or in RAM memory. If stored in RAM, the gNVM_TABLE_startAddr_c must be updated
 *              accordingly
 */
NVM_DataEntry_t *pNVM_DataTable = (NVM_DataEntry_t *)gNVM_TABLE_startAddr_c;

NVM_STATIC uint16_t mNVM_DataTableNbEntries = 0U;

#if gNvDualImageSupport_d
NVM_STATIC uint16_t mNvDiffEntryId[gNvTableEntriesCountMax_c];
NVM_STATIC uint16_t mNvNeedAddEntryCnt = 0U;

NVM_STATIC NVM_VirtualPageID_t mNvPreviousActivePageId = gVirtualPageNone_c;

#endif /* gNvDualImageSupport_d */
#endif /* gNvStorageIncluded_d */

/*****************************************************************************
 *****************************************************************************
 * Private functions
 *****************************************************************************
 *****************************************************************************/

#if gNvStorageIncluded_d
#if gNvUseExtendedFeatureSet_d
/******************************************************************************
 * Name: __NvRegisterTableEntry
 * Description: The function tries to register a new table entry within an
 *              existing NV table. If the NV table contained an erased (invalid)
 *              entry, the entry will be overwritten with a new one (provided
 *              by the mean of this function arguments)
 * Parameter(s): [IN] ptrData - generic pointer to RAM data to be registered
 *                              within the NV storage system
 *               [IN] uniqueId - an unique ID of the table entry
 *               [IN] elemCount - how many elements the table entry contains
 *               [IN] elemSize - the size of an element
 *               [IN] dataEntryType - the type of the new entry
 *               [IN] overwrite - if an existing table entry shall be
 *                                overwritten
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_RegisterFailure_c - invalid id or unmirrored data set
 *         gNVM_AlreadyRegistered - the id is already registered in another entry
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *****************************************************************************/
#if gNvTableKeptInRam_d
NVM_STATIC NVM_Status_t __NvRegisterTableEntry(void            *ptrData,
                                               NvTableEntryId_t uniqueId,
                                               uint16_t         elemCount,
                                               uint16_t         elemSize,
                                               uint16_t         dataEntryType,
                                               bool_t           overwrite)
{
    uint16_t     loopCnt = 0U;
    uint16_t     nullPos = gNvTableEntriesCountMax_c;
    NVM_Status_t status;
    bool_t       ret = FALSE;

    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        if (gNvInvalidDataEntry_c == uniqueId)
        {
            status = gNVM_RegisterFailure_c;
        }
        else
        {
#if gNvFragmentation_Enabled_d
            if (elemCount > (uint16_t)gNvRecordsCopiedBufferSize_c)
            {
                status = gNVM_DefragBufferTooSmall_c;
            }
            else
#endif
            {
                while (loopCnt < mNVM_DataTableNbEntries)
                {
                    if ((pNVM_DataTable[loopCnt].pData == NULL) && (!overwrite))
                    {
                        nullPos = loopCnt;
                        break;
                    }

                    if (pNVM_DataTable[loopCnt].DataEntryID == uniqueId)
                    {
                        if (overwrite)
                        {
                            /* make sure that the NvWriteRamTable writes the updated values */
                            pNVM_DataTable[loopCnt].pData         = ptrData;
                            pNVM_DataTable[loopCnt].ElementsCount = elemCount;
                            pNVM_DataTable[loopCnt].ElementSize   = elemSize;
                            pNVM_DataTable[loopCnt].DataEntryType = dataEntryType;
                            /*force page copy first*/
                            status = __NvEraseEntryFromStorage(uniqueId, loopCnt);
                        }
                        else
                        {
                            status = gNVM_AlreadyRegistered_c;
                        }
                        ret = TRUE;
                        break;
                    }
                    /* increment the loop counter */
                    loopCnt++;
                }

                if (FALSE == ret)
                {
                    if (gNvTableEntriesCountMax_c != nullPos)
                    {
                        pNVM_DataTable[nullPos].pData         = ptrData;
                        pNVM_DataTable[nullPos].DataEntryID   = uniqueId;
                        pNVM_DataTable[nullPos].ElementsCount = elemCount;
                        pNVM_DataTable[nullPos].ElementSize   = elemSize;
                        pNVM_DataTable[nullPos].DataEntryType = dataEntryType;

                        /* postpone the operation */
                        if (mNvCriticalSectionFlag > 0U)
                        {
                            mNvCopyOperationIsPending = TRUE;
                            status                    = gNVM_CriticalSectionActive_c;
                        }
                        else
                        {
                            /*update the flash table*/
                            status = NvModuleSwitchPage(gNvCopyAll_c);
                            /* mNvTableSizeInFlash will be updated after the page copy following the insertion of a new
                             * table entry */
                            if (gNVM_OK_c != status)
                            {
                                mNvModuleInitialized = FALSE;
                            }
                            else
                            {
                                mNvTableSizeInFlash = NvGetFlashTableSize();
                            }
                        }
                    }
                    else
                    {
                        status = gNVM_RegisterFailure_c;
                    }
                }
            }
        }
    }
    return status;
}
#endif /* gNvTableKeptInRam_d */

/******************************************************************************
 * Name: __NvEraseEntryFromStorage
 * Description: The function removes a table entry within the existing NV
 *              table. The RAM table must be updated before this call.
 * Parameter(s): [IN] entryId - the entry id of the entry that is removed
 *               [IN] tableEntryIndex - the index of the entry in the ram table
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *****************************************************************************/
#if gNvTableKeptInRam_d
NVM_STATIC NVM_Status_t __NvEraseEntryFromStorage(uint16_t entryId, uint16_t tableEntryIndex)
{
    uint16_t     loopCnt;
    NVM_Status_t status = gNVM_OK_c;
    uint16_t     remaining_count;

    /* Check if is in pending queue - if yes than remove it */
    if (NvIsPendingOperation())
    {
        /* Start from the queue's head */
        loopCnt         = mNvPendingSavesQueue.Head;
        remaining_count = mNvPendingSavesQueue.EntriesCount;

        while (remaining_count > 0U)
        {
            if (entryId == mNvPendingSavesQueue.QData[loopCnt].entryId)
            {
                mNvPendingSavesQueue.QData[loopCnt].entryId = gNvInvalidDataEntry_c;
            }
            remaining_count--;
            /* increment and wrap the loop index */
            if (++loopCnt >= (uint8_t)gNvPendingSavesQueueSize_c)
            {
                loopCnt = 0U;
            }
        }
    }
    maDatasetInfo[tableEntryIndex].countsToNextSave = mNvCountsBetweenSaves;
    maDatasetInfo[tableEntryIndex].saveNextInterval = FALSE;

    /* postpone the operation */
    if (mNvCriticalSectionFlag > 0U)
    {
        mNvCopyOperationIsPending = TRUE;
        status                    = gNVM_CriticalSectionActive_c;
    }
    else
    {
        /* erase the table entry by making a copy of the active page to the inactive one,
         * but skipping while copying the table entry to be erased */
        if (gNvInvalidMetaOffset_c != mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset)
        {
            status = NvModuleSwitchPage(entryId);
        }
    }
    return status;
}
#endif /* gNvTableKeptInRam_d */

/******************************************************************************
 * Name: NvIsRamTableUpdated
 * Description: Checks if the the NV table from RAM memory has changed since
 *              last system reset (e.g. via an OTA transfer)
 * Parameter(s): -
 * Return: TRUE if the NV RAM table has been changed / FALSE otherwise
 ******************************************************************************/

/*!
 * \brief Helper: check whether a single flash NVM entry differs from its RAM counterpart.
 *
 * HIS_LEVEL: extracted from NvIsRamTableUpdated to reduce nesting depth.
 *
 * \param[in]  entryInfo  Flash entry read from NVM table area.
 * \param[out] pChanged   Set to TRUE if a mismatch is detected.
 * \return TRUE if the entry was found in the RAM table, FALSE otherwise.
 */
NVM_STATIC bool_t NvCheckEntryChanged(const NVM_EntryInfo_t *entryInfo, bool_t *pChanged)
{
    /* HIS_VOCF: cache all repeated field accesses to minimise unique operand count */
    bool_t         idFound     = FALSE;
    uint16_t       idx         = 0U;
    uint16_t const entryId     = entryInfo->u.fields.NvDataEntryID;
    uint16_t const nbEntries   = mNVM_DataTableNbEntries;
    uint16_t const nvEntryType = entryInfo->u.fields.NvDataEntryType;
    uint16_t const nvEltCount  = entryInfo->u.fields.NvElementsCount;
    uint16_t const nvEltSize   = entryInfo->u.fields.NvElementSize;
    uint16_t const mirroredVal = (uint16_t)gNVM_MirroredInRam_c;

    /* search for the matching entry ID in the RAM table */
    while ((idx < nbEntries) && (!idFound))
    {
        if (entryId == pNVM_DataTable[idx].DataEntryID)
        {
            idFound = TRUE;
        }
        else
        {
            idx++;
        }
    }

    if (idFound)
    {
        uint16_t const ramEntryType = pNVM_DataTable[idx].DataEntryType;
        uint16_t const ramEltCount  = pNVM_DataTable[idx].ElementsCount;
        uint16_t const ramEltSize   = pNVM_DataTable[idx].ElementSize;
        /* check if the mirroring attribute changed between images */
        if (((mirroredVal == nvEntryType) || (mirroredVal == ramEntryType)) && (nvEntryType != ramEntryType))
        {
            *pChanged = TRUE;
        }
        /* check if element count or element size have changed */
        else if ((nvEltCount != ramEltCount) || (nvEltSize != ramEltSize))
        {
            *pChanged = TRUE;
        }
        else
        {
            /* entry matches the RAM table, no update needed */
        }
    }
    return idFound;
}

NVM_STATIC bool_t NvIsRamTableUpdated(void)
{
    /* HIS_LEVEL: inner comparison logic extracted to NvCheckEntryChanged helper. */
    bool_t ret = FALSE;
    /*  page counter size + table marker + table version */
    uint32_t end_offs = ((uint32_t)mNvTableSizeInFlash + (uint32_t)sizeof(NVM_TableInfo_t));
    if (end_offs <= (uint32_t)UINT16_MAX)
    {
        for (uint16_t loop_offs = (uint16_t)sizeof(NVM_TableInfo_t); loop_offs < end_offs;
             loop_offs += (uint16_t)sizeof(NVM_EntryInfo_t))
        {
            NVM_EntryInfo_t entryInfo;
            bool_t          changed;
            bool_t          found;
            /* read the flash entry */
            NVM_Status_t status =
                NV_PartitionReadAtOffset(mNvActivePageId, loop_offs, (uint8_t *)&entryInfo, sizeof(NVM_EntryInfo_t));
            if (gNVM_OK_c != status)
            {
                break;
            }
            changed = FALSE;
            found   = NvCheckEntryChanged(&entryInfo, &changed);

            if (!found)
            {
                /* entry from NVM not found in the RAM table - may belong to another application image */
#if !gNvDualImageSupport_d
                ret = TRUE;
                break;
#endif /* !gNvDualImageSupport_d */
            }
            else if (changed)
            {
                ret = TRUE;
                break;
            }
            else
            {
                /* entry matches, continue */
            }
        } /* for */
    }
    else
    {
        assert(false);
    }
    return ret;
}
/******************************************************************************
 * Name: NvGetTableEntry
 * Description: get the NV table entry information stored on FLASH memory
 * Parameter(s): [IN] tblEntryId - table entry ID
 *               [OUT] pDataEntry - a pointer to a memory location where the
 *                                  entry information will be stored
 * Return: TRUE if matching a table entry has been found / FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvGetTableEntry(uint16_t tblEntryId, NVM_DataEntry_t *pDataEntry)
{
    uint32_t                offs;
    NVM_TableAndEntryInfo_t tmp;
    bool_t                  ret = FALSE;

    pDataEntry->pData = NULL; /* the data pointer is not saved on FLASH table and
                               * shall not be used by the caller of this function */

    offs = 0U;

    if (gNVM_OK_c == NV_PartitionReadAtOffset(mNvActivePageId, offs, (uint8_t *)&tmp, sizeof(NVM_TableAndEntryInfo_t)))
    {
        if (mNvTableMarker == tmp.tableInfo.u.fields.NvTableMarker)
        {
            /* increment address */
            offs += sizeof(NVM_TableAndEntryInfo_t);

            do
            {
                NVM_EntryInfo_t *p_entry;

                if (gNVM_OK_c !=
                    NV_PartitionReadAtOffset(mNvActivePageId, offs, (uint8_t *)&tmp, sizeof(NVM_TableAndEntryInfo_t)))
                {
                    break;
                }
                if (mNvTableMarker == tmp.tableInfo.u.fields.NvTableMarker)
                {
                    /* reached end of table */
                    break;
                }
                p_entry = &tmp.entryInfo;
                if (p_entry->u.fields.NvDataEntryID == tblEntryId)
                {
                    pDataEntry->DataEntryID   = p_entry->u.fields.NvDataEntryID;
                    pDataEntry->DataEntryType = p_entry->u.fields.NvDataEntryType;
                    pDataEntry->ElementsCount = p_entry->u.fields.NvElementsCount;
                    pDataEntry->ElementSize   = p_entry->u.fields.NvElementSize;
                    ret                       = TRUE;
                    break;
                }

                /* continue searching */
                offs += sizeof(NVM_EntryInfo_t);
            } while (offs < mNvTotalPageSize);
        }
    }

    if (FALSE == ret)
    {
        pDataEntry->DataEntryType = 0U;
        pDataEntry->ElementsCount = 0U;
        pDataEntry->ElementSize   = 0U;
        pDataEntry->DataEntryID   = gNvInvalidDataEntry_c;
    }
    return ret;
}

/******************************************************************************
 * Name: NvGetFlashTableSize
 * Description: Retrieves the size of the NV table
 * Parameter(s): -
 * Return: the NV table size
 ******************************************************************************/
NVM_STATIC uint16_t NvGetFlashTableSize(void)
{
    uint32_t        offs   = 0U;
    uint16_t        size16 = 0U;
    NVM_TableInfo_t tableInfo;

    /* compute the size of the table stored in Flash memory */
    do
    {
        uint32_t size = 0U;
        if (gNVM_OK_c !=
            NV_PartitionReadAtOffset(mNvActivePageId, offs, (uint8_t *)&tableInfo, sizeof(NVM_TableInfo_t)))
        {
            /* Reading the start of the page has failed */
            break;
        }
        if (gNvTableMarker_c != tableInfo.u.fields.NvTableMarker)
        {
            /* The first phrase must contain the table marker */
            break;
        }
        /* Now iterate */
        for (offs = (uint32_t)sizeof(NVM_TableInfo_t); offs < mNvTotalPageSize;
             offs += (uint32_t)sizeof(NVM_TableInfo_t))
        {
            if (gNVM_OK_c != NV_PartitionReadAtOffset(mNvActivePageId, offs, (uint8_t *)&tableInfo,
                                                      (uint32_t)sizeof(NVM_TableInfo_t)))
            {
                size = 0U;
                break;
            }
            if (gNvTableMarker_c == tableInfo.u.fields.NvTableMarker)
            {
                /* Stop when closing table marker is found */
                break;
            }
            if (offs >= (mNvTotalPageSize - (uint32_t)sizeof(NVM_TableInfo_t)))
            {
                size = 0U;
                break;
            }
            size += (uint32_t)sizeof(NVM_TableInfo_t);
        }
        if ((size < mNvTotalPageSize) && (size <= ((uint32_t)UINT16_MAX)))
        {
            size16 = (uint16_t)size;
        }
    } while (false);

    return size16;
}

#if gNvDualImageSupport_d
/******************************************************************************
 * Name: NvGetEntryInfoNeedToAddInNVM
 * Description:
 * Return: number of entries to be added in NVM page
 ******************************************************************************/
NVM_STATIC uint32_t NvGetEntryInfoNeedToAddInNVM(void)
{
    NVM_Status_t            status;
    uint16_t                i, j;
    uint32_t                offs, end_offs;
    bool_t                  isDiffEntry;
    NVM_TableAndEntryInfo_t tableAndEntryInfo;
    uint16_t                NV_AllNVMEntryId[gNvTableEntriesCountMax_c];
    uint16_t                AllNVMEntryCnt = 0U;

    /* compute the size of the Entry stored in Flash memory */
    FLib_MemSet(&NV_AllNVMEntryId[0], 0xffU, sizeof(uint16_t) * gNvTableEntriesCountMax_c);

    mNvNeedAddEntryCnt = 0U;
    do
    {
        offs = 0U;
        if (NULL == pNVM_DataTable)
        {
            assert(false);
            break;
        }
        end_offs = (uint32_t)mNvTableSizeInFlash + sizeof(NVM_TableInfo_t);

        status =
            NV_PartitionReadAtOffset(mNvActivePageId, offs, (uint8_t *)&tableAndEntryInfo, sizeof(NVM_TableInfo_t));
        if (gNVM_OK_c != status)
        {
            break;
        }
        if ((gNvTableMarker_c != tableAndEntryInfo.tableInfo.u.fields.NvTableMarker))
        {
            break;
        }
        /* Get all Entry ID from NVM*/
        for (offs = (uint32_t)sizeof(NVM_TableInfo_t); offs < end_offs; offs += (uint32_t)sizeof(NVM_TableInfo_t))
        {
            /* Assume ecc error in NVM table*/
            status =
                NV_PartitionReadAtOffset(mNvActivePageId, offs, (uint8_t *)&tableAndEntryInfo, sizeof(NVM_TableInfo_t));
            if (gNVM_OK_c != status)
            {
                /* If it could not be read, what's to be done ? Skip fault and continue ?*/
                continue;
            }
            if (gNvTableMarker_c == tableAndEntryInfo.tableInfo.u.fields.NvTableMarker)
            {
                /* We found the end of the table */
                break;
            }
            /* Recollect all existing table entries within NV_AllNVMEntryId array */
            if (tableAndEntryInfo.entryInfo.u.fields.NvDataEntryID != gNvInvalidTableEntryIndex_c)
            {
                if (AllNVMEntryCnt < (gNvTableEntriesCountMax_c - 1U))
                {
                    NV_AllNVMEntryId[AllNVMEntryCnt] = tableAndEntryInfo.entryInfo.u.fields.NvDataEntryID;
                    AllNVMEntryCnt++;
                }
                else
                {
                    assert(AllNVMEntryCnt < (gNvTableEntriesCountMax_c - 1U));
                    break;
                }
            }
        }
        /* Parse application entries and find which ones need to be added */
        for (i = 0U; i < mNVM_DataTableNbEntries; i++)
        {
            /* Can skip void entries in the dual image case */
            if ((pNVM_DataTable[i].ElementsCount == 0U) || (pNVM_DataTable[i].ElementSize == 0U))
            {
                /* skip this entry that is empty */
                continue;
            }

            isDiffEntry = TRUE;
            for (j = 0U; j < AllNVMEntryCnt; j++)
            {
                if (pNVM_DataTable[i].DataEntryID == NV_AllNVMEntryId[j])
                {
                    isDiffEntry = FALSE;
                    break;
                }
            }

            if (isDiffEntry)
            {
                if (mNvNeedAddEntryCnt < (gNvTableEntriesCountMax_c - 1U))
                {
                    mNvDiffEntryId[mNvNeedAddEntryCnt++] = pNVM_DataTable[i].DataEntryID;
                }
                else
                {
                    assert(mNvNeedAddEntryCnt < gNvTableEntriesCountMax_c - 1U);
                    mNvNeedAddEntryCnt = 0U;
                }
            }
        }
    } while (false);

    return mNvNeedAddEntryCnt;
}

/*!
 * \brief Commit all NVM_EntryInfo_t (data-set entry descriptors) to a flash virtual page.
 *
 * The function performs a two-phase write of the NVM data-entry table to the
 * destination virtual page identified by \p dstPageId:
 *
 * Phase 1 - mirror existing flash entries:
 *   Iterates over every NVM_EntryInfo_t record stored in the currently active
 *   page (mNvActivePageId).  For each record found in flash, if a matching
 *   entry is present in the RAM data table (pNVM_DataTable), the RAM copy is
 *   used so that any in-memory metadata updates are preserved; otherwise the
 *   flash copy is written as-is.
 *
 * Phase 2 - append new RAM-only entries:
 *   After mirroring the flash table, any RAM data-table entries that were not
 *   present in flash (tracked by mNvDiffEntryId[] / mNvNeedAddEntryCnt) are
 *   appended to the destination page.  On success mNvNeedAddEntryCnt is reset
 *   to zero.
 *
 * The write offset pointed to by \p pWriteOffset is updated on every
 * successful NVM_EntryInfo_t write so the caller can continue appending data
 * records immediately after the table section.
 *
 * \note This function is only compiled when both gNvUseExtendedFeatureSet_d
 *       and gNvDualImageSupport_d are defined.
 *
 * \param[in]     pageId        Identifier of the destination virtual page that
 *                              will receive the entry descriptors.
 * \param[in,out] pWriteOffset  On entry: byte offset within \p dstPageId at which
 *                              the first NVM_EntryInfo_t is written (must point
 *                              past the NVM_TableInfo_t header).  On return:
 *                              updated to the offset of the first byte following
 *                              the last written NVM_EntryInfo_t.
 *
 * \return gNVM_OK_c            All entry descriptors were written successfully.
 * \return other                Any NVM_Status_t error code returned by
 *                              NV_PartitionReadAtOffset() or
 *                              NV_PartitionProgramAtOffset() on a hard failure.
 */
NVM_STATIC NVM_Status_t NvSaveAllDataSetEntry(NVM_VirtualPageID_t dstPageId, uint32_t *pWriteOffset)
{
    uint32_t        srcOffset;
    NVM_EntryInfo_t entryInfo;
    uint32_t        write_offset = *pWriteOffset;
    bool_t          isSameEntryFoundInRam;
    bool_t          isSaveError;
    NVM_Status_t    status = gNVM_OK_c;

    NvInitializeEntryInfo(&entryInfo, 0U);

    if ((mNvPreviousActivePageId != gVirtualPageNone_c) && (mNvTableSizeInFlash > 0U))
    {
        /* Denotes that we are coming from NvCopyPage so mNvPreviousActivePageId is equal to mNvActivePageId and
         * dstPageId is the other page. We need to mirror all entries from the active page to the destination page.
         */
        for (srcOffset = sizeof(NVM_TableInfo_t); srcOffset < (sizeof(NVM_TableInfo_t) + mNvTableSizeInFlash);
             srcOffset += sizeof(NVM_EntryInfo_t))
        {
            /* read NV table entry info */
            /* Assume that the source page may contain errors */
            status = NV_PartitionReadAtOffset(mNvPreviousActivePageId, srcOffset, (uint8_t *)&entryInfo,
                                              sizeof(NVM_EntryInfo_t));
            if (gNVM_OK_c != status)
            {
                /* Write a blank (erased) entry to the destination to keep the
                 * source/destination table slot count consistent (CERT-C correctness).
                 * Resetting status here is intentional: a read fault on the source
                 * page is non-fatal; the destination receives a blank placeholder. */
                NvInitializeEntryInfo(&entryInfo, gNvErasedFlashCellValue_c);
                status = NV_PartitionProgramAtOffset(dstPageId, write_offset, (uint8_t *)&entryInfo,
                                                     sizeof(NVM_EntryInfo_t));
                if (gNVM_OK_c != status)
                {
                    break;
                }
                write_offset += sizeof(NVM_EntryInfo_t);
                continue;
            }
            isSameEntryFoundInRam = FALSE;
            for (uint16_t idx = 0U; idx < mNVM_DataTableNbEntries; idx++)
            {
                assert(NULL != pNVM_DataTable);
                /* Can skip void entries in the case of Dual Image */
                if (pNVM_DataTable[idx].ElementsCount == 0U || pNVM_DataTable[idx].ElementSize == 0U)
                {
                    continue;
                }

                if (pNVM_DataTable[idx].DataEntryID == entryInfo.u.fields.NvDataEntryID)
                {
                    /* we use entry info from RAM and construct NVM_EntryInfo_t to be written to NVM */
                    NvInitializeEntryInfo(&entryInfo, 0xFFU); /* preset padding to blank value */
                    entryInfo.u.fields.NvDataEntryID   = pNVM_DataTable[idx].DataEntryID;
                    entryInfo.u.fields.NvDataEntryType = pNVM_DataTable[idx].DataEntryType;
                    entryInfo.u.fields.NvElementsCount = pNVM_DataTable[idx].ElementsCount;
                    entryInfo.u.fields.NvElementSize   = pNVM_DataTable[idx].ElementSize;
                    isSameEntryFoundInRam              = TRUE;
                    break; /* for loop */
                }
            }

            if (!isSameEntryFoundInRam)
            {
                /* we use entry info from NVM  */
                /* write the one found in NVM as was already */
            }
            status =
                NV_PartitionProgramAtOffset(dstPageId, write_offset, (uint8_t *)&entryInfo, sizeof(NVM_EntryInfo_t));
            if (gNVM_OK_c != status)
            {
                break;
            }

            /* increment offset */
            write_offset += sizeof(NVM_EntryInfo_t);
        }
    }
    /* Do not continue if a previous error was raised already */
    if (gNVM_OK_c == status)
    {
        /* Previous we save all NV Entry from NVM, and part of NV Entry from RAM which have same entry ID as from NVM
           also saved, next step we save remain NV Entry from RAM */
        if (mNvNeedAddEntryCnt != 0U)
        {
            isSaveError = FALSE;
            for (uint16_t idx = 0U; idx < mNVM_DataTableNbEntries; idx++)
            {
                /* Can skip void entries in the case of Dual Image */
                if ((pNVM_DataTable[idx].ElementsCount == 0U) || (pNVM_DataTable[idx].ElementSize == 0U))
                {
                    /* skip this entry that is empty */
                    continue;
                }
                for (uint16_t DifIdx = 0U; DifIdx < mNvNeedAddEntryCnt; DifIdx++)
                {
                    /* different entries from NVM are also saved */
                    if (mNvDiffEntryId[DifIdx] == pNVM_DataTable[idx].DataEntryID)
                    {
                        /* we use entry info from RAM */
                        NvInitializeEntryInfo(&entryInfo, 0xFFU); /* preset padding to blank value */

                        entryInfo.u.fields.NvDataEntryID   = pNVM_DataTable[idx].DataEntryID;
                        entryInfo.u.fields.NvDataEntryType = pNVM_DataTable[idx].DataEntryType;
                        entryInfo.u.fields.NvElementsCount = pNVM_DataTable[idx].ElementsCount;
                        entryInfo.u.fields.NvElementSize   = pNVM_DataTable[idx].ElementSize;
                        status = NV_PartitionProgramAtOffset(dstPageId, write_offset, (uint8_t *)&entryInfo,
                                                             sizeof(NVM_EntryInfo_t));
                        if (gNVM_OK_c != status)
                        {
                            isSaveError = TRUE;
                            break; /* for loop */
                        }
                        /* increment offset */
                        write_offset += sizeof(NVM_EntryInfo_t);
                    }
                } /* inner for */
                if (isSaveError)
                {
                    break; /* out for */
                }
            }              /* outer for */

            if (!isSaveError)
            {
                mNvNeedAddEntryCnt = 0U;
            }
        }
    }
    *pWriteOffset = write_offset;
    return status;
}

#endif /* gNvDualImageSupport_d */

NVM_STATIC void NvInitializeEntryInfo(NVM_EntryInfo_t *p_entry, uint8_t val)
{
    /* Initialize either to 0 or to 0xff */
    FLib_MemSet((uint8_t *)p_entry, val, sizeof(NVM_EntryInfo_t));
}

#endif /* gNvUseExtendedFeatureSet_d */

/******************************************************************************
 * Name: __NvAtomicSave
 * Description: The function performs an atomic save of the entire NV table
 *              to the storage system. The operation is performed
 *              in place (atomic).
 * Parameter(s):  -
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *         gNVM_PointerOutOfRange_c - if the pointer is out of range
 *         gNVM_InvalidTableEntry_c - if the table entry is not valid
 *         gNVM_MetaInfoWriteError_c - meta tag couldn't be written
 *         gNVM_RecordWriteError_c - record couldn't be written
 *         gNVM_CriticalSectionActive_c - the module is in critical section
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvAtomicSave(void)
{
    NVM_Status_t         status  = gNVM_OK_c;
    uint16_t             loopCnt = 0U;
    NVM_TableEntryInfo_t tblIdx;
#if gUnmirroredFeatureSet_d
    uint16_t loopCnt2 = 0U;
    uint16_t remaining_count;
    uint16_t tableEntryIdx;
    bool_t   skip;
    bool_t   ret = FALSE;
#endif

    do
    {
        /* remove all non unmirrored erase operations from the queue */
#if gUnmirroredFeatureSet_d
        if (NvIsPendingOperation())
        {
            /* Start from the queue's head */
            loopCnt         = mNvPendingSavesQueue.Head;
            remaining_count = mNvPendingSavesQueue.EntriesCount;

            while (remaining_count != 0U)
            {
                skip          = FALSE;
                tableEntryIdx = NvGetTableEntryIndexFromId(mNvPendingSavesQueue.QData[loopCnt].entryId);
                if (gNvInvalidTableEntryIndex_c != tableEntryIdx)
                {
                    if (pNVM_DataTable[tableEntryIdx].DataEntryType != (uint16_t)gNVM_MirroredInRam_c)
                    {
                        if (NULL == ((void **)pNVM_DataTable[tableEntryIdx]
                                         .pData)[mNvPendingSavesQueue.QData[loopCnt].elementIndex])
                        {
                            skip = TRUE;
                        }
                    }
                }
                if (FALSE == skip)
                {
                    mNvPendingSavesQueue.QData[loopCnt].entryId = gNvInvalidDataEntry_c;
                }
                remaining_count--;
                /* increment and wrap the loop index */
                if (++loopCnt >= (uint8_t)gNvPendingSavesQueueSize_c)
                {
                    loopCnt = 0U;
                }
            }
        }
#else  /*gUnmirroredFeatureSet_d*/
        NvInitPendingSavesQueue();
#endif /*gUnmirroredFeatureSet_d*/
        /* if critical section, add a special entry in the queue */
        if (mNvCriticalSectionFlag != 0U)
        {
            tblIdx.entryId      = gNvCopyAll_c;
            tblIdx.elementIndex = gNvCopyAll_c;
            tblIdx.op_type      = OP_SAVE_ALL;
            status              = NvAddSaveRequestToQueue(&tblIdx);
            if ((gNVM_SaveRequestRejected_c != status) && (gNVM_AtomicSaveRecursive_c != status))
            {
                status = gNVM_CriticalSectionActive_c;
            }
        }
        else
        {
            while (loopCnt < mNVM_DataTableNbEntries)
            {
#if gUnmirroredFeatureSet_d
                if (pNVM_DataTable[loopCnt].DataEntryType != (uint16_t)gNVM_MirroredInRam_c)
                {
                    for (loopCnt2 = 0U; loopCnt2 < pNVM_DataTable[loopCnt].ElementsCount; loopCnt2++)
                    {
                        status = __NvSyncSave(&((uint8_t **)pNVM_DataTable[loopCnt].pData)[loopCnt2], FALSE);
                        if (gNVM_NullPointer_c == status)
                        {
                            /* skip */
                            continue;
                        }

                        if (gNVM_OK_c != status)
                        {
                            ret = TRUE;
                            break;
                        }
                    }
                    if (TRUE == ret)
                    {
                        break;
                    }
                }
                else
#endif
                {
                    status = __NvSyncSave(pNVM_DataTable[loopCnt].pData, TRUE);
                    if (gNVM_NullPointer_c == status)
                    {
                        /* skip */
                        loopCnt++;
                        continue;
                    }

                    if (gNVM_OK_c != status)
                    {
                        break;
                    }
                }

                /* increment the loop counter */
                loopCnt++;
            }
        }
    } while (status == gNVM_AtomicSaveRecursive_c);
    return status;
}

/******************************************************************************
 * Name: __NvSyncSave
 * Description: The function saves the pointed element or the entire table
 *              entry to the storage system. The save operation is not
 *              performed on the idle task but within this function call.
 * Parameter(s): [IN] ptrData - a pointer to data to be saved
 *               [IN] saveAll - specifies if the entire table entry shall be
 *                              saved or only the pointed element
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *         gNVM_PointerOutOfRange_c - if the pointer is out of range
 *         gNVM_InvalidTableEntry_c - if the table entry is not valid
 *         gNVM_MetaInfoWriteError_c - meta tag couldn't be written
 *         gNVM_RecordWriteError_c - record couldn't be written
 *         gNVM_CriticalSectionActive_c - the module is in critical section
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvSyncSave(void *ptrData, bool_t saveAll)
{
    NVM_TableEntryInfo_t tblIdx;
    NVM_Status_t         status;

    do
    {
        if (NULL == ptrData)
        {
            status = gNVM_NullPointer_c;
            break;
        }
        status = NvGetEntryFromDataPtr(ptrData, &tblIdx);
        if (gNVM_OK_c != status)
        {
            break;
        }
        /* write the save all flag */
#if gNvFragmentation_Enabled_d
        tblIdx.op_type = saveAll ? OP_SAVE_ALL : OP_SAVE_SINGLE;
#else
        tblIdx.op_type = OP_SAVE_ALL;
#endif /* gNvFragmentation_Enabled_d */

        if (mNvCriticalSectionFlag > 0U)
        {
            status = NvAddSaveRequestToQueue(&tblIdx);
            if (gNVM_SaveRequestRejected_c != status)
            {
                status = gNVM_CriticalSectionActive_c;
                break;
            }
        }
        else
        {
            status = NvWriteRecord(&tblIdx);
            if (status == gNVM_PageCopyPending_c)
            {
                /* copy active page */
                status = NvModuleSwitchPage(gNvCopyAll_c);
                if (status != gNVM_OK_c)
                {
                    break;
                }
                mNvCopyOperationIsPending = FALSE;

                /* erase old page */
                status = NvEraseVirtualPage(mNvErasePgCmdStatus.NvPageToErase);
                if (gNVM_OK_c != status)
                {
                    break;
                }
                mNvVirtualPageProperty[mNvErasePgCmdStatus.NvPageToErase].NvLastMetaInfoOffset = gNvInvalidMetaOffset_c;
                mNvErasePgCmdStatus.NvErasePending                                             = FALSE;
                /* write record */
                status = NvWriteRecord(&tblIdx);
            }
        }
    } while (FALSE);

    return status;
}

#if gUnmirroredFeatureSet_d
/******************************************************************************
 * Name: NvClearUnmirroredEntries
 *
 * Description: Clear unmirrored entries : Rid of NVM DataTable entries that reside in flash.

 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void NvClearUnmirroredEntries(void)
{
    /* Parse all array of dataset */
    for (uint16_t loopCnt = 0U; loopCnt < mNVM_DataTableNbEntries; loopCnt++)
    {
        maDatasetInfo[loopCnt].countsToNextSave = mNvCountsBetweenSaves;
        maDatasetInfo[loopCnt].saveNextInterval = FALSE;
        if (pNVM_DataTable[loopCnt].DataEntryType != (uint16_t)gNVM_MirroredInRam_c)
        {
            for (uint16_t loopCnt2 = 0U; loopCnt2 < pNVM_DataTable[loopCnt].ElementsCount; loopCnt2++)
            {
                /* If address in flash clear pointer */
                if (NvIsNVMFlashAddress(((void **)pNVM_DataTable[loopCnt].pData)[loopCnt2]))
                {
                    ((void **)pNVM_DataTable[loopCnt].pData)[loopCnt2] = NULL;
                }
            }
        }
    }
}
#endif

/******************************************************************************
 * Name: __NvFormat
 * Description: Format the NV storage system. The function erases both virtual
 *              pages and then writes the page counter/ram table to active page.
 * Parameter(s): -
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_FormatFailure_c - if the format operation fails
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_CriticalSectionActive_c - if the system has entered in a
 *                                        critical section
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvFormat(void)
{
    NVM_Status_t    status = gNVM_OK_c;
    NVM_TableInfo_t tableInfo;

    do
    {
        if (mNvCriticalSectionFlag > 0U)
        {
            status = gNVM_CriticalSectionActive_c;
            break;
        }
        /* First read table information */
        status = NV_FlashRead(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress, (uint8_t *)&tableInfo,
                              sizeof(NVM_TableInfo_t), TRUE);
        if (gNVM_OK_c != status)
        {
            /* Could not read previous value force it to 1, do not not exit without calling NvInternalFormat  */
#if gNvUseExtendedFeatureSet_d
            tableInfo.u.fields.NvPageCounter  = 1UL;
            tableInfo.u.fields.NvTableMarker  = 0U; /* will be filled in by NvInternalFormat */
            tableInfo.u.fields.NvTableVersion = 0U;
#else
            tableInfo.u.fields.NvPageCounter = 1ULL;
#endif
        }
        status = NvInternalFormat((uint32_t)tableInfo.u.fields.NvPageCounter);
        if (gNVM_OK_c != status)
        {
            break;
        }
#if gUnmirroredFeatureSet_d
        /* Rid of NVM DataTable entries that reside in flash */
        NvClearUnmirroredEntries();
#endif
        /* clear the save queue */
        NvInitPendingSavesQueue();
    } while (FALSE);

    return status;
}

/******************************************************************************
 * Name: __NvIdle
 * Description: Called from the idle task (bare-metal) or NVM_Task (MQX,
 *              FreeRTOS) to process the pending saves, erase or copy
 *              operations.
 * Parameters: -
 * Return: Number of operations executed.
 ******************************************************************************/
NVM_STATIC int __NvIdle(void)
{
    NVM_TableEntryInfo_t tblIdx;
    int                  nb_operation = 0;
    NVM_Status_t         status;
    bool_t               ret = FALSE;

    if (mNvModuleInitialized && (mNvCriticalSectionFlag == 0U))
    {
        if (mNvCopyOperationIsPending)
        {
            status = NvModuleSwitchPage(gNvCopyAll_c);
            if (gNVM_OK_c == status)
            {
                mNvCopyOperationIsPending = FALSE;
            }
        }

        if (mNvErasePgCmdStatus.NvErasePending)
        {
            /* CERT INT31-C: guard division result fits in uint8_t before cast */
            uint32_t sectorCount = mNvTotalPageSize / (uint32_t)NV_STORAGE_SECTOR_SIZE;
            assert(sectorCount <= (uint32_t)UINT8_MAX);
            if (mNvErasePgCmdStatus.NvSectorIndex >= (uint8_t)(sectorCount & (uint32_t)UINT8_MAX))
            {
                /* all sectors of the page had been erased */
                mNvVirtualPageProperty[mNvErasePgCmdStatus.NvPageToErase].NvLastMetaInfoOffset = gNvInvalidMetaOffset_c;
                mNvErasePgCmdStatus.NvErasePending                                             = FALSE;
                FSCI_NV_VIRT_PAGE_ERASE_MONITOR(
                    mNvVirtualPageProperty[mNvErasePgCmdStatus.NvPageToErase].NvRawSectorStartAddress, gNVM_OK_c);
                ret = TRUE;
            }
            else
            {
                /* erase one sector */
                uint32_t sectorAddr =
                    NV_PAGE_ADDR(mNvErasePgCmdStatus.NvPageToErase,
                                 (uint32_t)mNvErasePgCmdStatus.NvSectorIndex * (uint32_t)NV_STORAGE_SECTOR_SIZE);
                (void)HAL_FlashEraseSector(sectorAddr, (uint32_t)NV_STORAGE_SECTOR_SIZE);

                /* blank check */
                if (kStatus_HAL_Flash_Success ==
                    HAL_FlashVerifyErase(sectorAddr, (uint32_t)NV_STORAGE_SECTOR_SIZE, kHAL_Flash_MarginValueNormal))

                {
                    mNvErasePgCmdStatus.NvSectorIndex++;
                    ret = TRUE;
                }
            }
        }
        if (FALSE == ret)
        {
#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
            /* process the save-on-interval requests */
            if (mNvSaveOnIntervalEvent)
            {
                uint64_t currentTimestampValue = 0ULL;

                currentTimestampValue = TM_GetTimestamp();
                uint64_t tim_diff     = currentTimestampValue - mNvLastTimestampValue;
                bool_t   elapsed      = FALSE;

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerJitter_c)
                if (mNvmUseTimerJitter != 0u)
                {
                    uint8_t timerJitter;

                    timerJitter = GetRandomRange(0U, 100U);

                    if (tim_diff >= (uint64_t)((gNvOneSecondInMicros_c +
                                                ((uint64_t)timerJitter * (uint64_t)gNvJitterMultiplier_c)) -
                                               (gNvJitterDecrement_c)))
                    {
                        elapsed = TRUE;
                    }
                }
                else
#endif /* gNvmUseSaveOnTimerJitter_c */
                {
                    if (tim_diff >= (uint64_t)gNvOneSecondInMicros_c)
                    {
                        elapsed = TRUE;
                    }
                }
                if (elapsed)
                {
                    mNvSaveOnIntervalEvent = __NvTimerTick(TRUE);
                    mNvLastTimestampValue  = currentTimestampValue;
                }
            }
#endif /* gNvmUseSaveOnTimerJitter_c */
            /* process the save-on-idle requests */
            while (NvGetPendingSaveHead(&tblIdx))
            /* so long as we find something in Head, there are pending operations */
            {
                if ((gNvCopyAll_c == tblIdx.entryId) && (gNvCopyAll_c == tblIdx.elementIndex) &&
                    (OP_SAVE_ALL == tblIdx.op_type))
                {
                    (void)__NvAtomicSave();
                    NvRemovePendingSaveHead();
                    continue;
                }
                else if (gNvInvalidDataEntry_c == tblIdx.entryId)
                {
                    NvRemovePendingSaveHead();
                    continue;
                }
                else
                {
                    /*MISRA rule 15.7*/
                }

                if (NvWriteRecord(&tblIdx) == gNVM_PageCopyPending_c)
                {
                    /* was left in queue : do not add again and reorder write */
                    break;
                }
                NvRemovePendingSaveHead();
                nb_operation++;
                if (nb_operation > (int)gNvPendingSavesQueueSize_c)
                {
                    assert(false);
                    break;
                }
            }
        }
    }
    return nb_operation;
}
/******************************************************************************
 * Name: __NvIsDataSetDirty
 * Description: return TRUE if the element pointed by ptrData is dirty
 * Parameters: [IN] ptrData - pointer to data to be checked
 * Return: TRUE if the element is dirty, FALSE otherwise
 ******************************************************************************/
bool_t __NvIsDataSetDirty(void *ptrData)
{
    NVM_TableEntryInfo_t tblIdx;
    uint16_t             tableEntryIdx;
    uint16_t             loopIdx;
    uint16_t             remaining_count;
    bool_t               ret = FALSE;

    if (NULL != ptrData)
    {
        if (gNVM_OK_c == NvGetTableEntryIndexFromDataPtr(ptrData, &tblIdx, &tableEntryIdx))
        {
            /* Check if is in pending queue */
            if (mNvPendingSavesQueue.EntriesCount != 0U)
            {
                /* Start from the queue's head */
                loopIdx         = mNvPendingSavesQueue.Head;
                remaining_count = mNvPendingSavesQueue.EntriesCount;

                while (remaining_count != 0U)
                {
                    if (mNvPendingSavesQueue.QData[loopIdx].entryId == tblIdx.entryId)
                    {
                        ret = TRUE;
                        break;
                    }
                    remaining_count--;
                    /* increment and wrap the loop index */
                    if (++loopIdx >= (uint8_t)gNvPendingSavesQueueSize_c)
                    {
                        loopIdx = 0U;
                    }
                }
            }
            if (FALSE == ret)
            {
                ret = maDatasetInfo[tableEntryIdx].saveNextInterval;
            }
        }
    }

    return ret;
}
/******************************************************************************
 * Name: __NvRestoreDataSet
 * Description: copy the most recent version of the element/table entry pointed
 *              by ptrData from NVM storage system to RAM memory
 * Parameter(s): [IN] ptrData - pointer to data (element) to be restored
 *               [IN] restoreAll - if FALSE restores a single element
 *                               - if TRUE restores an entire table entry
 * Return: status of the restore operation
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvRestoreDataSet(void *ptrData, bool_t restoreAll)
{
    NVM_TableEntryInfo_t tblIdx;
#if gUnmirroredFeatureSet_d
    uint16_t tableEntryIdx;
#endif
    NVM_Status_t nvmStatus;

    do
    {
        if (NULL == ptrData)
        {
            nvmStatus = gNVM_NullPointer_c;
            FSCI_NV_RESTORE_MONITOR(0, TRUE, nvmStatus);
            break;
        }
#if gNvFragmentation_Enabled_d
        tblIdx.op_type = restoreAll ? OP_SAVE_ALL : OP_SAVE_SINGLE;
#else
        tblIdx.op_type = OP_SAVE_ALL;
#endif /* gNvFragmentation_Enabled_d */
        /* get table and element indexes */
        if (gNVM_OK_c != NvGetEntryFromDataPtr(ptrData, &tblIdx))
        {
            nvmStatus = gNVM_PointerOutOfRange_c;
            FSCI_NV_RESTORE_MONITOR(tblIdx.entryId, TRUE, nvmStatus);
            break;
        }
#if gUnmirroredFeatureSet_d
        /*make sure you can't request a full backup for unmirrored data sets*/
        tableEntryIdx = NvGetTableEntryIndexFromId(tblIdx.entryId);

        assert(gNvInvalidTableEntryIndex_c != tableEntryIdx);

        if (pNVM_DataTable[tableEntryIdx].DataEntryType != (uint16_t)gNVM_MirroredInRam_c)
        {
            tblIdx.op_type = OP_SAVE_SINGLE;
        }
#endif

        /* Do Nv Restore Data */
        FSCI_NV_RESTORE_MONITOR(tblIdx.entryId, TRUE, gNVM_OK_c);
        nvmStatus = NvRestoreData(&tblIdx);
        FSCI_NV_RESTORE_MONITOR(tblIdx.entryId, FALSE, nvmStatus);
    } while (FALSE);
    return nvmStatus;
}

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
/******************************************************************************
 * Name: __NvTimerTick
 * Description: Called from the idle task to process save-on-interval requests
 * Parameters: [IN] countTick - enable/disable tick count
 * Return: FALSE if the timer tick counters for all data sets have reached
 *         zero. In this case, the timer can be turned off.
 *         TRUE if any of the data sets' timer tick counters have not yet
 *         counted down to zero. In this case, the timer should be active
 ******************************************************************************/
NVM_STATIC bool_t __NvTimerTick(bool_t countTick)
{
    bool_t               fTicksLeft = FALSE;
    NVM_TableEntryInfo_t tblIdx;
    uint16_t             idx = 0U;

    while (idx < mNVM_DataTableNbEntries)
    {
        if (countTick)
        {
            if (maDatasetInfo[idx].ticksToNextSave != 0U)
            {
                --maDatasetInfo[idx].ticksToNextSave;
            }
        }

        if (maDatasetInfo[idx].saveNextInterval)
        {
            if (maDatasetInfo[idx].ticksToNextSave != 0U)
            {
                fTicksLeft = TRUE;
            }
            else
            {
                tblIdx.entryId = pNVM_DataTable[idx].DataEntryID;
#if gUnmirroredFeatureSet_d
                if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[idx].DataEntryType)
                {
                    tblIdx.elementIndex = maDatasetInfo[idx].elementIndex;
                    tblIdx.op_type      = OP_SAVE_SINGLE;
                }
                else
#endif
                {
                    tblIdx.elementIndex = 0U;
                    tblIdx.op_type      = OP_SAVE_ALL;
                }
                maDatasetInfo[idx].saveNextInterval = FALSE;
                if (mNvCriticalSectionFlag == 0U)
                {
                    if (NvWriteRecord(&tblIdx) == gNVM_PageCopyPending_c)
                    {
                        /* retry next time we have a tick */
                        if (NvAddSaveRequestToQueue(&tblIdx) == gNVM_SaveRequestRejected_c)
                        {
                            maDatasetInfo[idx].saveNextInterval = TRUE;
                        }
                    }
                }
                else
                {
                    /* retry next time we have a tick */
                    if (NvAddSaveRequestToQueue(&tblIdx) == gNVM_SaveRequestRejected_c)
                    {
                        maDatasetInfo[idx].saveNextInterval = TRUE;
                    }
                }
            }
        }

        /* increment the loop counter */
        idx++;
    }

    return fTicksLeft;
} /* NvTimerTick() */
#endif

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnIdleCount_c)
/******************************************************************************
 * Name: __NvSaveOnCount
 * Description: Decrement the counter. Once it reaches 0, the next call to
 *              NvIdle() will save the entire table entry (all elements).
 * Parameters: [IN] ptrData - pointer to data to be saved
 * Return: NVM_OK_c - if operation completed successfully
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_STATIC NVM_Status_t __NvSaveOnCount(void *ptrData)
{
    NVM_Status_t         status;
    NVM_TableEntryInfo_t tblIdx;
    uint16_t             tableEntryIdx;

    do
    {
        if (NULL == ptrData)
        {
            status = gNVM_NullPointer_c;
            break;
        }

        /* get the NVM table entry */
        status = NvGetTableEntryIndexFromDataPtr(ptrData, &tblIdx, &tableEntryIdx);
        if (gNVM_OK_c != status)
        {
            break;
        }
        if (maDatasetInfo[tableEntryIdx].countsToNextSave != 0U)
        {
            --maDatasetInfo[tableEntryIdx].countsToNextSave;
        }
        else
        {
            /* all the elements of the NVM table entry will be saved */
            tblIdx.op_type                                = OP_SAVE_ALL;
            maDatasetInfo[tableEntryIdx].countsToNextSave = mNvCountsBetweenSaves;
            status                                        = NvAddSaveRequestToQueue(&tblIdx);
        }
    } while (FALSE);

    return status;
} /* NvSaveOnCount() */
#endif

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
/******************************************************************************
 * Name: __NvSaveOnInterval
 * Description:  save no more often than a given time interval. If it has
 *               been at least that long since the last save,
 *               this function will cause a save the next time the idle
 *               task runs.
 * Parameters: [IN] ptrData - pointer to data to be saved
 * NOTE: this function saves all the element of the table entry pointed by
 *       ptrData
 * Return: NVM_OK_c - if operation completed successfully
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_STATIC NVM_Status_t __NvSaveOnInterval(void *ptrData)
{
    NVM_Status_t         status;
    NVM_TableEntryInfo_t tblIdx;
    uint16_t             tableEntryIdx;

    do
    {
        if (NULL == ptrData)
        {
            status = gNVM_NullPointer_c;
            break;
        }
        /* get the NVM table entry */
        status = NvGetTableEntryIndexFromDataPtr(ptrData, &tblIdx, &tableEntryIdx);
        if (status != gNVM_OK_c)
        {
            break;
        }

        if (maDatasetInfo[tableEntryIdx].saveNextInterval == FALSE)
        {
            maDatasetInfo[tableEntryIdx].ticksToNextSave  = mNvMinimumTicksBetweenSaves;
            maDatasetInfo[tableEntryIdx].saveNextInterval = TRUE;
#if gUnmirroredFeatureSet_d
            if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[tableEntryIdx].DataEntryType)
            {
                maDatasetInfo[tableEntryIdx].elementIndex = tblIdx.elementIndex;
            }
#endif
            mNvSaveOnIntervalEvent = TRUE;
            mNvLastTimestampValue  = TM_GetTimestamp();
        }

    } while (FALSE);

    return status;
}
#endif

/******************************************************************************
 * Name: __NvSaveOnIdle
 * Description: Save the data pointed by ptrData on the next call to NvIdle()
 * Parameter(s): [IN] ptrData - pointer to data to be saved
 *               [IN] saveAll - specify if all the elements from the NVM table
 *                              entry shall be saved
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_Error_c - in case of error(s)
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_STATIC NVM_Status_t __NvSaveOnIdle(void *ptrData, bool_t saveAll)
{
    NVM_Status_t status;

    do
    {
        NVM_TableEntryInfo_t tblIdx;

        if (NULL == ptrData)
        {
            status = gNVM_NullPointer_c;
            break;
        }

        /* get the NVM table entry */
        status = NvGetTableEntryIndexFromDataPtr(ptrData, &tblIdx, NULL);
        if (status != gNVM_OK_c)
        {
            break;
        }
        /* write the save all flag */
#if gNvFragmentation_Enabled_d
        tblIdx.op_type = saveAll ? OP_SAVE_ALL : OP_SAVE_SINGLE;
#else
        tblIdx.op_type = OP_SAVE_ALL;
#endif /* gNvFragmentation_Enabled_d */

        status = NvAddSaveRequestToQueue(&tblIdx);

    } while (FALSE);

    return status;
}

/******************************************************************************
 * Name: NvModuleSwitchPage
 * Description: Perform page copy operation to switch between NVM virtual pages.
 *
 * Parameters [IN] skipEntryId entry ID to be skipped during the page copy operation -
 *                 gNvCopyAll_c is all must be taken
 *
 * Return: gNVM_EccFault_c - if an ECC fault is raised during the page copy, need to try again
 *         gNVM_OK_c - page copy operation was successful
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvModuleSwitchPage(NvTableEntryId_t skipEntryId)
{
    NVM_Status_t status = gNVM_OK_c;
    FSCI_NV_VIRT_PAGE_MONITOR(TRUE, gNVM_OK_c);
    status = NvCopyPage(skipEntryId);
#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
    if (gNVM_EccFault_c == status)
    {
        status = NvCopyPage(gNvCopyAll_c);
    }
#endif /* gNvSalvageFromEccFault_d */
    FSCI_NV_VIRT_PAGE_MONITOR(FALSE, status);
    return status;
}
/******************************************************************************
 * Name: __NvModulePostInit
 * Description: Do post Initialize the NV storage module when format failure
 * Parameter(s):
 * Return: gNVM_ModuleAlreadyInitialized_c - if the module is already
 *                                           initialized
 *         gNVM_InvalidSectorsCount_c - if the sector count configured in the
 *                                      project linker file is invalid
 *         gNVM_MetaNotFound_c - if no meta information was found
 *         gNVM_OK_c - module was successfully initialized
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvModulePostInit(void)
{
    NVM_Status_t status = gNVM_OK_c;
    uint32_t     pageFreeSpace;
    /* get the last meta information address */
    status = NvUpdateLastMetaInfoAddress();
    /* NvUpdateLastMetaInfoAddress may return no error but the top most MIT may still be invalid .
     * This will be detected when NvGetPageFreeSpace returns an error.
     */
    if (gNVM_OK_c == status)
    {
        /* NVM module is now initialized */
        mNvModuleInitialized = TRUE;

        /* get active page free space */
        status = NvGetPageFreeSpace(&pageFreeSpace, TRUE);
        if (gNVM_OK_c == status)
        {
            if (pageFreeSpace < gNvMinimumFreeBytesCountStart_c)
            {
                status = NvModuleSwitchPage(gNvCopyAll_c);
            }
#if gUnmirroredFeatureSet_d
            __NvmRestoreUnmirrored();
#endif
        }
        else
        {
            /* copy page  */
            status = NvModuleSwitchPage(gNvCopyAll_c);
            if (gNVM_OK_c != status)
            {
                mNvModuleInitialized = FALSE;

                status = NvInternalFormat(0U);
            }
        }
    }
    else
    {
        /* copy page  */
        status = NvModuleSwitchPage(gNvCopyAll_c);
    }
    return status;
}

/******************************************************************************
 * Name: __NvModuleInit
 * Description: Initialize the NV storage module
 * Parameter(s): [IN] flashInit - need to Initialize flash adapter
 * Return: gNVM_ModuleAlreadyInitialized_c - if the module is already
 *                                           initialized
 *         gNVM_InvalidSectorsCount_c - if the sector count configured in the
 *                                      project linker file is invalid
 *         gNVM_MetaNotFound_c - if no meta information was found
 *         gNVM_OK_c - module was successfully initialized
 *****************************************************************************/
#if defined(__IAR_SYSTEMS_ICC__)
#pragma required = eraseNVMFirst
#pragma required = eraseNVMSecond
#endif /* __IAR_SYSTEMS_ICC__ */
NVM_STATIC NVM_Status_t __NvModuleInit(bool_t flashInit)
{
    uint16_t     loopCnt;
    NVM_Status_t status            = gNVM_OK_c;
    uint32_t     flashEstimateSize = 0U;
    uint32_t     pageFreeSpace     = 0U;

    if (mNVM_DataTableNbEntries == 0U)
    {
        /* If deinit was applied need to reset to normal value deduced from NVM_TABLE section size.
           Might be made to point explicitly on an alternate dataset so test if initialized.
        */
        mNVM_DataTableNbEntries = gNVM_TABLE_entries_c;
    }
#if gNvUseExtendedFeatureSet_d
    bool_t ret = FALSE;
#endif
#if defined(__IAR_SYSTEMS_ICC__)
    (void)eraseNVMFirst;
    (void)eraseNVMSecond;
#endif
    if ((mNVM_DataTableNbEntries == 0U) || (mNVM_DataTableNbEntries >= gNvTableEntriesCountMax_c))
    {
        status = gNVM_InvalidTableEntriesCount_c;
    }
    else
    {
#if (gNvDualImageSupport_d)
        FLib_MemSet(&mNvDiffEntryId[0], 0xffU, gNvTableEntriesCountMax_c * sizeof(mNvDiffEntryId[0]));
#endif

#if (gNvFragmentation_Enabled_d == TRUE)
        for (loopCnt = 0U; loopCnt < mNVM_DataTableNbEntries; loopCnt++)
        {
            if (pNVM_DataTable[loopCnt].ElementsCount > (uint16_t)gNvRecordsCopiedBufferSize_c)
            {
                status = gNVM_DefragBufferTooSmall_c;
                break;
            }
        }
        if (gNVM_OK_c == status)
#endif
        {
            /* Initialize the pending saves queue */
            NvInitPendingSavesQueue();

            /* Initialize the data set info table */
            for (loopCnt = 0U; loopCnt < (uint16_t)gNvTableEntriesCountMax_c; loopCnt++)
            {
                maDatasetInfo[loopCnt].saveNextInterval = FALSE;
                maDatasetInfo[loopCnt].countsToNextSave = mNvCountsBetweenSaves;
            }

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
            /* initialize the event used by save-on-interval functionality */
            mNvSaveOnIntervalEvent = FALSE;
#endif
            /* check linker file symbol definition for sector count; it should be multiple of 2 */
            if ((((uint32_t)NV_STORAGE_MAX_SECTORS) & 0x1U) != 0U)
            {
                status = gNVM_InvalidSectorsCount_c;
            }
            else
            {
                /* Init the NVM configuration */
                InitNVMConfig();

                /* both pages are not valid, format the NV storage system */
                if (mNvActivePageId == gVirtualPageNone_c)
                {
                    /* CERT INT30-C: guard gNvFirstMetaOffset_c does not exceed MIN(mNvTotalPageSize, UINT16_MAX) */
                    uint32_t minVal  = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
                    uint32_t firstMO = gNvFirstMetaOffset_c;
                    assert(minVal >= firstMO);
                    uint32_t max_val = (minVal >= firstMO) ? (minVal - firstMO) : 0U;

                    mNvActivePageId = gFirstVirtualPage_c;
#if gNvDualImageSupport_d
                    (void)NvGetEntryInfoNeedToAddInNVM();
#endif /* gNvDualImageSupport_d */
                    status = NvInternalFormat(0U);
                    (void)NvGetPageFreeSpace(&pageFreeSpace, TRUE);
                    for (loopCnt = 0U; loopCnt < mNVM_DataTableNbEntries; loopCnt++)
                    {
                        uint16_t sz_per_elt          = NvUpdateSize(pNVM_DataTable[loopCnt].ElementSize);
                        uint16_t elt_total_footprint = 0U;
                        if (sz_per_elt < (((uint32_t)UINT16_MAX) - (uint32_t)sizeof(NVM_RecordMetaInfo_t)))
                        {
                            sz_per_elt += (uint16_t)sizeof(NVM_RecordMetaInfo_t); /* record data plus MIT sizes */
                            /* CERT INT30-C / MISRA 10.3: sz_per_elt fits in uint16_t after the above guard */
                            if (NvMultEltSzByNb(pNVM_DataTable[loopCnt].ElementsCount, sz_per_elt, max_val,
                                                &elt_total_footprint) < 0)
                            {
                                assert(false);
                                status = gNVM_Error_c;
                            }
                            flashEstimateSize += (uint32_t)elt_total_footprint;
                        }
                        else
                        {
                            assert(false); /* If NvUpdateSize returned 0xFFFF it is caught here */
                            status = gNVM_Error_c;
                        }
                    }
                    if (pageFreeSpace < (flashEstimateSize + gNvMinimumFreeBytesCountStart_c))
                    {
                        assert(false);
                        /* Estimated Flash buffer is too small, Need increase the gNVMSectorCountLink_d */
                        status = gNVM_ReservedFlashTooSmall_c;
                    }
                }
                if (gNVM_OK_c == status)
                {
#if gNvUseExtendedFeatureSet_d
                    /* get the size of the NV table stored in FLASH memory */
                    mNvTableSizeInFlash = NvGetFlashTableSize();

                    if (0U == mNvTableSizeInFlash) /* no NV table found in FLASH, format the system */
                    {
#if gNvDualImageSupport_d
                        (void)NvGetEntryInfoNeedToAddInNVM();
#endif                                                 /* gNvDualImageSupport_d */
                        status = NvInternalFormat(0U); /* will also save the NV table to FLASH memory */
                        if (status != gNVM_OK_c)
                        {
                            ret = TRUE;
                        }
                    }
                    else /* found a valid NV table in FLASH memory */
                    {
                        /* check if the RAM table was updated (e.g. new binary image via OTA) */
                        mNvTableUpdated = (GetFlashTableVersion() != mNvFlashTableVersion) || NvIsRamTableUpdated();
#if gNvDualImageSupport_d
                        if (NvGetEntryInfoNeedToAddInNVM() != 0UL)
                        {
                            mNvTableUpdated = TRUE;
                        }
#endif /* gNvDualImageSupport_d */
                        if (mNvTableUpdated)
                        {
                            ret = TRUE;
                            if (gNVM_OK_c == NvUpdateLastMetaInfoAddress())
                            {
                                /* copy the new RAM table and the page content */
                                status          = NvModuleSwitchPage(gNvCopyAll_c);
                                mNvTableUpdated = FALSE;
                                if (gNVM_OK_c == status)
                                {
                                    /* NVM module is now initialised */
                                    mNvModuleInitialized = TRUE;
                                }
                                else
                                {
                                    mNvModuleInitialized = FALSE;
                                }
#if gUnmirroredFeatureSet_d
                                __NvmRestoreUnmirrored();
#endif
                            }
                            else
                            {
                                /* format the system */
                                status = NvInternalFormat(0U);
                            }
                        }
                    }
                    if (FALSE == ret)
#endif /* gNvUseExtendedFeatureSet_d */
                    {
                        status = __NvModulePostInit();
                    }
                }
            }
        }
    }
    return status;
}

#if gUnmirroredFeatureSet_d

/******************************************************************************
 * Name: NvIsRecordErased
 * Description: Checks the most recent metas to see if the un-mirrored element
 *              was erased or is just uninitialized
 * Parameter(s): [IN] srcTableEntryIdx - the index of the entry to be checked
 *               [IN] srcTableEntryElementIdx - the element index
 *               [IN] srcMetaOffset - the starting offset of the search
 * Return: TRUE if the element was erased with NvErase or FALSE otherwise
 *****************************************************************************/
NVM_STATIC bool_t NvIsRecordErased(uint16_t srcTableEntryIdx, uint16_t srcTableEntryElementIdx, uint32_t srcMetaOffset)
{
    bool_t               status      = FALSE;
    NVM_RecordMetaInfo_t srcMetaInfo = {0U};
    NVM_Status_t         st          = gNVM_OK_c;

    uint32_t firstSrcMetaOffset = srcMetaOffset;

    while (srcMetaOffset < mNvTotalPageSize)
    {
        st = NvGetMetaInfo(mNvActivePageId, srcMetaOffset, &srcMetaInfo);
        if (st == gNVM_MetaInfoBlank_c)
        {
            /* means that the meta data contained the guard value */
            break;
        }
        if (st == gNVM_OK_c)
        {
            /* skip invalid contents */
            if ((firstSrcMetaOffset != srcMetaOffset) &&
                (srcMetaInfo.u.fields.NvmElementIndex == srcTableEntryElementIdx) &&
                (srcMetaInfo.u.fields.NvmDataEntryID == pNVM_DataTable[srcTableEntryIdx].DataEntryID))
            {
                status = TRUE;
                break;
            }

            if ((srcMetaInfo.u.fields.NvmRecordOffset == 0U) &&
                (srcMetaInfo.u.fields.NvmElementIndex == srcTableEntryElementIdx) &&
                (srcMetaInfo.u.fields.NvmDataEntryID == pNVM_DataTable[srcTableEntryIdx].DataEntryID))
            {
                status = TRUE;
                break;
            }
        }
        srcMetaOffset += sizeof(NVM_RecordMetaInfo_t);
    }
    return status;
}

/******************************************************************************
 * Name: __NvmRestoreUnmirrored
 * Description: Restores all unmirrored entries that should be restored at init
 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void __NvmRestoreUnmirrored(void)
{
    uint32_t             metaInfoOffset;
    uint16_t             tableEntryIdx;
    NVM_RecordMetaInfo_t metaInfo = {0U};
    uint16_t             loopCnt  = 0U;
    uint16_t             loopCnt2;
    const uint32_t       erasedElement = 0xFFFFFFFFU;
    NVM_Status_t         status        = gNVM_OK_c;

    /* get the last meta information address */
    metaInfoOffset = mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset;
    if (metaInfoOffset != gNvInvalidMetaOffset_c)
    {
        /* If metaInfoOffset has been initialized it is necessarily greater than gNvFirstMetaOffset_c */
        /* parse meta info backwards until the element is found */
        for (uint32_t meta_offs = metaInfoOffset; meta_offs >= gNvFirstMetaOffset_c;
             meta_offs -= sizeof(NVM_RecordMetaInfo_t))
        {
            metaInfoOffset = meta_offs;
            /* get the meta information */
            status = NvGetMetaInfo(mNvActivePageId, metaInfoOffset, &metaInfo);
            if (status == gNVM_MetaInfoBlank_c)
            {
                break;
            }
            if ((status != gNVM_OK_c) || (metaInfo.u.fields.NvValidationStartByte != gValidationByteSingleRecord_c))
            {
                /* skip invalid MIT entries but also Single records*/
                continue;
            }

            /* get table entry information */
            tableEntryIdx = NvGetTableEntryIndexFromId(metaInfo.u.fields.NvmDataEntryID);
            if ((gNvInvalidTableEntryIndex_c == tableEntryIdx) ||
                ((uint16_t)gNVM_NotMirroredInRamAutoRestore_c != pNVM_DataTable[tableEntryIdx].DataEntryType))
            {
                continue;
            }

            /* if it was already restored, ignore it */
            if (NvIsNVMFlashAddress(
                    ((void **)pNVM_DataTable[tableEntryIdx].pData)[metaInfo.u.fields.NvmElementIndex]) ||
                (erasedElement == (uint32_t)(uint32_t *)((void **)pNVM_DataTable[tableEntryIdx]
                                                             .pData)[metaInfo.u.fields.NvmElementIndex]))
            {
                continue;
            }

            /* erased element */
            if (metaInfo.u.fields.NvmRecordOffset == 0U)
            {
                ((void **)pNVM_DataTable[tableEntryIdx].pData)[metaInfo.u.fields.NvmElementIndex] =
                    (uint32_t *)erasedElement;
            }
            else
            {
                ((void **)pNVM_DataTable[tableEntryIdx].pData)[metaInfo.u.fields.NvmElementIndex] =
                    (void *)((uint32_t *)(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress +
                                          metaInfo.u.fields.NvmRecordOffset));
            }

            /* move to the previous meta info */
        } /* for */

        while (loopCnt < mNVM_DataTableNbEntries)
        {
            if ((uint16_t)gNVM_NotMirroredInRamAutoRestore_c == pNVM_DataTable[loopCnt].DataEntryType)
            {
                for (loopCnt2 = 0U; loopCnt2 < pNVM_DataTable[loopCnt].ElementsCount; loopCnt2++)
                {
                    if (erasedElement == (uint32_t)(uint32_t *)((void **)pNVM_DataTable[loopCnt].pData)[loopCnt2])
                    {
                        ((void **)pNVM_DataTable[loopCnt].pData)[loopCnt2] = NULL;
                    }
                }
            }
            /* increment the loop counter */
            loopCnt++;
        }
    }
}
/******************************************************************************
 * Name: __NvmMoveToRam
 * Description: Move from NVM to Ram an unmirrored dataset
 * Parameter(s):  ppData     double pointer to the entity to be moved from flash to RAM
 * Return: pointer to Ram location
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvmMoveToRam(void **ppData)
{
    NVM_TableEntryInfo_t tblIdx;
    uint16_t             tableEntryIndex;
    NVM_Status_t         status = gNVM_OK_c;
    void                *pData  = NULL;
    uint16_t             loopIdx;
    uint16_t             remaining_count;

    /* Get entry from NVM table */
    status = NvGetTableEntryIndexFromDataPtr(ppData, &tblIdx, &tableEntryIndex);
    if (gNVM_OK_c == status)
    {
        if ((uint16_t)gNVM_MirroredInRam_c == pNVM_DataTable[tableEntryIndex].DataEntryType)
        {
            status = gNVM_IsMirroredDataSet_c;
        }
        else
        {
            /* Check if entry is in ram  */
            if (!NvIsNVMFlashAddress(*ppData) && (*ppData != NULL))
            {
                /* Check if is in pending queue - if yes than remove it */
                if (NvIsPendingOperation())
                {
                    /* Start from the queue's head */
                    loopIdx         = mNvPendingSavesQueue.Head;
                    remaining_count = mNvPendingSavesQueue.EntriesCount;

                    while (remaining_count != 0U)
                    {
                        if ((tblIdx.entryId == mNvPendingSavesQueue.QData[loopIdx].entryId) &&
                            (tblIdx.elementIndex == mNvPendingSavesQueue.QData[loopIdx].elementIndex))
                        {
                            mNvPendingSavesQueue.QData[loopIdx].entryId = gNvInvalidDataEntry_c;
                            break;
                        }
                        remaining_count--;
                        /* increment and wrap the loop index */
                        if (++loopIdx >= (uint8_t)gNvPendingSavesQueueSize_c)
                        {
                            loopIdx = 0U;
                        }
                    }
                }
                maDatasetInfo[tableEntryIndex].saveNextInterval = FALSE;
                status                                          = gNVM_OK_c;
            }
            else
            {
                /* Allocate a buffer for the data set */
                pData = MEM_BufferAllocWithId(pNVM_DataTable[tableEntryIndex].ElementSize, gNvmMemPoolId_c);
                if (pData == NULL)
                {
                    status = gNVM_NoMemory_c;
                }
                else
                {
                    /* Write from Flash to Ram */
                    if (*ppData != NULL)
                    {
                        FLib_MemCpy(pData, *ppData, pNVM_DataTable[tableEntryIndex].ElementSize);
                    }

                    OSA_InterruptDisable();
                    *ppData = pData;
                    OSA_InterruptEnable();
                    /* Check if the address is in ram */
                    status = gNVM_OK_c;
                }
            }
        }
    }
    return status;
}

/******************************************************************************
 * Name: __NvmErase
 * Description: Erase from NVM an unmirrored dataset
 * Parameter(s):  ppData     double pointer to the entity to be erased
 * Return: pointer to Ram location
 *****************************************************************************/
NVM_STATIC NVM_Status_t __NvmErase(void **ppData)
{
    NVM_Status_t         status;
    NVM_TableEntryInfo_t tblIdx;
    uint16_t             tableEntryIndex;
    uint16_t             loopCnt;
    uint16_t             remaining_count;

    /* Get entry from NVM table */
    status = NvGetTableEntryIndexFromDataPtr(ppData, &tblIdx, &tableEntryIndex);
    if (gNVM_OK_c == status)
    {
        if ((uint16_t)gNVM_MirroredInRam_c == pNVM_DataTable[tableEntryIndex].DataEntryType)
        {
            status = gNVM_IsMirroredDataSet_c;
        }
        else
        {
            if (!NvIsNVMFlashAddress(*ppData))
            {
                if (*ppData != NULL)
                {
                    (void)MEM_BufferFree(*ppData);
                }
                OSA_InterruptDisable();
                *ppData = NULL;
                OSA_InterruptEnable();
                status = gNVM_OK_c;
            }
            else
            {
                /* Check if is in pending queue - if yes than remove it */
                if (NvIsPendingOperation())
                {
                    /* Start from the queue's head */
                    loopCnt         = mNvPendingSavesQueue.Head;
                    remaining_count = mNvPendingSavesQueue.EntriesCount;
                    while (remaining_count != 0U)
                    {
                        /* if the element is waiting to be saved, cancel the save */
                        if ((tblIdx.entryId == mNvPendingSavesQueue.QData[loopCnt].entryId) &&
                            (tblIdx.elementIndex == mNvPendingSavesQueue.QData[loopCnt].elementIndex))
                        {
                            mNvPendingSavesQueue.QData[loopCnt].entryId = gNvInvalidDataEntry_c;
                        }
                        remaining_count--;
                        /* increment and wrap the loop index */
                        if (++loopCnt >= (uint8_t)gNvPendingSavesQueueSize_c)
                        {
                            loopCnt = 0U;
                        }
                    }
                }
                OSA_InterruptDisable();
                *ppData = NULL;
                OSA_InterruptEnable();
                status = __NvSyncSave(ppData, FALSE);
            }
        }
    }
    return status;
}

/******************************************************************************
 * Name: NvIsNVMFlashAddress
 * Description: check if the address is in Flash
 * Parameter(s): [IN] address
 *
 * Return: TRUE if the table entry is in Flash / FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvIsNVMFlashAddress(void *address)
{
    bool_t  status = FALSE;
    uint8_t idx;
    for (idx = 0U; idx < gNvVirtualPagesCount_c; idx++)
    {
        if (((uint32_t)((uint32_t *)address) > mNvVirtualPageProperty[idx].NvRawSectorStartAddress) &&
            ((uint32_t)((uint32_t *)address) < mNvVirtualPageProperty[idx].NvRawSectorEndAddress))
        {
            status = TRUE;
            break;
        }
    }
    return status;
}
#endif

/******************************************************************************
 * Name: NvInitPendingSavesQueue
 * Description: Initialize the pending saves queue
 * Parameters: none
 * Return: none
 ******************************************************************************/
NVM_STATIC void NvInitPendingSavesQueue(void)
{
    mNvPendingSavesQueue.Head         = 0U;
    mNvPendingSavesQueue.Tail         = 0U;
    mNvPendingSavesQueue.EntriesCount = 0U;
}

/******************************************************************************
 * Name: NvPushPendingSave
 * Description: Add a new pending save to the queue
 * Parameters: [IN] data - data to be saved
 * Return: TRUE if the push operation succeeded, FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvPushPendingSave(NVM_TableEntryInfo_t data)
{
    bool_t status = FALSE;

    /* Can only add to queue if at least one slot is remaining */
    if (mNvPendingSavesQueue.EntriesCount < (uint16_t)(gNvPendingSavesQueueSize_c))
    {
        uint16_t tail_idx = mNvPendingSavesQueue.Tail;
        /* Add the item to queue */
        mNvPendingSavesQueue.QData[tail_idx] = data;
        /* Increment and wrap the tail when it reaches gNvPendingSavesQueueSize_c */
        INCREMENT_Q_INDEX(tail_idx);
        mNvPendingSavesQueue.Tail = tail_idx;

        /* Increment the entries count */
        mNvPendingSavesQueue.EntriesCount++;
        status = TRUE;
    }

    return status;
}

/******************************************************************************
 * Name: NvGetPendingSaveHead
 * Description: Retrieves the head element from the pending saves queue
 * Parameters: [IN] pQueue - pointer to queue
 *             [OUT] pData - pointer to the location where data will be placed
 * Return: TRUE if the pop operation succeeded, FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvGetPendingSaveHead(NVM_TableEntryInfo_t *pData)
{
    bool_t status = FALSE;
    assert(pData != NULL);
    if (mNvPendingSavesQueue.EntriesCount != 0U)
    {
        *pData = mNvPendingSavesQueue.QData[mNvPendingSavesQueue.Head];
        status = TRUE;
    }
    return status;
}

NVM_STATIC void NvRemovePendingSaveHead(void)
{
    if (mNvPendingSavesQueue.EntriesCount > 0u)
    {
        /* Increment and wrap the head when it reaches gNvPendingSavesQueueSize_c */
        INCREMENT_Q_INDEX(mNvPendingSavesQueue.Head);

        /* Decrement the entries count */
        mNvPendingSavesQueue.EntriesCount--;
    }
}

/******************************************************************************
 * Name: NvPopPendingSave
 * Description: Retrieves the head element from the pending saves queue
 * Parameters: [OUT] pData - pointer to the location where data will be placed
 * Return: TRUE if the pop operation succeeded, FALSE otherwise
 ******************************************************************************/
NVM_STATIC bool_t NvPopPendingSave(NVM_TableEntryInfo_t *pData)
{
    bool_t status;

    status = NvGetPendingSaveHead(pData);

    if (status == TRUE)
    {
        /* Update Head index to consume head */
        NvRemovePendingSaveHead();
    }
    return status;
}

/******************************************************************************
 * Name: NvGetPendingSavesCount
 * Description: self explanatory
 * Parameters: none
 * Return: Number of pending saves
 ******************************************************************************/
NVM_STATIC uint16_t NvGetPendingSavesCount(void)
{
    /* Called from context where pQueue is well controlled */
    return mNvPendingSavesQueue.EntriesCount;
}

/******************************************************************************
 * Name: NvLookAheadInPendingSaveQueue
 * Description: Retrieves the head element from the pending saves queue
 * Parameters: [IN] searched_id - entry Id
 *             [IN] searched_index
 * Return: OP_SAVE_SINGLE or OP_SAVE_ALL if the element was found, OP_NONE otherwise
 ******************************************************************************/
NVM_STATIC uint8_t NvLookAheadInPendingSaveQueue(uint16_t searched_id, uint16_t searched_index)
{
    eNvFlashOp_t found = OP_NONE;
    if (mNvPendingSavesQueue.EntriesCount != 0U)
    {
        uint16_t i = mNvPendingSavesQueue.Head;
        while (i != mNvPendingSavesQueue.Tail)
        {
            /* Parse the pending save queue looking for elements whose id is searched_id */
            NVM_TableEntryInfo_t *elm = &mNvPendingSavesQueue.QData[i];
            if (elm->entryId == searched_id)
            {
                if ((elm->op_type == OP_SAVE_ALL) ||
                    ((elm->elementIndex == searched_index) && (elm->op_type == OP_SAVE_SINGLE)))
                {
                    found = elm->op_type;
                    break;
                }
            }
            INCREMENT_Q_INDEX(i);
        }
    }

    return (uint8_t)found;
}

/******************************************************************************
 * Name: InitNVMConfig
 * Description: Initialises the hal driver, and gets the active page.
 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void InitNVMConfig(void)
{
    if (FALSE == mNvFlashConfigInitialised)
    {
        uint32_t start_addr;
        uint32_t partition_size;
        /* Initialize flash HAL driver */
        if (kStatus_HAL_Flash_Success != HAL_FlashInit())
        {
            return;
        }
        /* no pending erase operations on system initialisation */
        mNvErasePgCmdStatus.NvErasePending = FALSE;

        /* Initialize the active page ID */
        mNvActivePageId = gVirtualPageNone_c;
        Nv_GetPartitionAddressAndSize(&start_addr, &partition_size);
        if (start_addr == 0U || partition_size == 0U)
        {
            return;
        }
        mNvTotalPageSize = (uint32_t)(partition_size / 2U);

        for (uint8_t pageID = (uint8_t)gFirstVirtualPage_c; pageID < gVirtualPageNb_c; pageID++)
        {
            NVM_VirtualPageProperties_t *page_props = &mNvVirtualPageProperty[pageID];
            page_props->NvRawSectorStartAddress     = start_addr;
            start_addr += mNvTotalPageSize;
            page_props->NvRawSectorEndAddress = start_addr - 1U;
            page_props->has_ecc_faults        = FALSE;
#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
            {
                uint32_t fault_at = 0U;
                fault_at          = NV_SweepRangeForEccFaults(page_props->NvRawSectorStartAddress, mNvTotalPageSize);
                if (fault_at != 0U)
                {
                    page_props->has_ecc_faults = TRUE;
                }
            }
#endif
        }

        NvInitStorageSystem();
#if gNvUseExtendedFeatureSet_d
        if (mNvActivePageId != gVirtualPageNone_c)
        {
            mNvTableSizeInFlash = NvGetFlashTableSize();
        }
#endif
        mNvFlashConfigInitialised = TRUE;
    }
}

/******************************************************************************
 * Name: NvUpdateSize
 * Description: Updates the size to be a multiple of the flash controller
 * phrase size (4, 8 or 16)
 * Parameter(s): [IN] size - size to be updated
 * Return: the computed size, 0xFFFFU in case of overflow
 *****************************************************************************/
NVM_STATIC uint16_t NvUpdateSize(uint16_t size)
{
    /* compute the size that will be actually written on FLASH memory */
    uint16_t paddingBytes = (uint16_t)(size % (uint8_t)PGM_SIZE_BYTE);
    uint16_t sz_increment;
    uint32_t max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
    if (paddingBytes != 0U)
    {
        sz_increment = ((uint16_t)PGM_SIZE_BYTE - paddingBytes);
        if (size <= (max_val - sz_increment))
        {
            size += sz_increment;
        }
        else
        {
            size = 0xFFFFU;
        }
    }

    return size;
}

/******************************************************************************
 * Name: NvEraseVirtualPage
 * Description: erase the specified page
 * Parameter(s): [IN] pageID - the ID of the page to be erased
 * Return: gNVM_InvalidPageID_c - if the page ID is not valid
 *         gNVM_SectorEraseFail_c - if the page cannot be erased
 *         gNVM_OK_c - if operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvEraseVirtualPage(NVM_VirtualPageID_t pageID)
{
    NVM_Status_t status = gNVM_OK_c;

    if (pageID > gSecondVirtualPage_c)
    {
        status = gNVM_InvalidPageID_c;
    }
    else
    {
        /* Blank check first */
        status = NvVirtualPageBlankCheck(pageID);
        if (gNVM_OK_c != status)
        {
            /* If already blank avoid unrequired erase */
            /* erase virtual page */
            if (kStatus_HAL_Flash_Success !=
                HAL_FlashEraseSector(mNvVirtualPageProperty[pageID].NvRawSectorStartAddress, mNvTotalPageSize))
            {
                status = gNVM_SectorEraseFail_c;
            }
            else
            {
                status = NvVirtualPageBlankCheck(pageID);
            }
        }
        /* After erase ECC errors got cleaned */
        mNvVirtualPageProperty[pageID].has_ecc_faults = FALSE; /* erase virtual page */
        FSCI_NV_VIRT_PAGE_ERASE_MONITOR(mNvVirtualPageProperty[pageID].NvRawSectorStartAddress, status);
    }
    return status;
}
/******************************************************************************
 * Name: NvSetErasePgCmdStatus
 * Description: Nv Set Erase Page CmdStatus. Sets mNvActivePageId
 * Parameter(s): [IN] PageToErase - the ID of the page to be erased
 * Parameter(s): [IN] doPageBlankCheck - need do page blank check
 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void NvSetErasePgCmdStatus(NVM_VirtualPageID_t PageToErase, bool_t doPageBlankCheck)
{
    bool_t req_erase = TRUE;
    mNvActivePageId  = (PageToErase == gSecondVirtualPage_c) ? gFirstVirtualPage_c : gSecondVirtualPage_c;

    if (doPageBlankCheck)
    {
        if (gNVM_PageIsNotBlank_c != NvVirtualPageBlankCheck(PageToErase))
        {
            /* Already done */
            req_erase = FALSE;
        }
    }
    if (req_erase)
    {
        /* request the erase of the  page */
        mNvErasePgCmdStatus.NvPageToErase  = PageToErase;
        mNvErasePgCmdStatus.NvSectorIndex  = 0U;
        mNvErasePgCmdStatus.NvErasePending = TRUE;
    }
}

NVM_STATIC NVM_Status_t ReadPageTopCount(NVM_VirtualPageProperties_t *page_prop, uint32_t *top_count)
{
    *top_count = gPageCounterMaxValue_c;
    /* Avoid direct read to flash in case of ECC fault */
    return NV_FlashRead(page_prop->NvRawSectorStartAddress, (uint8_t *)top_count, sizeof(*top_count), TRUE);
}

/*
 * Erase virtual pages whose top count is unset / or not readable.
 */
NVM_STATIC void NvPostFwUpdateMaintenance(void)
{
    uint8_t pageID;

    for (pageID = (uint8_t)gFirstVirtualPage_c; pageID < gVirtualPageNb_c; pageID++)
    {
        NVM_VirtualPageProperties_t *page_props = &mNvVirtualPageProperty[pageID];
        uint32_t                     top_count  = ~0UL;
        if (ReadPageTopCount(page_props, &top_count) == gNVM_OK_c)
        {
            if (top_count == gPageCounterMaxValue_c)
            {
                /* we read 0xffffffffU from the flash: it either means that it is blank / erased or written with
                 * 0xffffffff . HAL_FlashVerifyErase does not make the difference!
                 */
                bool_t erase_req = FALSE;
                if (!page_props->has_ecc_faults)
                {
                    if (NvVirtualPageBlankCheck((NVM_VirtualPageID_t)pageID) == gNVM_OK_c)
                    {
                        continue;
                    }
                    else
                    {
                        erase_req = TRUE;
                    }
                }
                else
                {
                    uint8_t  ram_buf[PGM_SIZE_BYTE];
                    uint32_t rd_offset;
                    uint8_t *ram_ptr;
                    for (rd_offset = 0U; rd_offset < mNvTotalPageSize; rd_offset += PGM_SIZE_BYTE)
                    {
                        ram_ptr = &ram_buf[0];
                        if (NV_PartitionReadAtOffset((NVM_VirtualPageID_t)pageID, rd_offset, ram_ptr, PGM_SIZE_BYTE) !=
                            gNVM_OK_c)
                        {
                            erase_req = TRUE;
                            break;
                        }
                        if (!FLib_MemCmpToVal(ram_buf, 0xffU, PGM_SIZE_BYTE))
                        {
                            erase_req = TRUE;
                            break;
                        }
                    }
                }

                if (erase_req)
                {
                    (void)HAL_FlashEraseSector(page_props->NvRawSectorStartAddress, mNvTotalPageSize);
                }
            }
        }
        else
        {
            /* ECC Error detected erase whole page regardless of any other consideration */
            (void)HAL_FlashEraseSector(page_props->NvRawSectorStartAddress, mNvTotalPageSize);
        }
    }
}

/*
 * Name: IsVirtualPageValid
 * Description: Check virtual page validity.
 * Parameter(s): v_page pointer of probed virtual page.
 * Return: TRUE if valid FALSE otherwise
 */
NVM_STATIC bool_t IsVirtualPageValid(NVM_VirtualPageProperties_t *v_page)
{
    bool_t valid = FALSE;
    if ((v_page->CounterTop == v_page->CounterBottom) && (gPageCounterMaxValue_c != v_page->CounterTop))
    {
        valid = TRUE;
#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
        if (v_page->has_ecc_faults != FALSE)
        {
            valid = FALSE;
        }
#endif
    }
    return valid;
}

/*
 * NvReadPageCounters
 *
 * Read both Top and Bottom page counters for a virtual page.
 *
 */
NVM_STATIC NVM_Status_t NvReadPageCounters(NVM_VirtualPageID_t pageId)
{
    NVM_Status_t st;
    uint32_t     value;

    NVM_VirtualPageProperties_t *page_prop = &mNvVirtualPageProperty[pageId];

    page_prop->CounterTop = page_prop->CounterBottom = 0U;
    uint32_t location_offset                         = 0U;

    do
    {
        st = NV_FlashRead(page_prop->NvRawSectorStartAddress, (uint8_t *)&value, sizeof(value),
                          page_prop->has_ecc_faults);
        if (st != gNVM_OK_c)
        {
            break;
        }
        page_prop->CounterTop = value;

        st = NV_FlashRead(page_prop->NvRawSectorEndAddress - (sizeof(NVM_TableInfo_t) - 1U - location_offset),
                          (uint8_t *)&value, sizeof(value), page_prop->has_ecc_faults);
        if (st != gNVM_OK_c)
        {
            break;
        }

        page_prop->CounterBottom = value;

    } while (FALSE);
    return st;
}

/*
 * NvAttemptToSalvageWhatCanBe
 *
 * If this function is called it means neither pages are valid:
 * To be valid they require:
 *   - contain no ECC fault
 *   - Top counter to be programmed
 *   - Top and Bottom counters to match
 *
 */
NVM_STATIC NVM_Status_t NvAttemptToSalvageWhatCanBe(void)
{
    NVM_VirtualPageProperties_t *cur_pg, *other_pg;
    NVM_Status_t                 status = gNVM_OK_c;
    bool_t                       ret    = FALSE;

    for (uint8_t pg_id = (uint8_t)gFirstVirtualPage_c; pg_id < gVirtualPageNb_c; pg_id++)
    {
        NVM_VirtualPageID_t other_pg_id = (NVM_VirtualPageID_t)(uint8_t)(((uint8_t)pg_id + 1U) % 2U);
        cur_pg                          = &mNvVirtualPageProperty[pg_id];
        other_pg                        = &mNvVirtualPageProperty[other_pg_id];
        if (cur_pg->has_ecc_faults)
        {
            if ((cur_pg->CounterTop != ~0UL) && (cur_pg->CounterTop == cur_pg->CounterBottom))
            {
                if (((other_pg->CounterTop != ~0UL) && (other_pg->CounterTop == other_pg->CounterBottom)))
                {
                    /* must mean that other_pg has_ecc_faults otherwise it would have been valid */
                    if (other_pg->CounterTop > cur_pg->CounterTop)
                    {
                        mNvActivePageId = other_pg_id;
                        mNvPageCounter  = other_pg->CounterTop;
                        ret             = TRUE;
                        break;
                    }
                }
                else
                {
                    mNvActivePageId = (NVM_VirtualPageID_t)pg_id;
                    mNvPageCounter  = cur_pg->CounterTop;
                    ret             = TRUE;
                    break;
                }
            }
            else
            {
                /* One of the page counters may have been corrupted or bottom counter programming may have failed */
#if gNvUseExtendedFeatureSet_d
                if (cur_pg->CounterTop != 0U) /* Could be read (no ECC fault on it) */
                {
                    NVM_TableInfo_t tbInfo;

                    if (NV_PartitionReadAtOffset((NVM_VirtualPageID_t)pg_id, 0U, (uint8_t *)&tbInfo,
                                                 sizeof(NVM_TableInfo_t)) == gNVM_OK_c)
                    {
                        if ((tbInfo.u.fields.NvTableMarker == gNvTableMarker_c) &&
                            (tbInfo.u.fields.NvTableVersion == gNvFlashTableVersion_c))
                        {
                            mNvActivePageId = (NVM_VirtualPageID_t)pg_id;
                            mNvPageCounter  = cur_pg->CounterTop;
                            ret             = TRUE;
                            break;
                        }
                    }
                }
#endif
            }
        }
    } /* for */
    if (ret)
    {
#if gNvUseExtendedFeatureSet_d
        /* get the size of the NV table stored in FLASH memory */
        mNvTableSizeInFlash = NvGetFlashTableSize();
#endif
        /* No longer useful to call NvGetEntryInfoNeedToAddInNVM here since it is moved to NvCopyPage */
        (void)NvUpdateLastMetaInfoAddress();

        status = NvCopyPage(gNvCopyAll_c);
    }
    return status;
}

/******************************************************************************
 * Name: NvInitStorageSystem
 * Description: Initialize the storage system, retrieve the active page and
 *              the page counter. Called once by NvModuleInit() function.
 * Parameter(s): -
 * Return: -
 *****************************************************************************/
NVM_STATIC void NvInitStorageSystem(void)
{
    bool_t same_cnt = FALSE;
    NvPostFwUpdateMaintenance();
    /* Read bottom and top counter for both virtual pages. */
    for (uint8_t idx = (uint8_t)gFirstVirtualPage_c; idx < gVirtualPageNb_c; idx++)
    {
        (void)NvReadPageCounters((NVM_VirtualPageID_t)idx);
    }

    /* get the active page */
    if (IsVirtualPageValid(&mNvVirtualPageProperty[gFirstVirtualPage_c]))      /* first page is valid */
    {
        if (IsVirtualPageValid(&mNvVirtualPageProperty[gSecondVirtualPage_c])) /* second page is valid */
        {
            /* Both valid: determine which is most recent */
            if (mNvVirtualPageProperty[gFirstVirtualPage_c].CounterTop >=
                mNvVirtualPageProperty[gSecondVirtualPage_c].CounterTop)
            {
                /* first page is active */
                mNvActivePageId = gFirstVirtualPage_c;
            }
            else
            {
                /* second page is active */
                mNvActivePageId = gSecondVirtualPage_c;
            }
            mNvPageCounter = mNvVirtualPageProperty[mNvActivePageId].CounterTop;
        }
        else
        {
            same_cnt = (mNvVirtualPageProperty[gSecondVirtualPage_c].CounterTop ==
                        mNvVirtualPageProperty[gSecondVirtualPage_c].CounterBottom) ?
                           TRUE :
                           FALSE;
            /* first page is active */
            mNvPageCounter = mNvVirtualPageProperty[gFirstVirtualPage_c].CounterTop;
            NvSetErasePgCmdStatus(gSecondVirtualPage_c, same_cnt);
        }
    }
    else
    {
        /* First page is not valid:
         *    1) There might be an ECC fault within
         *    2) The top and bottom counters may differ
         *    3) The top and bottom counter are blank
         */
        /* same_cnt first page is not valid but counter match */
        same_cnt = (mNvVirtualPageProperty[gFirstVirtualPage_c].CounterTop ==
                    mNvVirtualPageProperty[gFirstVirtualPage_c].CounterBottom) ?
                       TRUE :
                       FALSE;

        if (IsVirtualPageValid(&mNvVirtualPageProperty[gSecondVirtualPage_c])) /* second page is valid */
        {
            /* second page is active */
            mNvPageCounter = mNvVirtualPageProperty[gSecondVirtualPage_c].CounterTop;
            NvSetErasePgCmdStatus(gFirstVirtualPage_c, same_cnt);
        }
        else
        {
            mNvActivePageId = gVirtualPageNone_c;
            (void)NvAttemptToSalvageWhatCanBe();
        }
    }
}

/******************************************************************************
 * Name: NvVirtualPageBlankCheck
 * Description: checks if the specified page is blank (erased)
 * Parameter(s): [IN] pageID - the ID of the page to be checked
 * Return: gNVM_InvalidPageID_c - if the page ID is not valid
 *         gNVM_PageIsNotBlank_c - if the page is not blank
 *         gNVM_OK_c - if the page is blank (erased)
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvVirtualPageBlankCheck(NVM_VirtualPageID_t pageID)
{
    NVM_Status_t status = gNVM_OK_c;
    if (!NV_PartitionBlankCheckAtOffset(pageID, 0U, mNvTotalPageSize))
    {
        status = gNVM_PageIsNotBlank_c;
    }
    return status;
}

/******************************************************************************
 * Name: NvUpdateLastMetaInfoAddress
 * Description: retrieve and store (update) the last meta information address
 * Parameter(s): -
 * Return: gNVM_MetaNotFound_c - if no meta information has been found
 *         gNVM_OK_c - if the meta was found and stored (updated)
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvUpdateLastMetaInfoAddress(void)
{
    NVM_Status_t         status = gNVM_MetaNotFound_c;
    NVM_RecordMetaInfo_t metaValue;

    /* CERT-C INT30-C: use uint16_t so the backward loop decrement cannot wrap to UINT32_MAX.
     * All flash offsets are guaranteed to fit in uint16_t (guarded at init time). */
    uint16_t readOffset;

    do
    {
        mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset = gNvInvalidMetaOffset_c;
#if gUnmirroredFeatureSet_d
        mNvVirtualPageProperty[mNvActivePageId].NvLastMetaUnerasedInfoOffset = gNvInvalidMetaOffset_c;
#endif

        /* First forward loop to find first blank phrase from bottom of page */
        for (readOffset = (uint16_t)gNvFirstMetaOffset_c; readOffset < (uint16_t)mNvTotalPageSize;
             readOffset += (uint16_t)sizeof(NVM_RecordMetaInfo_t))
        {
            status = NV_PartitionReadAtOffset(mNvActivePageId, readOffset, (uint8_t *)&metaValue,
                                              sizeof(NVM_RecordMetaInfo_t));
            if (gNVM_OK_c != status)
            {
                continue;
            }
            if (gNvGuardValue_c == metaValue.u.rawValue)
            {
                /* break when we find a blank phrase */
                break;
            }
        }
        if (readOffset >= (uint16_t)(mNvTotalPageSize - sizeof(NVM_RecordMetaInfo_t)))
        {
            status = gNVM_MetaNotFound_c;
            break;
        }
        /* Either readAddress is at start meaning that there are no records registered in the page , or
         * readAOffset advanced beyond last record position and we need to step back.
         */
        if (readOffset == gNvFirstMetaOffset_c)
        {
            status = gNVM_OK_c;
            break;
        }
        /* If we reached here there should be some records : we went one step too far - step back */
        readOffset -= (uint16_t)sizeof(NVM_RecordMetaInfo_t);
        /* CERT-C INT30-C: readOffset is uint16_t; the do-while processes the body first
         * and only decrements when readOffset is strictly greater than gNvFirstMetaOffset_c,
         * so the last valid slot is processed without an underflowing decrement. */
        do
        {
            status = NvGetMetaInfo(mNvActivePageId, readOffset, &metaValue);
            if (gNVM_OK_c != status)
            {
                /* read error on this slot – keep scanning */
            }
            else
            {
                if (mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset == gNvInvalidMetaOffset_c)
                {
                    /* If we reach here the meta information start and end validation bytes are correct - so is the
                     * offset
                     */
                    mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset = readOffset;
#if !(defined gUnmirroredFeatureSet_d && (gUnmirroredFeatureSet_d != 0))
                    status = gNVM_OK_c;
                    break;
#endif
                }

#if gUnmirroredFeatureSet_d
                /* record valid and not erased NvmRecordOffset denotes that record has been priorly erased*/
                if (metaValue.u.fields.NvmRecordOffset != 0U)
                {
                    /* we found the last unerased meta info address */
                    mNvVirtualPageProperty[mNvActivePageId].NvLastMetaUnerasedInfoOffset = readOffset;
                    status                                                               = gNVM_OK_c;
                    break;
                }
#endif
            }
            if (readOffset == (uint16_t)gNvFirstMetaOffset_c)
            {
                break;
            }
            readOffset -= (uint16_t)sizeof(NVM_RecordMetaInfo_t);
        } while (TRUE); /* do-while: safe backward scan, no underflow possible */
    } while (FALSE);
    return status;
}

/******************************************************************************
 * Name: NvRecordMetaInfoIsBlank
 * Description: Check if MIT is blank
 * Parameter(s): [IN] p_mit - pointer on MIT
 * Return: TRUE if blank, FALSE otherwise
 *****************************************************************************/
NVM_STATIC bool_t NvRecordMetaInfoIsBlank(NVM_RecordMetaInfo_t *p_mit)
{
    bool_t ret = TRUE;
    /* Is MIT blank ? */
    uint8_t *p = (uint8_t *)(void *)p_mit;
    for (uint32_t i = 0U; i < sizeof(NVM_RecordMetaInfo_t); i++)
    {
        /* Equivalent to FLib_MemCmpToVal but avoid HIS_CALLING */
        if (p[i] != 0xffU)
        {
            ret = FALSE;
            break;
        }
    }
    return ret;
}

/******************************************************************************
 * Name: NvGetMetaInfo
 * Description: get meta information based on the meta information address
 * Parameter(s): [IN] pageId - the ID of the page
 *               [IN] metaInfoOffset - meta information address
 *               [OUT] pMetaInfo - a pointer to a memory location where the
 *                                 requested meta information will be stored
 * Return: gNVM_InvalidPageID_c - if the active page is not valid
 *         gNVM_AddressOutOfRange_c - if the provided address is out of range
 *         gNVM_MetaInfoInvalidError_c - MIT is invalid
 *         gNVM_MetaInfoBlank_c - contains guard value
 *         gNVM_EccFault_c - if ECC fault was raised when reading MIT
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetMetaInfo(NVM_VirtualPageID_t   pageId,
                                      uint32_t              metaInfoOffset,
                                      NVM_RecordMetaInfo_t *pMetaInfo)
{
    NVM_Status_t status;
    /* check address range */
    do
    {
        if ((metaInfoOffset < gNvFirstMetaOffset_c) || (metaInfoOffset >= mNvTotalPageSize))
        {
            status = gNVM_AddressOutOfRange_c;
            break;
        }

        /* read the meta information tag */
        status = NV_PartitionReadAtOffset(pageId, metaInfoOffset, (uint8_t *)pMetaInfo, sizeof(NVM_RecordMetaInfo_t));

        if (status != gNVM_OK_c)
        {
            /* Especially in case of ECC error return error now */
            break;
        }

        if (NvRecordMetaInfoIsBlank(pMetaInfo) == TRUE)
        {
            status = gNVM_MetaInfoBlank_c;
            break;
        }
        /* NVM version is the first version supporting checksum validation */
        if (mNvMetaInfoChecksumEnabled != 0)
        {
#if (defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d != 0))
            if (pMetaInfo->NvmMetaChecksum != 0xffffffffUL)
            {
                uint32_t checksum = NvCalculateChecksum(pMetaInfo);
                if (checksum != 0xffffffffUL)
                {
                    status = gNVM_MetaInfoInvalidError_c;
                    break;
                }
            }
#else
            ; /* NOP - checksum validation disabled */
#endif
        }

        if ((pMetaInfo->u.fields.NvValidationStartByte != gValidationByteAllRecords_c) &&
            (pMetaInfo->u.fields.NvValidationStartByte != gValidationByteSingleRecord_c))
        {
            status = gNVM_MetaInfoInvalidError_c;
            break;
        }
        if (pMetaInfo->u.fields.NvValidationStartByte != pMetaInfo->u.fields.NvValidationEndByte)
        {
            status = gNVM_MetaInfoInvalidError_c;
            break;
        }
        if (pMetaInfo->u.fields.NvmRecordOffset > mNvTotalPageSize)
        {
            status = gNVM_MetaInfoInvalidError_c;
            break;
        }
        if ((pMetaInfo->u.fields.NvmRecordOffset != 0U) &&
            (pMetaInfo->u.fields.NvmRecordOffset) < (metaInfoOffset + sizeof(NVM_RecordMetaInfo_t)))
        {
            status = gNVM_MetaInfoInvalidError_c;
            break;
        }

#if gNvFragmentation_Enabled_d
        if (pMetaInfo->u.fields.NvmElementIndex >= gNvRecordsCopiedBufferSize_c)
        {
            status = gNVM_MetaInfoInvalidError_c;
            break;
        }
#endif

        status = gNVM_OK_c;
    } while (false);
    return status;
}

#if (defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d != 0))
/******************************************************************************
 * Name: NvCalculateChecksum
 * Description: Compute checksum over MIT fields up to padding field.
 *
 * Note: When setting the checksum, the checksum field itself must be set to 0xffffffffU.
 * A checksum is computed by XORing first 3 32-bit words in the MIT structure.
 * A valid value is expected to be 0xffffffffU after XORing with the checksum field.
 *
 * Parameter(s): [IN] metaInfo - pointer on MIT structure
 *
 * Return: checksum value on 32 bits
 *  ******************************************************************************/
NVM_STATIC uint32_t NvCalculateChecksum(NVM_RecordMetaInfo_t *metaInfo)
{
    uint32_t  checksum = 0U;
    uint32_t *ptr      = (uint32_t *)(void *)metaInfo;
    for (uint32_t i = 0U; i < (uint32_t)(offsetof(NVM_RecordMetaInfo_t, Padding) / sizeof(uint32_t)); i++)
    {
        checksum ^= ptr[i];
    }
    return checksum;
}
#endif /* gNvmMetaCheckSum_d */

/******************************************************************************
 * Name: NvSetMetaInfo
 * Description: Writes MIT fields and computes optional checksum
 * Parameter(s): [IN] metaInfo - pointer on MIT structure to be filled
 *               [IN] entryId - the entry ID
 *               [IN] eltIndex element index
 *               [IN] recordOffset - offset of the record data in the page - must be 32 bit aligned
 *               [IN] vb_val - validation byte value, gValidationByteSingleRecord_c or gValidationByteAllRecords_c
 * Return: -
 *  ******************************************************************************/
NVM_STATIC void NvSetMetaInfo(
    NVM_RecordMetaInfo_t *metaInfo, uint16_t entryId, uint16_t eltIndex, uint16_t recordOffset, uint8_t vb_val)
{
    assert(IS_OFFSET_32BIT_ALIGNED(recordOffset));
    assert((vb_val == gValidationByteSingleRecord_c) || (vb_val == gValidationByteAllRecords_c));

    FLib_MemSet((uint8_t *)metaInfo->Padding, 0xffU, sizeof(metaInfo->Padding));
    metaInfo->u.fields.NvValidationStartByte = vb_val;
    metaInfo->u.fields.NvmDataEntryID        = entryId;
    metaInfo->u.fields.NvmElementIndex       = eltIndex;
    metaInfo->u.fields.NvmRecordOffset       = recordOffset;
    metaInfo->u.fields.NvValidationEndByte   = vb_val;
#if (defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d != 0))
    metaInfo->NvmMetaChecksum = 0xffffffffUL;
    metaInfo->NvmMetaChecksum = NvCalculateChecksum(metaInfo);
#endif /* gNvmMetaCheckSum_d */
}

/*!
 * \brief Compute the flash offset of a single NVM dataset element.
 *
 * The offset is calculated as:
 *   elt_offset = (elt_index * elt_size) + inner_offset + rec_offset
 *
 * All intermediate additions are checked for 16-bit overflow. If any
 * addition would wrap beyond UINT16_MAX the function returns an error
 * without writing to *elt_offset.
 *
 * \param[in]  nb           Number of elements.
 * \param[in]  elt_size     Size in bytes of one element.
 * \param[out] pval         Receives the computed 16-bit multiplication on
 *                          success. Not written on error.
 *
 * \retval  0   Success; *pval contains the computed offset.
 * \retval -1   Overflow detected; the result would exceed UINT16_MAX.
 */
NVM_STATIC int NvMultEltSzByNb(uint16_t nb, uint16_t elt_size, uint32_t max_val, uint16_t *pval)
{
    int ret = -1;
    do
    {
        uint32_t val;
        /* Compute multiplication */
        val = (uint32_t)elt_size * (uint32_t)nb;
        /* We are guaranteed that the multiplication does not exceed 32 bits but it
         * must also remain smaller than 16 bits */
        if (val > max_val)
        {
            /* Check that no wrap would occur by adding inner_offset */
            break;
        }
        /* MISRA 10.3: val is guaranteed <= max_val <= UINT16_MAX at this point */
        assert(val <= (uint32_t)UINT16_MAX);
        *pval = (uint16_t)val;
        ret   = 0;
    } while (false);

    return ret;
}
/*!
 * \brief Compute the flash offset of a single NVM dataset element.
 *
 * The offset is calculated as:
 *   elt_offset = (elt_index * elt_size) + inner_offset + rec_offset
 *
 * All intermediate additions are checked for 16-bit overflow. If any
 * addition would wrap beyond UINT16_MAX the function returns an error
 * without writing to *elt_offset.
 *
 * \param[in]  elt_index    Zero-based index of the element within the dataset.
 * \param[in]  elt_size     Size in bytes of one element. When the dataset is
 *                          being copied without fragmentation this may be 0,
 *                          in which case the index contribution is 0.
 * \param[in]  inner_offset Byte offset within the element (e.g. partial-write
 *                          start position).
 * \param[in]  rec_offset   Base offset of the NVM record in the page.
 * \param[out] elt_offset   Receives the computed 16-bit flash offset on
 *                          success. Not written on error.
 *
 * \retval  0   Success; *elt_offset contains the computed offset.
 * \retval -1   Overflow detected; the result would exceed UINT16_MAX.
 */
NVM_STATIC int NvComputeEltOffset(
    uint16_t elt_index, uint16_t elt_size, uint16_t inner_offset, uint16_t rec_offset, uint16_t *elt_offset)
{
    int ret = -1;

    do
    {
        uint16_t offs    = 0U;
        uint32_t max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
        /* Compute offset of element */
        if (NvMultEltSzByNb(elt_index, elt_size, max_val, &offs) < 0)
        {
            break;
        }
        /* We are guaranteed that the multiplication does not exceed 32 bits but it
         * must also remain smaller than 16 bits */
        /* CERT INT30-C: guard subtraction does not wrap before comparison */
        if (inner_offset > max_val)
        {
            break;
        }
        if (offs > (max_val - inner_offset))
        {
            /* Check that no wrap would occur by adding inner_offset */
            break;
        }
        offs += inner_offset;
        /* CERT INT30-C: guard subtraction does not wrap before comparison */
        if (rec_offset > (uint32_t)max_val)
        {
            break;
        }
        if (offs > ((uint32_t)max_val - rec_offset))
        {
            /* Check that no wrap would occur by adding rec_offset */
            break;
        }
        offs += rec_offset;
        *elt_offset = (uint16_t)offs;
        ret         = 0;
    } while (false);
    return ret;
}

/******************************************************************************
 * Name: NvGetPageFreeSpace
 * Description: return the active page free space, in bytes
 * Parameter(s): [OUT] ptrFreeSpace - a pointer to a memory location where the
 *                                    page free space will be stored
 *               [IN] blank_check_req : if TRUE perform blank_check over assumed free space
 * Return: gNVM_InvalidPageID_c - if the active page is not valid
 *         gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PageIsEmpty_c - if the page is empty
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetPageFreeSpace(uint32_t *ptrFreeSpace, bool_t blank_check_req)
{
    NVM_RecordMetaInfo_t metaInfo;
    NVM_Status_t         retVal = gNVM_Error_c;
    uint32_t             len    = 0U;

    do
    {
        NVM_VirtualPageProperties_t *act_page = &mNvVirtualPageProperty[mNvActivePageId];

        uint32_t last_meta_offset       = (uint32_t)act_page->NvLastMetaInfoOffset;
        uint32_t bottom_rec_data_offset = mNvTotalPageSize;
        uint32_t top_mit_offset;

#if gUnmirroredFeatureSet_d
        NVM_RecordMetaInfo_t metaInfoUndeleted;
#endif

        /* Check if page is empty : in which case the free space is the entire page minus the table info */
        if (gNvInvalidMetaOffset_c == act_page->NvLastMetaInfoOffset)
        {
            /* gNvFirstMetaOffset_c is a macro that takes into account mNvTableSizeInFlash for extended feature set */
            top_mit_offset = gNvFirstMetaOffset_c;
            if (bottom_rec_data_offset <= (2U * sizeof(NVM_TableInfo_t)))
            {
                retVal = gNVM_Error_c;
                break;
            }
            /* Safe: bottom_rec_data_offset is much larger than sizeof(NVM_TableInfo_t) */
            bottom_rec_data_offset -= sizeof(NVM_TableInfo_t);
            if (top_mit_offset < bottom_rec_data_offset)
            {
                len    = bottom_rec_data_offset - top_mit_offset;
                retVal = gNVM_OK_c;
            }
            break;
        }
        /* Read the last meta info from the active page and the last unerased meta info (if applicable) */
        top_mit_offset = last_meta_offset + sizeof(NVM_RecordMetaInfo_t);
        retVal         = NvGetMetaInfo(mNvActivePageId, act_page->NvLastMetaInfoOffset, &metaInfo);

        if (gNVM_OK_c != retVal)
        {
            break;
        }
#if gUnmirroredFeatureSet_d
        /* Erased records occupy no space */
        if (metaInfo.u.fields.NvmRecordOffset == 0U)
        {
            retVal = NvGetMetaInfo(mNvActivePageId, act_page->NvLastMetaUnerasedInfoOffset, &metaInfoUndeleted);
            if (gNVM_OK_c != retVal)
            {
                break;
            }
            bottom_rec_data_offset = metaInfoUndeleted.u.fields.NvmRecordOffset;
        }
        else
        {
            bottom_rec_data_offset = metaInfo.u.fields.NvmRecordOffset;
        }
#else
        bottom_rec_data_offset = metaInfo.u.fields.NvmRecordOffset;
#endif /* gUnmirroredFeatureSet_d */
        if (bottom_rec_data_offset > top_mit_offset)
        {
            len = bottom_rec_data_offset - top_mit_offset;
        }
        else
        {
            len = 0U;
        }
        if (blank_check_req == TRUE)
        {
            if (NV_PartitionBlankCheckAtOffset(mNvActivePageId, top_mit_offset, len) != TRUE)
            {
                len    = 0U;
                retVal = gNVM_Error_c;
                break;
            }
        }

    } while (FALSE);

    *ptrFreeSpace = len;

    return retVal;
}

/******************************************************************************
 * Name: NvIsRecordCopied
 * Description: Checks if a record or an entire table entry is already copied.
 *              Called by page copy function.
 * Parameter(s): [IN] pageId - the ID of the page where to perform the check
 *               [IN] metaInf - a pointer to source page meta information tag
 * Return: TRUE if the element is already copied, FALSE otherwise
 *****************************************************************************/
NVM_STATIC bool_t NvIsRecordCopied(NVM_VirtualPageID_t pageId, NVM_RecordMetaInfo_t *metaInf)
{
    NVM_RecordMetaInfo_t metaValue;
    bool_t               retVal;
    NVM_Status_t         status;
    uint32_t             start_offset = gNvFirstMetaOffset_c;

    FLib_MemSet(&metaValue, 0U, sizeof(NVM_RecordMetaInfo_t));
#if gNvDualImageSupport_d
    uint16_t table_sz_increase;
    uint32_t max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX) - gNvFirstMetaOffset_c;
    /* MISRA 10.3: explicit cast of sizeof result to uint16_t */
    if (NvMultEltSzByNb(mNvNeedAddEntryCnt, (uint16_t)sizeof(NVM_TableInfo_t), max_val, &table_sz_increase) < 0)
    {
        assert(false);
    }
    else
    {
        /* CERT INT30-C: guard addition does not wrap uint32_t */
        assert(start_offset <= ((uint32_t)UINT32_MAX - (uint32_t)table_sz_increase));
        if (start_offset <= ((uint32_t)UINT32_MAX - (uint32_t)table_sz_increase))
        {
            start_offset += (uint32_t)table_sz_increase;
        }
    }
#endif

    retVal = FALSE;

    for (uint32_t loop_offset = start_offset; loop_offset < mNvTotalPageSize;
         loop_offset += sizeof(NVM_RecordMetaInfo_t))
    {
        /* read the meta information tag from destination page  */
        status = NvGetMetaInfo(pageId, loop_offset, &metaValue);

        if ((status == gNVM_EccFault_c) || (status == gNVM_MetaInfoInvalidError_c))
        {
            /* detected ECC fault or wrongly stored MIT while reading it - skip and jump to next */
            continue;
        }
        if (status == gNVM_MetaInfoBlank_c)
        {
            /* reached last meta */
            break;
        }

        if (metaInf->u.fields.NvmDataEntryID == metaValue.u.fields.NvmDataEntryID)
        {
            if (metaInf->u.fields.NvValidationStartByte == gValidationByteSingleRecord_c)
            {
                if (metaValue.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c)
                {
                    if (metaValue.u.fields.NvmElementIndex == metaInf->u.fields.NvmElementIndex)
                    {
                        retVal = TRUE;
                        break;
                    }

                    /* skip */
                    continue;
                }
                retVal = TRUE;
                break;
            }

            if (metaInf->u.fields.NvValidationStartByte == gValidationByteAllRecords_c)
            {
                if (metaValue.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c)
                {
                    /* skip */
                    continue;
                }
                retVal = TRUE;
                break;
            }

            /* skip */
            continue;
        }
    }

    return retVal;
}
/******************************************************************************
 * Name: NvInternalCopy
 * Description: Performs a copy of an record / entire table entry
 * Parameter(s): [IN] dstRecOffset - destination record address
 *               [IN] dstMetaOffset - destination meta address
 *               [IN] srcMetaInfo - source meta information
 *               [IN] srcTblEntryIdx - source table entry index
 *               [IN] size - bytes to copy
 * Return: gNVM_InvalidPageID_c - if the source or destination page is not
 *                                valid
 *         gNVM_MetaInfoWriteError_c - if the meta information couldn't be
 *                                     written
 *         gNVM_RecordWriteError_c - if the record couldn't be written
 *         gNVM_Error_c - in case of error(s)
 *         gNVM_OK_c - page copy completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalCopy(uint16_t              dstRecOffset,
                                       uint16_t              dstMetaOffset,
                                       NVM_RecordMetaInfo_t *srcMetaInfo,
                                       uint16_t              srcTblEntryIdx,
                                       uint16_t              size)
{
    NVM_Status_t status = gNVM_OK_c;

    uint8_t              cacheBuffer[PGM_SIZE_BYTE] = {0U};
    NVM_RecordMetaInfo_t dstMetaInfo                = {0U};
    uint16_t             diffSize                   = 0U;
    uint16_t             diffIdx                    = 0U;
    uint16_t             ramSize                    = 0U;
    uint8_t              misalignedBytes;

    uint16_t innerOffset;

    do
    {
        NVM_VirtualPageID_t dstPgId            = OTHER_PAGE_ID(mNvActivePageId);
        uint32_t            max_val            = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
        uint32_t            afterDstRecordOffs = (uint32_t)dstRecOffset + (uint32_t)size;
        uint16_t            tb_element_count   = 0U;
        uint16_t            tb_element_sz      = 0U;
        if (afterDstRecordOffs > ((uint32_t)UINT16_MAX + 1U))
        {
            /* Must have been verified by the caller already */
            assert(false);
            status = gNVM_AddressOutOfRange_c;
            break;
        }
        /* Initialize the inner offset*/
        innerOffset = 0U;
        /* prepare destination page meta info tag and write if after the record is entirely written.
         * the preparation is made here because the 'dstAddress' may change afterwards
         */
        /* patch checksum since offset changes in new page */

        NvSetMetaInfo(&dstMetaInfo, srcMetaInfo->u.fields.NvmDataEntryID, srcMetaInfo->u.fields.NvmElementIndex,
                      dstRecOffset, srcMetaInfo->u.fields.NvValidationStartByte);

        if (srcMetaInfo->u.fields.NvValidationStartByte != gValidationByteSingleRecord_c)
        {
#if gNvDualImageSupport_d
            if (srcTblEntryIdx == gNvInvalidTableEntryIndex_c)
            {
                /* no action needed */
            }
            else
            {
#endif /* gNvDualImageSupport_d */
                tb_element_count = pNVM_DataTable[srcTblEntryIdx].ElementsCount;
                tb_element_sz    = pNVM_DataTable[srcTblEntryIdx].ElementSize;
                if (NvMultEltSzByNb(tb_element_count, tb_element_sz, max_val, &ramSize) < 0)
                {
                    assert(false);
                    status = gNVM_Error_c;
                    break;
                }
                /* supplementary bytes from RAM when RAM table entry is larger than flash record */
                if (size < ramSize)
                {
                    diffSize = ramSize - size;
                    diffIdx  = size / tb_element_sz;
                }
#if gNvDualImageSupport_d
            }
#endif /* gNvDualImageSupport_d */
        }

        while (size != 0U)
        {
            uint16_t src_elt_offset = 0U;
            /* Calculate innerOffset + srcMetaInfo->u.fields.NvmRecordOffset */
            if (NvComputeEltOffset(0U, 0U, innerOffset, srcMetaInfo->u.fields.NvmRecordOffset, &src_elt_offset) < 0)
            {
                status = gNVM_Error_c;
                break;
            }

            if (size > (uint16_t)sizeof(cacheBuffer))
            {
                /* copy from FLASH to cache buffer : phrase by phrase progression */
                /* The source page is unlikely to contain errors because if we reached this point it means the meta data
                 * of the source were safely read, so the contents of the record must be error free. */
                status = NV_PartitionReadAtOffset(mNvActivePageId, src_elt_offset, (uint8_t *)&cacheBuffer[0],
                                                  (uint16_t)sizeof(cacheBuffer));
                if (gNVM_OK_c != status)
                {
                    break;
                }
                /* write to destination page */
                status = NV_PartitionProgramUnalignedAtOffset(dstPgId, dstRecOffset, (uint16_t)sizeof(cacheBuffer),
                                                              (uint8_t *)cacheBuffer);
                if (gNVM_OK_c != status)
                {
                    /* The copy of the record contents did not go well  */
                    /* It might mean that an ECC error occurred while writing */
                    break;
                }

                /* update the destination record address copy */
                dstRecOffset += (uint16_t)sizeof(cacheBuffer);
                /* dstRecOffset guaranteed not to exceed UINT16_MAX thanks to above afterDstRecordOffs check */
                size -= (uint16_t)sizeof(cacheBuffer);
                /* update the inner offset value */
                innerOffset += (uint16_t)sizeof(cacheBuffer);

                /* continue since no error */
            }
            else
            {
                /* copy from FLASH to cache buffer: assuming error free record contents  */
                status = NV_PartitionReadAtOffset(mNvActivePageId, src_elt_offset, &cacheBuffer[0], size);
                break;
            }
        } /* while */

        if (gNVM_OK_c != status)
        {
            break;
        }
        if (diffSize != 0U)
        {
            /* pNVM_DataTable[srcTblEntryIdx] describes data in RAM whose length is larger */
            /* Note that tb_element_sz has necessarily been initialized if diffSize is not 0*/
            uint8_t *src_ptr;
            uint16_t cpy_offs;
            if (NvMultEltSzByNb(diffIdx, tb_element_sz, max_val, &cpy_offs) < 0)
            {
                assert(false);
                status = gNVM_Error_c;
                break;
            }
            src_ptr = (uint8_t *)pNVM_DataTable[srcTblEntryIdx].pData + cpy_offs;

            /* update the destination record address copy */
            /*  CERT-C Integers (CERT INT30-C)  false positive: dstRecOffset already not to exceed UINT16_MAX
             * thanks to above afterDstRecordOffs check*/
            dstRecOffset += size;

            /* check alignment and adjust it if necessary */
            misalignedBytes = (uint8_t)(dstRecOffset & (((uint16_t)PGM_SIZE_BYTE - 1U)));

            /* initialise the inner offset */
            innerOffset = 0U;

            /* check if the destination is longword aligned or not */
            if (misalignedBytes != 0U)
            {
                uint16_t loopIdx;
                uint16_t loopEnd;
                /* align to previous phrase boundary */
                dstRecOffset &= (uint16_t)(~((uint16_t)PGM_SIZE_BYTE - 1U));

                /* compute the loop end */
                loopEnd =
                    (uint16_t)sizeof(cacheBuffer) - (uint16_t)misalignedBytes; /* Number of byte to complete cache */

                /* update with data from RAM */
                for (loopIdx = 0U; loopIdx < loopEnd; loopIdx++)
                {
                    cacheBuffer[misalignedBytes] = src_ptr[innerOffset];
                    innerOffset++;
                    misalignedBytes++;
                    if (innerOffset == diffSize)
                    {
                        break;
                    }
                }
                /* write the cache buffer that got filled in to Flash destination page */
                status = NV_PartitionProgramUnalignedAtOffset(dstPgId, dstRecOffset, (uint16_t)sizeof(cacheBuffer),
                                                              (uint8_t *)cacheBuffer);
                if (gNVM_OK_c == status)
                {
                    /* align to next phrase boundary */
                    dstRecOffset += (uint16_t)sizeof(cacheBuffer);
                }
            }

            if (gNVM_OK_c == status)
            {
                /* write to Flash destination page the rest of the aligned data */
                if (diffSize >= innerOffset)
                {
                    /* Always true but please coverity */
                    uint16_t sz = diffSize - innerOffset;
                    src_ptr += innerOffset;

                    status = NV_PartitionProgramUnalignedAtOffset(dstPgId, dstRecOffset, sz, src_ptr);
                }
                else
                {
                    assert(false);
                    ;
                }
            }
        }
        else
        {
            /* write to destination page */
            status = NV_PartitionProgramUnalignedAtOffset(dstPgId, dstRecOffset, (uint16_t)size, cacheBuffer);
        }
        if (gNVM_OK_c != status)
        {
            break;
        }
        /* write the associated record meta information */
        /* Use aligned version of programming API because meta data are always aligned */
        status =
            NV_PartitionProgramAtOffset(dstPgId, dstMetaOffset, (uint8_t *)&dstMetaInfo, sizeof(NVM_RecordMetaInfo_t));
        if (gNVM_OK_c != status)
        {
            status = gNVM_MetaInfoWriteError_c;
        }
    } while (false);
    return status;
}

/******************************************************************************
 * Name: NvGetTblEntryMetaOffsetFromId
 * Description: Gets the table entry meta address based on table entry ID
 * Parameter(s): [IN] searchOffset - the search start address
 *               [IN] dataEntryId - table entry ID
 * Return: the value of the sought meta offset
 *****************************************************************************/
#if gNvFragmentation_Enabled_d
NVM_STATIC uint16_t NvGetTblEntryMetaOffsetFromId(uint16_t searchOffset, uint16_t dataEntryId)
{
    NVM_RecordMetaInfo_t metaInfo     = {0U};
    uint16_t             ret_offs     = gNvInvalidMetaOffset_c;
    NVM_Status_t         status       = gNVM_OK_c;
    uint32_t             firstMetaOff = gNvFirstMetaOffset_c;

    for (uint32_t offs = searchOffset; offs >= firstMetaOff; offs -= (uint32_t)sizeof(NVM_RecordMetaInfo_t))
    {
        /* Tread backwards through MIT records */
        status = NvGetMetaInfo(mNvActivePageId, offs, &metaInfo);
        if ((status != gNVM_OK_c) || (metaInfo.u.fields.NvValidationStartByte != gValidationByteAllRecords_c))
        {
            if (status != gNVM_Error_c)
            {
                /* skip wrong MIT or if not AllRecords */
                continue;
            }
            else
            {
                break;
            }
        }

        if (metaInfo.u.fields.NvmDataEntryID == dataEntryId)
        {
            /* found it */
            ret_offs = (uint16_t)(offs & (uint32_t)0xffffU);
            break;
        }
    }
    return ret_offs;
}

/******************************************************************************
 * Name: NvInternalRecordsUpdate
 * Description: Performs to update nv records
 * Parameter(s): [IN] srcMetaOffset - source page meta address
 *               [IN] srcTblEntryIdx - source page table entry index
 *               [IN] ownerRecordMetaInfoOffset - offset to the location of a full dataset save - search bottom
 *               [IN] ownerRecordId sought ID
 * Return: the status of the operation
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalRecordsUpdate(uint32_t srcMetaOffset,
                                                uint16_t srcTblEntryIdx,
                                                uint16_t ownerRecordMetaInfoOffset,
                                                uint16_t ownerRecordId)
{
    NVM_RecordMetaInfo_t metaInfo   = {0U};
    uint32_t             metaOffset = srcMetaOffset;
    NVM_Status_t         status     = gNVM_OK_c;

    do
    {
#if gNvDualImageSupport_d
        NVM_DataEntry_t flashDataEntry = {0U};

        /* if the srcTblEntryIdx is invalid, it means the entry may is from NVM, then size should from NVM entry  */
        if (srcTblEntryIdx == gNvInvalidTableEntryIndex_c)
        {
            /* get current meta information */
            status = NvGetMetaInfo(mNvActivePageId, metaOffset, &metaInfo);
            if (status != gNVM_OK_c)
            {
                break;
            }

            /* get Entry table from NVM*/
            if (NvGetTableEntry(metaInfo.u.fields.NvmDataEntryID, &flashDataEntry) == TRUE)
            {
                /* clear the records offsets buffer */
                FLib_MemSet(maNvRecordsCpyOffsets, 0U, (uint32_t)sizeof(uint16_t) * flashDataEntry.ElementsCount);

                FLib_MemSet(&metaInfo, 0U, sizeof(metaInfo));
            }
        }
        else
        {
#endif /* gNvDualImageSupport_d */
            /* clear the records offsets buffer */
            FLib_MemSet(maNvRecordsCpyOffsets, 0U,
                        (uint32_t)sizeof(uint16_t) * pNVM_DataTable[srcTblEntryIdx].ElementsCount);
#if gNvDualImageSupport_d
        }
#endif /* gNvDualImageSupport_d */

        /* Search backwards in active page for partial records of same ID */
        for (metaOffset = srcMetaOffset; metaOffset > ownerRecordMetaInfoOffset;
             metaOffset -= sizeof(NVM_RecordMetaInfo_t))
        {
            uint16_t elt_index;
            /* get meta information */
            status = NvGetMetaInfo(mNvActivePageId, metaOffset, &metaInfo);
            if ((status != gNVM_OK_c) || (metaInfo.u.fields.NvValidationStartByte != gValidationByteSingleRecord_c))
            {
                /* skip invalid entries and full table records */
                continue;
            }
            elt_index = metaInfo.u.fields.NvmElementIndex;
#if gNvDualImageSupport_d
            if (srcTblEntryIdx == gNvInvalidTableEntryIndex_c)
            {
                /* if the srcTblEntryIdx is invalid, it means the entry may is from NVM, then not need if the element
                 * still belongs to an valid RAM  */
                if (elt_index >= flashDataEntry.ElementsCount)
                {
                    /* maybe something wrong*/
                    continue;
                }
            }
            else
            {
#endif /* gNvDualImageSupport_d */
                /* check if the element still belongs to an valid RAM table entry */
                if (elt_index >= pNVM_DataTable[srcTblEntryIdx].ElementsCount)
                {
                    /* the FLASH element is no longer a current RAM table entry element */
                    continue;
                }
#if gNvDualImageSupport_d
            }
#endif /* gNvDualImageSupport_d */
            /* found a new single record not copied */
            if (metaInfo.u.fields.NvmDataEntryID == ownerRecordId)
            {
                if (0U == maNvRecordsCpyOffsets[elt_index])
                {
                    /* Coverity: Speculative execution data leak
                     * Insert a barrier between the comparison and the memory accesses to prevent speculative execution */
                    __DSB();
                    maNvRecordsCpyOffsets[elt_index] = metaInfo.u.fields.NvmRecordOffset;
                }
            }
            /* continue */
        } /* for */
    } while (false);

    return status;
}

/******************************************************************************
 * Name: NvInternalDefragmentedCopy
 * Description: Performs defragmentation and copy from the source page to
 *              the destination one
 * Parameter(s): [IN] srcMetaOffset - source page meta offset
 *               [IN] srcTblEntryIdx - source page table entry index
 *               [IN] dstMetaOffset - destination meta address
 *               [IN] dstRecordOffset - destination record offset (to copy to)
 *               [IN] ownerRecordMetaInfo - pointer to the location of a full dataset save
 * Return: the status of the operation
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalDefragmentedCopy(uint32_t srcMetaOffset,
                                                   uint16_t srcTblEntryIdx,
                                                   uint32_t dstMetaOffset,
                                                   uint32_t dstRecordOffset,
                                                   uint16_t ownerRecordMetaInfoOffset)
{
    NVM_Status_t status = gNVM_OK_c;
    uint16_t     size   = 0U;

    NVM_RecordMetaInfo_t dstMetaInfo;
    uint32_t             prog_offset;
    uint8_t              dstBuffer[PGM_SIZE_BYTE];

    do
    {
        NVM_VirtualPageID_t  dstPgId             = OTHER_PAGE_ID(mNvActivePageId);
        NVM_RecordMetaInfo_t ownerRecordMetaInfo = {0U};

#if gNvUseExtendedFeatureSet_d
        uint8_t  space_left;
        uint16_t copy_amount;
        uint16_t element_idx          = 0U;
        uint8_t  element_inner_copied = 0U;

        uint16_t        elt_cnt        = 0U;
        uint16_t        elt_sz         = 0U;
        uint32_t        max_val        = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
        bool_t          fillFromRAM    = FALSE;
        NVM_DataEntry_t flashDataEntry = {0U};

#if gNvDualImageSupport_d
        NVM_DataEntry_t     *flashDataEntryForSave;
        NVM_RecordMetaInfo_t srcMetaInfo = {0U};
        /* if the srcTblEntryIdx is invalid, the entry may come from NVM: size is taken
           from the NVM entry and the RAM table does not need to be considered */
        if (srcTblEntryIdx == gNvInvalidTableEntryIndex_c)
        {
            /* get current meta information */
            status = NvGetMetaInfo(mNvActivePageId, srcMetaOffset, &srcMetaInfo);
            if (status != gNVM_OK_c)
            {
                break;
            }
            /* get Entry table from NVM*/
            if (NvGetTableEntry(srcMetaInfo.u.fields.NvmDataEntryID, &flashDataEntry) == TRUE)
            {
                elt_cnt = flashDataEntry.ElementsCount;
                elt_sz  = flashDataEntry.ElementSize;
            }
            /* IF NvGetTableEntry returned FALSE, flashDataEntryForSave points to a structure with
               an invalid entry ID, a 0 ElementSize and 0 ElementsCount */
            flashDataEntryForSave = &flashDataEntry;
        }
        else
#endif /* gNvDualImageSupport_d */
        {
            /* common RAM table path: size comes from the RAM data table entry */
            elt_cnt = pNVM_DataTable[srcTblEntryIdx].ElementsCount;
            elt_sz  = pNVM_DataTable[srcTblEntryIdx].ElementSize;
#if gNvDualImageSupport_d
            flashDataEntryForSave = &pNVM_DataTable[srcTblEntryIdx];
#endif /* gNvDualImageSupport_d */
            /* RAM table was updated */
            if (mNvTableUpdated)
            {
                if (NvGetTableEntry(pNVM_DataTable[srcTblEntryIdx].DataEntryID, &flashDataEntry) == TRUE)
                {
                    if (pNVM_DataTable[srcTblEntryIdx].ElementsCount > flashDataEntry.ElementsCount)
                    {
                        /* fill the FLASH destination page with the default RAM value for the missing element(s) */
                        fillFromRAM = TRUE;
                    }
                }
            }
        }
        if (NvMultEltSzByNb(elt_cnt, elt_sz, max_val, &size) < 0)
        {
            status = gNVM_Error_c;
            break;
        }
#endif /* gNvUseExtendedFeatureSet_d */

        /* Function used to find all valid single offset value for a dataset, the offset value will be stored in
         * maNvRecordsCpyOffsets */
        status = NvGetMetaInfo(mNvActivePageId, ownerRecordMetaInfoOffset, &ownerRecordMetaInfo);
        if (status != gNVM_OK_c)
        {
            break;
        }

        (void)NvInternalRecordsUpdate(srcMetaOffset, srcTblEntryIdx, ownerRecordMetaInfoOffset,
                                      ownerRecordMetaInfo.u.fields.NvmDataEntryID);
        prog_offset = dstRecordOffset;
        while (size != 0U)
        {
#if gNvUseExtendedFeatureSet_d
            uint8_t *cpy_to;
            uint8_t *cpy_from;
            uint16_t elt_offset;

            space_left = PGM_SIZE_BYTE;

            FLib_MemSet(dstBuffer, 0xFF, PGM_SIZE_BYTE);

            /* fill the internal buffer */
            while ((0U != space_left) && (element_idx < elt_cnt))
            {
                bool_t full_element;
                cpy_to = &dstBuffer[(uint8_t)PGM_SIZE_BYTE - space_left];

#if gNvDualImageSupport_d
                /* plenty of space left to copy the rest of the element */
                if (space_left >= (flashDataEntryForSave->ElementSize - element_inner_copied))
                {
                    copy_amount  = flashDataEntryForSave->ElementSize - element_inner_copied;
                    full_element = TRUE;
                }
                else
                {
                    copy_amount  = space_left;
                    full_element = FALSE;
                }
                /* copy the rest of the data from the RAM entry */
                if (fillFromRAM && element_idx >= flashDataEntry.ElementsCount)
                {
                    if (NvComputeEltOffset(element_idx, flashDataEntryForSave->ElementSize, element_inner_copied, 0U,
                                           &elt_offset) < 0)
                    {
                        status = gNVM_Error_c;
                        break;
                    }
                    cpy_from = (uint8_t *)flashDataEntryForSave->pData + elt_offset;
                }
                else
                    /* copy from the owning full record save if no single save offset was found */
                    if (0U == maNvRecordsCpyOffsets[element_idx])
                    {
                        if (NvComputeEltOffset(element_idx, flashDataEntryForSave->ElementSize, element_inner_copied,
                                               ownerRecordMetaInfo.u.fields.NvmRecordOffset, &elt_offset) < 0)
                        {
                            status = gNVM_Error_c;
                            break;
                        }
                        cpy_from =
                            (uint8_t *)(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress + elt_offset);
                    }
                    else
                    {
                        if (NvComputeEltOffset(element_idx, 0U, element_inner_copied,
                                               maNvRecordsCpyOffsets[element_idx], &elt_offset) < 0)
                        {
                            status = gNVM_Error_c;
                            break;
                        }
                        cpy_from =
                            (uint8_t *)(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress + elt_offset);
                    }
#else  /* gNvDualImageSupport_d */
                /* plenty of space left to copy the rest of the element */
                if (space_left >= (pNVM_DataTable[srcTblEntryIdx].ElementSize - element_inner_copied))
                {
                    copy_amount  = pNVM_DataTable[srcTblEntryIdx].ElementSize - element_inner_copied;
                    full_element = TRUE;
                }
                else
                {
                    copy_amount  = space_left;
                    full_element = FALSE;
                }
                /* copy the rest of the data from the RAM entry */
                if (fillFromRAM && element_idx >= flashDataEntry.ElementsCount)
                {
                    if (NvComputeEltOffset(element_idx, flashDataEntryForSave->ElementSize, element_inner_copied, 0U,
                                           &elt_offset) < 0)
                    {
                        status = gNVM_Error_c;
                        break;
                    }
                    cpy_from = (uint8_t *)flashDataEntryForSave->pData + elt_offset;
                }
                else
                    /* copy from the owning full record save if no single save offset was found */
                    if (0U == maNvRecordsCpyOffsets[element_idx])
                    {
                        if (NvComputeEltOffset(element_idx, pNVM_DataTable[srcTblEntryIdx].ElementSize,
                                               element_inner_copied, ownerRecordMetaInfo.u.fields.NvmRecordOffset,
                                               &elt_offset) < 0)
                        {
                            status = gNVM_Error_c;
                            break;
                        }
                        cpy_from =
                            (uint8_t *)(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress + elt_offset);
                    }
                    else
                    {
                        if (NvComputeEltOffset(element_idx, 0U, element_inner_copied,
                                               maNvRecordsCpyOffsets[element_idx], &elt_offset) < 0)
                        {
                            status = gNVM_Error_c;
                            break;
                        }
                        cpy_from =
                            (uint8_t *)(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress + elt_offset);
                    }
#endif /* gNvDualImageSupport_d */
                FLib_MemCpy(cpy_to, cpy_from, copy_amount);

                if (full_element)
                {
                    space_left -= (uint8_t)copy_amount;
                    /* move to next element */
                    element_idx++;
                    element_inner_copied = 0U;
                }
                else
                {
                    element_inner_copied += (uint8_t)copy_amount;
                    break;
                }

            } /* inner while */
#endif        /* gNvUseExtendedFeatureSet_d */

            if (gNVM_OK_c != status)
            {
                break;
            }
            status = NV_PartitionProgramUnalignedAtOffset(dstPgId, prog_offset, PGM_SIZE_BYTE, dstBuffer);
            if (gNVM_OK_c != status)
            {
                /* avoid losing status value returned by NV_FlashProgramUnaligned, not necessarily
                 * gNVM_RecordWriteError_c
                 */
                break;
            }
            /* copied all the data, exit */
            if (size <= (uint16_t)PGM_SIZE_BYTE)
            {
                break;
            }
            prog_offset += (uint16_t)PGM_SIZE_BYTE;
            if (prog_offset >= mNvTotalPageSize)
            {
                status = gNVM_Error_c;
                break;
            }
            size -= (uint16_t)PGM_SIZE_BYTE;
        } /* outer while size */

        if (gNVM_OK_c != status)
        {
            break;
        }
        /* write meta information tag */
        /* CERT INT31-C: guard dstRecordOffset fits in uint16_t before cast */
        assert(dstRecordOffset <= (uint32_t)UINT16_MAX);
        NvSetMetaInfo(&dstMetaInfo, ownerRecordMetaInfo.u.fields.NvmDataEntryID, 0U,
                      (uint16_t)(dstRecordOffset & (uint32_t)UINT16_MAX), gValidationByteAllRecords_c);

        /* write the associated record meta information */
        status =
            NV_PartitionProgramAtOffset(dstPgId, dstMetaOffset, (uint8_t *)&dstMetaInfo, sizeof(NVM_RecordMetaInfo_t));
    } while (false);
    return status;
}
#endif /* gNvFragmentation_Enabled_d */

/******************************************************************************
 * Name: NvIsMetaInfoValid
 * Description: Performs to check is the meta is valid
 * Parameter(s): [IN] srcMetaOffset - offset of meta information from source page
 *               [IN] srcMetaInfo - point to the meta info
 *               [IN] srcTableEntryIdx - point to the variable which stored table entry index
 *               [IN] skipEntryId - point to the variable which stored the entry ID that should skip
 *               [IN] dstPageId - point to the variable which stored the target page ID
 * Return: the status of the operation.
 *         If FALSE the caller must decrement the source meta info addres itself.
 *****************************************************************************/
NVM_STATIC bool_t NvIsMetaInfoValid(uint32_t              srcMetaOffset,
                                    NVM_RecordMetaInfo_t *srcMetaInfo,
                                    uint16_t             *srcTableEntryIdx,
                                    NvTableEntryId_t      skipEntryId,
                                    NVM_VirtualPageID_t   dstPageId)
{
    bool_t state = TRUE;
#if gNvUseExtendedFeatureSet_d
    uint16_t idx;
    bool_t   entryFound;

#if gNvDualImageSupport_d
    NVM_DataEntry_t flashDataEntry;
#endif /* gNvDualImageSupport_d */
#endif /* gNvUseExtendedFeatureSet_d */

    /* do ... while(FALSE) for MISRA 15.5 control flow problem */
    do
    {
#if (defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d != 0))
        /* Verify the meta information checksum only if it has been written */

        if (srcMetaInfo->NvmMetaChecksum != 0xffffffffUL)
        {
            uint32_t checksum = NvCalculateChecksum(srcMetaInfo);
            if (checksum != 0xffffffffUL)
            {
                state = FALSE;
                break;
            }
        }
#endif

#if gNvUseExtendedFeatureSet_d
        /* NV RAM table has been updated */
        /* Check if meta info still in RAM table */
        if (mNvTableUpdated)
        {
            idx        = 0U;
            entryFound = FALSE;

            /* check if the saved entry is still present in the new RAM table */
            while (idx < mNVM_DataTableNbEntries)
            {
                if (srcMetaInfo->u.fields.NvmDataEntryID == pNVM_DataTable[idx].DataEntryID)
                {
                    entryFound = TRUE;
                    break;
                }
                idx++;
            }

            if (!entryFound)
            {
#if gNvDualImageSupport_d
                /* Not only check if the entry from RAM, but also check if it in NVM*/
                if (NvGetTableEntry(srcMetaInfo->u.fields.NvmDataEntryID, &flashDataEntry) == FALSE)
                {
                    /* The Table also not found from NVM */
                    state = FALSE;
                    break;
                }
#else  /* gNvDualImageSupport_d */
                /* move to the next meta info */
                state = FALSE;
                break;
#endif /* gNvDualImageSupport_d */
            }
        }
#endif /* gNvUseExtendedFeatureSet_d */

        /* get table entry index */
        *srcTableEntryIdx = NvGetTableEntryIndexFromId(srcMetaInfo->u.fields.NvmDataEntryID);
        /* Check if VSB ?= VEB */
        if (NvIsRecordCopied(dstPageId, srcMetaInfo) ||
            (srcMetaInfo->u.fields.NvValidationStartByte != srcMetaInfo->u.fields.NvValidationEndByte) ||
#if gNvDualImageSupport_d
            (srcMetaInfo->u.fields.NvmDataEntryID == skipEntryId)
#else  /* gNvDualImageSupport_d */
            (*srcTableEntryIdx == gNvInvalidDataEntry_c) || (srcMetaInfo->u.fields.NvmDataEntryID == skipEntryId)
#endif /* gNvDualImageSupport_d */
        )
        {
            /* go to the next meta information tag */
            state = FALSE;
            break;
        }

#if gUnmirroredFeatureSet_d
#if gNvDualImageSupport_d
        if (*srcTableEntryIdx != gNvInvalidTableEntryIndex_c)
        {
#endif /* gNvDualImageSupport_d */
            if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[*srcTableEntryIdx].DataEntryType)
            {
                /*check if the data was erased using NvErase or is just uninitialized*/
                if (NULL == ((void **)pNVM_DataTable[*srcTableEntryIdx].pData)[srcMetaInfo->u.fields.NvmElementIndex] &&
                    NvIsRecordErased(*srcTableEntryIdx, srcMetaInfo->u.fields.NvmElementIndex, srcMetaOffset))
                {
                    /* go to the next meta information tag */
                    state = FALSE;
                    break;
                }
            }
#if gNvDualImageSupport_d
        }
#endif /* gNvDualImageSupport_d */
#endif
    } while (FALSE);

    return state;
}
#if gNvUseExtendedFeatureSet_d
/******************************************************************************
 * Name: NvIsNvTableChanged
 * Description: Performs to check if the NvTable is changed
 * Parameter(s): [IN] srcMetaInfo - point to the meta info
 *               [IN] srcTableEntryIdx - stored table entry index
 *               [IN] tableUpgraded - point to the variable which stored the state of if a table upgrade has happened
 *               [IN] bytesToCopy - point to the variable which stored how many bytes should copy
 * Return: the status of the operation. If TRUE, let the caller (NvCopyPage) perform decrement the source meta info
 * address.
 *****************************************************************************/
NVM_STATIC bool_t NvIsNvTableChanged(NVM_RecordMetaInfo_t *srcMetaInfo,
                                     uint16_t              srcTableEntryIdx,
                                     bool_t                tableUpgraded,
                                     uint16_t             *bytesToCopy)
{
    bool_t status = FALSE;

    NVM_DataEntry_t flashDataEntry;

    /* do ... while(FALSE) for MISRA 15.5 control flow problem */
    do
    {
        uint32_t max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
        /* NV RAM table has been updated */
        if (mNvTableUpdated)
        {
            if (NvGetTableEntry(pNVM_DataTable[srcTableEntryIdx].DataEntryID, &flashDataEntry) == TRUE)
            {
                /* entries changed from mirrored/unmirrored and with different entry size cannot be recovered */
                if (((((uint16_t)gNVM_MirroredInRam_c == flashDataEntry.DataEntryType) ||
                      ((uint16_t)gNVM_MirroredInRam_c == pNVM_DataTable[srcTableEntryIdx].DataEntryType)) &&
                     (flashDataEntry.DataEntryType != pNVM_DataTable[srcTableEntryIdx].DataEntryType)) ||
                    (flashDataEntry.ElementSize != pNVM_DataTable[srcTableEntryIdx].ElementSize))
                {
                    status = TRUE;
                    break;
                }

                if (flashDataEntry.ElementsCount != pNVM_DataTable[srcTableEntryIdx].ElementsCount)
                {
                    if (tableUpgraded)
                    {
                        uint16_t sz = 0U;
                        if (NvMultEltSzByNb(flashDataEntry.ElementsCount, flashDataEntry.ElementSize, max_val, &sz) < 0)
                        {
                            assert(false);
                            status = TRUE;
                            break;
                        }
                        if (flashDataEntry.ElementsCount < pNVM_DataTable[srcTableEntryIdx].ElementsCount)
                        {
                            /* copy only the bytes that were previously written to FLASH virtual page */
                            *bytesToCopy = sz;
                        }
#if gNvFragmentation_Enabled_d
                        /*ignore if out of bounds*/
                        if (srcMetaInfo->u.fields.NvValidationStartByte == gValidationByteSingleRecord_c &&
                            srcMetaInfo->u.fields.NvmElementIndex >= pNVM_DataTable[srcTableEntryIdx].ElementsCount)
                        {
                            status = TRUE;
                            break;
                        }
#endif
                    }
                    else
                    {
                        status = TRUE;
                        break;
                    }
                }
            }
        }
    } while (FALSE);
    return status;
}
#endif /* gNvUseExtendedFeatureSet_d */

/******************************************************************************
 * Name: NvCopyRecord
 * Description: Performs to copy the record to another page
 * Parameter(s): [IN] dstMetaAddress - point to the variable which stored the target meta info address
 *               [IN] srcMetaAddress - point to the variable which stored meta info address
 *               [IN] srcMetaInfo - point to the meta info
 *               [IN] dstRecordAddress - point to the variable which stored the target record address
 *               [IN] srcTableEntryIdx - point to the variable which stored table entry index
 *               [IN] tblEntryMetaOffset - point to the variable which stored table entry meta offset
 *               [IN] tableUpgraded - point to the variable which stored the state of if a table upgrate has happened
 *               [IN] bytesToCopy - point to the variable which stored how many bytes should copy
 * Return: the status of the operation:
 *               gNVM_OK_c
 *               gNVM_RecordWriteError_c
 *               gNVM_MetaInfoWriteError_c
 *               gNVM_EccFault_c (ECC check only)
 *               gNVM_EccFaultWritingRecord_c (ECC check only)
 *               gNVM_EccFaultWritingMeta_c (ECC check only)
 *               gNVM_AlignmentError_c (defragmentation only)
 *
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvCopyRecord(uint16_t             *dstMetaOffset,
                                     uint16_t             *srcMetaOffset,
                                     NVM_RecordMetaInfo_t *srcMetaInfo,
                                     uint16_t             *dstRecordOffset,
                                     uint16_t             *srcTableEntryIdx,
#if gNvFragmentation_Enabled_d
                                     uint16_t *tblEntryMetaOffset,
#endif /* gNvFragmentation_Enabled_d */
#if gNvUseExtendedFeatureSet_d
                                     bool_t *tableUpgraded,
#endif /* gNvUseExtendedFeatureSet_d */
                                     uint16_t *bytesToCopy)
{
    NVM_Status_t status = gNVM_OK_c;

    do
    {
        uint16_t sz = 0U;
        uint16_t rounded_sz;
        uint16_t data_offset;

        data_offset = *dstRecordOffset;
        /* if the copy operation must take elements from ram */
#if gNvUseExtendedFeatureSet_d
        uint32_t max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);

#if gNvDualImageSupport_d
        NVM_DataEntry_t flashDataEntry;

        FLib_MemSet(&flashDataEntry, 0U, sizeof(NVM_DataEntry_t));

        /* if the srcTableEntryIdx is invalid, it means the entry is from NVM,
         * then it is not needed to check if NvTable is changed from RAM  */
        if (*srcTableEntryIdx == gNvInvalidTableEntryIndex_c)
        {
            if (NvGetTableEntry(srcMetaInfo->u.fields.NvmDataEntryID, &flashDataEntry))
            {
                if (NvMultEltSzByNb(flashDataEntry.ElementsCount, flashDataEntry.ElementSize, max_val, &sz) < 0)
                {
                    status = gNVM_Error_c;
                    assert(false);
                    break;
                }
                if (!(mNvTableUpdated && *tableUpgraded && (*bytesToCopy < sz)))
                {
                    /* make sure the address can hold the entire space (+ what is taken from ram) */
                    sz = *bytesToCopy;
                }
                rounded_sz = NvUpdateSize(sz);

                if (data_offset < rounded_sz)
                {
                    status = gNVM_Error_c;
                    assert(false); /* catches the case where NvUpdateSize returned 0xffff */
                    break;
                }
                data_offset -= rounded_sz;
                *dstRecordOffset = data_offset;
            }
            else
            {
                /* Entry was not found, so flashDataEntry remained empty */
                status = gNVM_Error_c;
                break;
            }
            status = gNVM_OK_c;
            break;
        }
#endif /* gNvDualImageSupport_d */
        if (NvMultEltSzByNb(pNVM_DataTable[*srcTableEntryIdx].ElementsCount,
                            pNVM_DataTable[*srcTableEntryIdx].ElementSize, max_val, &sz) < 0)
        {
            status = gNVM_Error_c;
            assert(false);
            break;
        }
        if ((mNvTableUpdated && *tableUpgraded && (*bytesToCopy < sz)))
        {
            /* keep the original size derived pNVM_DataTable element count and size */
            /* make sure the address can hold the entire space (+ what is taken from ram) */
        }
        else
#endif /* gNvUseExtendedFeatureSet_d */
        {
            /* use bytesToCopy argument as size */
            sz = *bytesToCopy;
        }

        /* Round the size */
        rounded_sz = NvUpdateSize(sz);
        if (data_offset < rounded_sz)
        {
            status = gNVM_Error_c;
            assert(false);
            break;
        }
        /* compute the destination record start address */
        data_offset -= rounded_sz;
        *dstRecordOffset = data_offset;
    } while (false);

    if (gNVM_OK_c == status)
    {
#if gNvFragmentation_Enabled_d
        /*
         * single element record
         */
        if (srcMetaInfo->u.fields.NvValidationStartByte == gValidationByteSingleRecord_c)
        {
            status = NvInternalDefragmentedCopy(*srcMetaOffset, *srcTableEntryIdx, *dstMetaOffset, *dstRecordOffset,
                                                *tblEntryMetaOffset);
        }
        else
#endif /* gNvFragmentation_Enabled_d */
        /*
         * full table entry
         */
        {
            status = NvInternalCopy(*dstRecordOffset, *dstMetaOffset, srcMetaInfo, *srcTableEntryIdx,
                                    (uint16_t)(*bytesToCopy));
        }
    }
    return status;
}

/******************************************************************************
 * Name: NvCopyPage
 * Description: Copy the active page content to the mirror page. Only the
 *              latest table entries / elements are copied. A merge operation
 *              is performed before copy if an entry has single elements
 *              saved priorly and newer than the table entry. If one or more
 *              elements were singular saved and the NV page doesn't have a
 *              full table entry saved, then the elements are copied as they
 *              are.
 * Parameter(s): [IN] skipEntryId - the entry ID to be skipped when page
 *                                  copy is performed
 * Return: gNVM_InvalidPageID_c - if the source or destination page is not
 *                                valid
 *         gNVM_MetaInfoWriteError_c - if the meta information couldn't be
 *                                     written
 *         gNVM_RecordWriteError_c - if the record couldn't be written
 *         gNVM_Error_c - in case of error(s)
 *         gNVM_OK_c - page copy completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvCopyPage(NvTableEntryId_t skipEntryId)
{
    /* source page related variables */
    uint16_t             srcMetaOffset;
    NVM_RecordMetaInfo_t srcMetaInfo = {0U};
    uint16_t             srcTableEntryIdx;

    /* destination page related variables */
    uint16_t            dstMetaOffset;
    uint16_t            firstMetaOffset;
    NVM_VirtualPageID_t dstPageId;
    uint16_t            dstRecordOffset;
#if gNvUseExtendedFeatureSet_d
    bool_t tableUpgraded = FALSE;
#endif /* gNvUseExtendedFeatureSet_d */
#if gNvFragmentation_Enabled_d
    uint16_t tblEntryMetaOffset = gNvInvalidMetaOffset_c;
#endif
    uint16_t bytesToCopy = 0U;
#if gNvDualImageSupport_d
    NVM_DataEntry_t flashDataEntry;
#endif /* gNvDualImageSupport_d */

    /* status variable */
    NVM_Status_t status  = gNVM_OK_c;
    uint32_t     max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);

    dstPageId = OTHER_PAGE_ID(mNvActivePageId);
    /* Check if the destination page is blank. If not, erase it. */
    if (gNVM_PageIsNotBlank_c == NvVirtualPageBlankCheck(dstPageId))
    {
        status = NvEraseVirtualPage(dstPageId);
    }
    if (gNVM_OK_c == status)
    {
        /* initialise the destination page meta info start address */
        /* CERT INT31-C / MISRA 10.3: guard gNvFirstMetaOffset_c fits in uint16_t */
        assert(gNvFirstMetaOffset_c <= (uint32_t)UINT16_MAX);
        dstMetaOffset = (gNvFirstMetaOffset_c <= (uint32_t)UINT16_MAX) ? (uint16_t)gNvFirstMetaOffset_c : (uint16_t)0U;
#if gNvDualImageSupport_d
        uint16_t extra_offset = 0U;

        /* Need to determine mNvNeedAddEntryCnt */
        (void)NvGetEntryInfoNeedToAddInNVM();
        /* MISRA 10.3: explicit cast of sizeof to uint16_t */
        if (NvMultEltSzByNb(mNvNeedAddEntryCnt, (uint16_t)sizeof(NVM_TableInfo_t), max_val, &extra_offset) < 0)
        {
            assert(false);
        }
        /* CERT INT30-C: guard addition does not wrap uint16_t */
        else if (extra_offset <= ((uint16_t)UINT16_MAX - dstMetaOffset))
        {
            dstMetaOffset += extra_offset;
        }
        else
        {
            assert(false); /* overflow: extra_offset + dstMetaOffset would exceed UINT16_MAX */
        }
#endif                     /* gNvDualImageSupport_d */
#if gNvUseExtendedFeatureSet_d
        if (mNvTableUpdated)
        {
            tableUpgraded = (GetFlashTableVersion() != mNvFlashTableVersion);
        }
#endif

        firstMetaOffset = dstMetaOffset;
        /*if src is an empty page, just copy the table and make the initializations*/
        srcMetaOffset = mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset;
        if (srcMetaOffset != gNvInvalidMetaOffset_c)
        {
            /* initialise the destination page record start address */
            /* CERT INT31-C / MISRA 10.3: guard subtraction fits in uint16_t */
            {
                uint32_t dstRec32 = (mNvTotalPageSize >= (uint32_t)sizeof(NVM_TableInfo_t)) ?
                                        (mNvTotalPageSize - (uint32_t)sizeof(NVM_TableInfo_t)) :
                                        0U;
                assert(dstRec32 <= (uint32_t)UINT16_MAX);
                dstRecordOffset = (dstRec32 <= (uint32_t)UINT16_MAX) ? (uint16_t)dstRec32 : (uint16_t)0xFFFFU;
            }
            /* gNvFirstMetaOffset_c is dependent on mNvTableSizeInFlash, which must have been updated beforehand.
             * CERT INT30-C: the plain decrement cannot underflow. Both NVM_RecordMetaInfo_t and NVM_TableInfo_t
             * are PGM_SIZE_BYTE in size, so gNvFirstMetaOffset_c (>= sizeof(NVM_TableInfo_t) when the extended
             * feature set is disabled, and 2*sizeof(NVM_TableInfo_t) + mNvTableSizeInFlash otherwise) is always
             * >= sizeof(NVM_RecordMetaInfo_t). The smallest offs that satisfies the loop condition is therefore
             * gNvFirstMetaOffset_c; subtracting one step yields a non-negative value strictly below
             * gNvFirstMetaOffset_c, which terminates the loop without wraparound. */
            for (uint16_t offs = srcMetaOffset; offs >= gNvFirstMetaOffset_c;
                 offs -= (uint16_t)sizeof(NVM_RecordMetaInfo_t))
            {
                srcMetaOffset = offs;
                /* get current meta information */
                status = NvGetMetaInfo(mNvActivePageId, srcMetaOffset, &srcMetaInfo);
                /* Presumably the ECC fault could only happen at the latest record write.
                 * This must denote that the writing of the meta data failed. Skip this failed write.
                 * The error must have been detected synchronously so the write operation should still be pending
                 * in the queue. End the copy first then will reattempt this failed write operation. Likewise, if
                 * the ECC error has occurred while writing the record contents, not its meta, the operation was
                 * aborted and will be reattempted naturally because the order is still pending in the queue.
                 */
                if (gNVM_OK_c != status)
                {
                    continue;
                }

                /* Check if meta info is valid */
                if (!NvIsMetaInfoValid(srcMetaOffset, &srcMetaInfo, &srcTableEntryIdx, skipEntryId, dstPageId))
                {
                    continue;
                }
#if gNvDualImageSupport_d
                /* if meta info is valid, but the srcTableEntryIdx is invalid, it means the entry is from NVM,
                   then it not need to check if NvTable is changed from RAM  */
                if (srcTableEntryIdx == gNvInvalidTableEntryIndex_c)
                {
                    (void)NvGetTableEntry(srcMetaInfo.u.fields.NvmDataEntryID, &flashDataEntry);
                    /* compute the destination record start address */
                    /* CERT INT31-C / MISRA 10.3: NvMultEltSzByNb computes ElementsCount * ElementSize and
                     * guards the product against max_val, so it cannot exceed the uint16_t range. */
                    bytesToCopy = 0U;
                    if (NvMultEltSzByNb(flashDataEntry.ElementsCount, flashDataEntry.ElementSize, max_val,
                                        &bytesToCopy) < 0)
                    {
                        assert(false);
                        status = gNVM_Error_c;
                        break;
                    }
                }
                else
                {
#endif /* gNvDualImageSupport_d */
                    bytesToCopy = 0U;
                    if (NvMultEltSzByNb(pNVM_DataTable[srcTableEntryIdx].ElementsCount,
                                        pNVM_DataTable[srcTableEntryIdx].ElementSize, max_val, &bytesToCopy) < 0)
                    {
                        assert(false);
                        status = gNVM_Error_c;
                        break;
                    }

                    /* Check if NvTable is changed */
#if gNvUseExtendedFeatureSet_d
                    if (NvIsNvTableChanged(&srcMetaInfo, srcTableEntryIdx, tableUpgraded, &bytesToCopy))
                    {
                        continue;
                    }
#endif /* gNvUseExtendedFeatureSet_d */
#if gNvDualImageSupport_d
                }
#endif /* gNvDualImageSupport_d */

#if gNvFragmentation_Enabled_d
                if (srcMetaInfo.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c)
                {
#if gUnmirroredFeatureSet_d

#if gNvDualImageSupport_d
                    /* if meta info is valid, but the srcTableEntryIdx is invalid, it means the entry is from NVM,
                       then it not need to check if NvTable is changed from RAM  */
                    if (srcTableEntryIdx == gNvInvalidTableEntryIndex_c)
                    {
                        if ((uint16_t)gNVM_MirroredInRam_c != flashDataEntry.DataEntryType)
                        {
                            tblEntryMetaOffset = gNvInvalidMetaOffset_c;
                        }
                        else
                        {
                            tblEntryMetaOffset =
                                NvGetTblEntryMetaOffsetFromId(srcMetaOffset, srcMetaInfo.u.fields.NvmDataEntryID);
                        }
                    }
                    else
#endif /* gNvDualImageSupport_d */
                        if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[srcTableEntryIdx].DataEntryType)
                        {
                            tblEntryMetaOffset = gNvInvalidMetaOffset_c;
                        }
                        else
#endif
                        {
                            tblEntryMetaOffset =
                                NvGetTblEntryMetaOffsetFromId(srcMetaOffset, srcMetaInfo.u.fields.NvmDataEntryID);
                        }

                    if (NvLookAheadInPendingSaveQueue(srcMetaInfo.u.fields.NvmDataEntryID,
                                                      srcMetaInfo.u.fields.NvmElementIndex) != (uint8_t)OP_NONE)
                    {
                        /* skip and continue : either we found a pending save operation that will obliterate the
                         * same element or the whole array */
                        /* move to the next meta info */
                        continue;
                    }

                    /* if the record has no full entry associated perform simple copy */
                    if (tblEntryMetaOffset == gNvInvalidMetaOffset_c)
                    {
                        /* compute the 'real record size' taking into consideration that the flash controller only
                         * writes in burst of whole phrases (PGM_SIZE_BYTE bytes) */
#if gNvDualImageSupport_d
                        /* if the srcTableEntryIdx is invalid, it means the entry is from NVM,
                           then it not need to check if NvTable is changed from RAM  */
                        if (srcTableEntryIdx == gNvInvalidTableEntryIndex_c)
                        {
                            /* compute the destination record start address */
                            bytesToCopy = flashDataEntry.ElementSize;
                        }
                        else
                        {
#endif /* gNvDualImageSupport_d */
                            bytesToCopy = pNVM_DataTable[srcTableEntryIdx].ElementSize;

#if gNvDualImageSupport_d
                        }
#endif /* gNvDualImageSupport_d */
                        /* CERT INT30-C / CERT INT31-C / MISRA 10.3: guard NvUpdateSize fits in uint16_t and subtraction
                         * does not wrap */
                        {
                            uint16_t updateSz = NvUpdateSize(bytesToCopy);
                            if (dstRecordOffset >= updateSz)
                            {
                                dstRecordOffset -= updateSz;
                            }
                            else
                            {
                                assert(false); /* If NvUpdateSize returns 0xffff it is caught here */
                                status = gNVM_AddressOutOfRange_c;
                                break;
                            }
                        }
                        status =
                            NvInternalCopy(dstRecordOffset, dstMetaOffset, &srcMetaInfo, srcTableEntryIdx, bytesToCopy);
                        if (gNVM_OK_c != status)
                        {
                            /* If error is caused by ECC when reading the source to be copied just skip record, but if
                             * it occurs during programming phase it is more severe */
                            if (gNVM_EccFaultWritingMeta_c == status || gNVM_EccFaultWritingRecord_c == status)
                            {
                                /* The error happened when reading back after write: desperate case, bailing out */
                                assert(false);
#if defined gNvDebugEnabled_d && (gNvDebugEnabled_d > 0)
                                NV_ShowPageMetas(dstPageId, true);
#endif
                                /* Break for ECC errors during programming operation */
                                break;
                            }
                            else
                            {
                                /* if gNVM_EccFault_c : ECC error happened on read, losing the original data but
                                 * continue like for other errors */
                                /* skip and move to the next meta info */
                                continue;
                            }
                        }
#if gUnmirroredFeatureSet_d
#if gNvDualImageSupport_d
                        /* if the srcTableEntryIdx is invalid, it means the entry is from NVM,
                         then it not need to check if NvTable is changed from RAM  */
                        if (srcTableEntryIdx == gNvInvalidTableEntryIndex_c)
                        {
                            /* no action needed */
                        }
                        else
                        {
#endif /* gNvDualImageSupport_d */
                            if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[srcTableEntryIdx].DataEntryType)
                            {
                                uint32_t dstRecordAddress = 0U;
                                if (NvAddOffsetToAddr(mNvVirtualPageProperty[dstPageId].NvRawSectorStartAddress,
                                                      dstRecordOffset, &dstRecordAddress) != 0)
                                {
                                    status = gNVM_Error_c;
                                    break;
                                }
                                OSA_InterruptDisable();
                                /* set the pointer to the flash data */
                                if (NvIsNVMFlashAddress(((void **)pNVM_DataTable[srcTableEntryIdx]
                                                             .pData)[srcMetaInfo.u.fields.NvmElementIndex]))
                                {
                                    ((uint8_t **)pNVM_DataTable[srcTableEntryIdx]
                                         .pData)[srcMetaInfo.u.fields.NvmElementIndex] = (uint8_t *)dstRecordAddress;
                                }
                                OSA_InterruptEnable();
                            }
#if gNvDualImageSupport_d
                        }
#endif /* gNvDualImageSupport_d */
#endif
                        /* update destination meta information address */
                        /* MISRA 10.3: explicit cast, guard no wrap */
                        assert(dstMetaOffset <= ((uint16_t)UINT16_MAX - (uint16_t)sizeof(NVM_RecordMetaInfo_t)));
                        dstMetaOffset = (uint16_t)(dstMetaOffset + (uint16_t)sizeof(NVM_RecordMetaInfo_t));

                        /* move to the next meta info */
                        continue;
                    } /* (tblEntryMetaOffset == gNvInvalidMetaOffset_c) */
                    else
                    {
                        if (mNvVirtualPageProperty[mNvActivePageId].has_ecc_faults)
                        {
                            /* Skip record data if contents has ECC fault: needs to be done only if page has faults  */
                            uint8_t  cacheBuffer[gNvCacheBufferSize_c] = {0U};
                            uint32_t read_offset                       = srcMetaInfo.u.fields.NvmRecordOffset;
                            uint16_t remaining_sz                      = pNVM_DataTable[srcTableEntryIdx].ElementSize;

                            while (remaining_sz > 0U)
                            {
                                uint16_t rd_sz = MIN(remaining_sz, gNvCacheBufferSize_c);
                                status = NV_PartitionReadAtOffset(mNvActivePageId, read_offset, cacheBuffer, rd_sz);
                                if (gNVM_OK_c != status)
                                {
                                    break;
                                }
                                read_offset += gNvCacheBufferSize_c;
                                remaining_sz -= rd_sz;
                            }
                            if (gNVM_OK_c != status)
                            {
                                /* move to the next meta info */
                                continue;
                            }
                        }
                    } /* else !(tblEntryMetaOffset == gNvInvalidMetaOffset_c) */
                }     /* (srcMetaInfo.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c) */
#endif                /* gUnmirroredFeatureSet_d */
                /* Copy record operation */
                status = NvCopyRecord(&dstMetaOffset, &srcMetaOffset, &srcMetaInfo, &dstRecordOffset, &srcTableEntryIdx,
#if gNvFragmentation_Enabled_d
                                      &tblEntryMetaOffset,
#endif
#if gNvUseExtendedFeatureSet_d
                                      &tableUpgraded,
#endif
                                      &bytesToCopy);
                if (gNVM_OK_c != status)
                {
                    if (gNVM_EccFaultWritingMeta_c == status || gNVM_EccFaultWritingRecord_c == status)
                    {
                        /* The error happened when reading back after write: desperate case, bailing out */
                        assert(false);
#if defined gNvDebugEnabled_d && (gNvDebugEnabled_d > 0)
                        NV_ShowPageMetas(dstPageId, true);
#endif
                        break;
                    }
                    else
                    {
                        /* gNVM_EccFault_c : ECC error happened on read, losing the original data but continue like for
                         * other errors */
                        continue;
                    }
                }

                /* update destination meta information address */
                /* MISRA 10.3: explicit cast of sizeof to uint16_t, guard no wrap */
                assert(dstMetaOffset <= ((uint16_t)UINT16_MAX - (uint16_t)sizeof(NVM_RecordMetaInfo_t)));
                dstMetaOffset = (uint16_t)(dstMetaOffset + (uint16_t)sizeof(NVM_RecordMetaInfo_t));

                /* move to the next meta info */
            }
            if (gNVM_EccFault_c == status)
            {
                /* Ignore bad record if any, they are just skipped but continue */
                status = gNVM_OK_c;
            }
        } /* srcMetaOffset != gNvInvalidMetaOffset_c */

        if (gNVM_OK_c == status)
        {
            /* update the last meta info address */
            if (dstMetaOffset == firstMetaOffset)
            {
                mNvVirtualPageProperty[dstPageId].NvLastMetaInfoOffset = gNvInvalidMetaOffset_c;
            }
            else
            {
                mNvVirtualPageProperty[dstPageId].NvLastMetaInfoOffset =
                    (uint16_t)(dstMetaOffset - sizeof(NVM_RecordMetaInfo_t));
            }

#if gUnmirroredFeatureSet_d
            mNvVirtualPageProperty[dstPageId].NvLastMetaUnerasedInfoOffset =
                mNvVirtualPageProperty[dstPageId].NvLastMetaInfoOffset;
#endif

            mNvPageCounter++;
            /* save the current RAM table */
#if gNvDualImageSupport_d
            /* Set mNvPreviousActivePageId but postpone mNvActivePageId update until NvSaveRamTable status is known.
             mNvPreviousActivePageId is used in NvSaveRamTable.
            */
            mNvPreviousActivePageId = mNvActivePageId;
#endif /* gNvDualImageSupport_d */
            status = NvSaveRamTable(dstPageId);
            if (gNVM_OK_c == status)
            {
#if gNvUseExtendedFeatureSet_d
                if (mNvTableUpdated)
                {
                    /* update the size of the NV table stored in FLASH */
                    mNvTableSizeInFlash = NvGetFlashTableSize();

                    /* clear the flag */
                    mNvTableUpdated = FALSE;
                }
#endif /* gNvUseExtendedFeatureSet_d */
                /* make a request to erase the old page */
                mNvErasePgCmdStatus.NvPageToErase  = mNvActivePageId;
                mNvErasePgCmdStatus.NvSectorIndex  = 0U;
                mNvErasePgCmdStatus.NvErasePending = TRUE;

                /* update the the active page ID */

                mNvActivePageId = dstPageId;
            }
            else
            {
                assert(false);
#if defined gNvDebugEnabled_d && (gNvDebugEnabled_d > 0)
                NV_ShowPageMetas(dstPageId, true);
#endif
            }
        }
    }
    return status;
}

/******************************************************************************
 * Name: NvInternalFormat
 * Description: Format the NV storage system. The function erases in place both
 *              virtual pages and then writes the page counter value to first
 *              virtual page. The provided page counter value is automatically
 *              incremented and then written to first (active) virtual page.
 * Parameter(s): [IN] pageCounterValue - the page counter value that will
 *                                       be incremented and then written to
 *                                       active page
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_FormatFailure_c - if the format operation fails
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvInternalFormat(uint32_t pageCounterValue)
{
    uint8_t      retryCount = gNvFormatRetryCount_c;
    NVM_Status_t status;

    /* increment the page counter value */
    if (pageCounterValue == (uint32_t)gPageCounterMaxValue_c - 1U)
    {
        pageCounterValue = 1U;
    }
    else
    {
        pageCounterValue++;
    }
    mNvPageCounter = pageCounterValue;

    while (retryCount-- != 0U)
    {
        /* erase first page */
        if (gNVM_OK_c == NvEraseVirtualPage(gFirstVirtualPage_c))
        {
            if (gNVM_OK_c == NvEraseVirtualPage(gSecondVirtualPage_c))
            {
                break;
            }
        }
    }
#if gNvDualImageSupport_d
    mNvPreviousActivePageId = gVirtualPageNone_c;
#endif /* gNvDualImageSupport_d */
    /* active page after format = first virtual page */
    mNvActivePageId = gFirstVirtualPage_c;

    /* save NV table from RAM memory to FLASH memory */
    status = NvSaveRamTable(mNvActivePageId);
    if (gNVM_OK_c != status)
    {
        status = gNVM_FormatFailure_c;
    }
    else
    {
#if gNvUseExtendedFeatureSet_d
        /* update the size of the NV table stored in FLASH */
        mNvTableSizeInFlash = NvGetFlashTableSize();
#endif

        /* update the page counter value */
        mNvPageCounter = pageCounterValue;

        status = NvUpdateLastMetaInfoAddress();
    }
    return status;
}

/******************************************************************************
 * Name: NvSaveRamTable
 * Description: Saves the NV table. Called from NvCopyPage.
 *              Check flash program operation.
 * \note:
 * If called with gNvDualImageSupport_d option enabled mNvPreviousActivePageId,
 * is equal to gVirtualPageNone_c when coming from InternalFormat procedure
 * otherwise it is equal to mNvActivePageId.
 * Parameter(s): [IN] pageId - the virtual page ID where the table will be
 *                             saved
 * Return: gNVM_OK_c if table saved successfully, other error statuses otherwise.
 ******************************************************************************/
NVM_STATIC NVM_Status_t NvSaveRamTable(NVM_VirtualPageID_t pageId)
{
    NVM_Status_t status;
    uint32_t     write_offset = 0U;
    assert(NULL != pNVM_DataTable);
    do
    {
        NVM_TableInfo_t tbInfo;
        /* write table qualifier start */
        FLib_MemSet((uint8_t *)&tbInfo, 0xffU, sizeof(NVM_TableInfo_t));
        /* Page counter exists whether gNvUseExtendedFeatureSet_d is defined or not */
        tbInfo.u.fields.NvPageCounter = mNvPageCounter;
#if gNvUseExtendedFeatureSet_d
        tbInfo.u.fields.NvTableMarker  = mNvTableMarker;
        tbInfo.u.fields.NvTableVersion = mNvFlashTableVersion;
#endif
        /*write page counter, table marker, table version top*/
        status = NV_PartitionProgramAtOffset(pageId, write_offset, (uint8_t *)&tbInfo, sizeof(NVM_TableInfo_t));
        if (gNVM_OK_c != status)
        {
            break;
        }
#if gNvUseExtendedFeatureSet_d
        write_offset += sizeof(NVM_TableInfo_t);

#if gNvDualImageSupport_d
        status = NvSaveAllDataSetEntry(pageId, &write_offset);
        if (gNVM_OK_c != status)
        {
            break;
        }
#else  /* gNvDualImageSupport_d */
        for (uint16_t idx = 0U; idx < mNVM_DataTableNbEntries; idx++)
        {
            NVM_EntryInfo_t entryInfo;
            /* write data entry ID */
            NvInitializeEntryInfo(&entryInfo, 0xffU);
            /* Create empty entries so as to 'pre-reserve the space for table entries */
            entryInfo.u.fields.NvDataEntryID   = pNVM_DataTable[idx].DataEntryID;
            entryInfo.u.fields.NvDataEntryType = pNVM_DataTable[idx].DataEntryType;
            entryInfo.u.fields.NvElementsCount = pNVM_DataTable[idx].ElementsCount;
            entryInfo.u.fields.NvElementSize   = pNVM_DataTable[idx].ElementSize;

            status = NV_PartitionProgramAtOffset(pageId, write_offset, (uint8_t *)&entryInfo, sizeof(NVM_EntryInfo_t));
            if (gNVM_OK_c != status)
            {
                break;
            }
            /* increment address */
            write_offset += sizeof(NVM_EntryInfo_t);

            /* increment table entry index */
        }
        /* We may have exited the for loop with an error status */
        if (gNVM_OK_c != status)
        {
            break;
        }
#endif /* gNvDualImageSupport_d */

        FLib_MemSet((uint8_t *)&tbInfo, 0xffU, sizeof(NVM_TableInfo_t));
        tbInfo.u.fields.NvPageCounter = 0U;
#if gNvUseExtendedFeatureSet_d
        tbInfo.u.fields.NvTableMarker  = mNvTableMarker;
        tbInfo.u.fields.NvTableVersion = 0U;
#endif
        /* write table qualifier end, the rest 6 bytes are left 0x00 */
        status = NV_PartitionProgramAtOffset(pageId, write_offset, (uint8_t *)&tbInfo, sizeof(NVM_TableInfo_t));
        if (gNVM_OK_c != status)
        {
            break;
        }
#endif
        /*write page counter bottom*/
        FLib_MemSet((uint8_t *)&tbInfo, 0xffU, sizeof(NVM_TableInfo_t));
        tbInfo.u.fields.NvPageCounter = mNvPageCounter;
        write_offset                  = mNvTotalPageSize - sizeof(NVM_TableInfo_t);
        status = NV_PartitionProgramAtOffset(pageId, write_offset, (uint8_t *)&tbInfo, sizeof(NVM_TableInfo_t));
    } while (FALSE);

    return status;
}

/******************************************************************************
 * Name: NvGetEntryFromDataPtr
 * Description: get table and element indexes based on a generic pointer address
 * Parameter(s): [IN] pData - a pointer to a NVM RAM table
 *               [OUT] pIndex - a pointer to a memory location where the
 *                              requested indexed will be stored
 * Return: gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PointerOutOfRange_c - if the provided pointer cannot be found
 *                                    within the RAM table
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetEntryFromDataPtr(void *pData, NVM_TableEntryInfo_t *pIndex)
{
    NVM_Status_t status;
    if ((NULL == pData) || (NULL == pIndex))
    {
        /* A NULL pData or pIndex pointer are bad arguments so return error */
        status = gNVM_NullPointer_c;
    }
    else
    {
        status = gNVM_PointerOutOfRange_c;
        /* Convert pointer to integer for comparison - MISRA compliant approach */
        uintptr_t dataAddr = (uintptr_t)(uint8_t *)pData;
        uint32_t  max_val  = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);

        for (uint16_t idx = 0U; idx < mNVM_DataTableNbEntries; idx++)
        {
            uintptr_t tableEntryAddr;
            uintptr_t endAddr;
            uint16_t  elt_sz;
            uint16_t  tb_sz;
            /* parse NVM Table of data entries */
            if ((pNVM_DataTable[idx].pData == NULL) || (pNVM_DataTable[idx].ElementSize == 0U) ||
                (pNVM_DataTable[idx].ElementsCount == 0U))
            {
                /* ensures among other things that elt_sz cannot be 0 */
                continue; /* Skip invalid table entries */
            }
            tableEntryAddr = (uintptr_t)(uint8_t *)pNVM_DataTable[idx].pData;
#if gUnmirroredFeatureSet_d
            if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[idx].DataEntryType)
            {
                elt_sz = (uint16_t)sizeof(void *);
            }
            else
#endif
            {
                elt_sz = (uint16_t)pNVM_DataTable[idx].ElementSize;
                /* elt_sz cannot be 0 due to earlier continue statement,
                 * invalid entries already skipped */
            }
            /* Determining the end address of the table entry varies based on data entry type,
             * whether mirrored in RAM or not */
            if (NvMultEltSzByNb(pNVM_DataTable[idx].ElementsCount, elt_sz, max_val, &tb_sz) < 0)
            {
                status = gNVM_PointerOutOfRange_c;
                break;
            }
            endAddr = tableEntryAddr + (uintptr_t)tb_sz;

            /* Use integer comparison instead of pointer comparison */
            if ((dataAddr >= tableEntryAddr) && (dataAddr < endAddr))
            {
                uint16_t offset;
                uint16_t index;
                /* Checked that pData is within the table entry's address range, so greater than or equal to
                 * tableEntryAddr. offset guaranteed to be positive due to previous comparison.
                 */
                offset = (uint16_t)(((uint32_t)dataAddr - (uint32_t)tableEntryAddr) & 0xffffUL);
                /* Use integer arithmetic instead of pointer subtraction */

                index                = offset / elt_sz;
                pIndex->elementIndex = index;
                pIndex->entryId      = pNVM_DataTable[idx].DataEntryID;
                status               = gNVM_OK_c;
                break; /* Exit loop after finding matching entry */
            }
            /* Continue loop if not found */
        } /* End of for loop idx incremented */
    }
    return status;
}
/******************************************************************************
 * Name: NvGetTableEntryIndexFromDataPtr
 * Description: get table and element indexes based on a generic pointer address
 * Parameter(s): [IN] pData - a pointer to a NVM RAM table
 *               [OUT] pIndex - a pointer to a memory location where the
 *                              requested indexed will be stored
 *               [OUT] pTableEntryIdx - a pointer to a memory location where the
 *                              requested TableEntry Idx will be stored
 * Return: gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PointerOutOfRange_c - if the provided pointer cannot be founded
 *                                    within the RAM table
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvGetTableEntryIndexFromDataPtr(void                 *pData,
                                                        NVM_TableEntryInfo_t *pIndex,
                                                        uint16_t             *pTableEntryIdx)
{
    NVM_Status_t status = gNVM_InvalidTableEntry_c;
    /* Get entry from the dataPtr */
    status = NvGetEntryFromDataPtr(pData, pIndex);

    if (gNVM_OK_c == status)
    {
        assert(gNvInvalidDataEntry_c != pIndex->entryId);
        if (pTableEntryIdx != NULL)
        {
            /* Get table entry index from id */
            *pTableEntryIdx = NvGetTableEntryIndexFromId(pIndex->entryId);
            assert(gNvInvalidTableEntryIndex_c != *pTableEntryIdx);
        }
    }
    return status;
}
/******************************************************************************
 * Name: NvMetaAndRecordAddressRegulate
 * Description: Performs to regulate
 * Parameter(s): [IN] pageFreeSpace - free space in active page
 *               [IN] recordSize - the size of record aligned to Flash write size
 *               [IN] metaInfoOffset - the address of meta info will write to
 *               [IN] newRecordOffset - the address of record info will write to
 * Return: the status of the operation
 *          gNVM_PageCopyPending_c : if page copy is required for lack of space
 *          gNVM_OK_c : if the operation can complete successfully
 *          gNVM_AddressOutOfRange_c: if metaInfoAddress is invalid
 *          any error status returned by NvGetMetaInfo if MIT found corrupted
 *
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvMetaAndRecordAddressRegulate(uint32_t  pageFreeSpace,
                                                       uint16_t  recordSize,
                                                       uint32_t *metaInfoOffset,
                                                       uint32_t *newRecordOffset)
{
    NVM_Status_t status = gNVM_OK_c;

    NVM_RecordMetaInfo_t metaInfo = {0U};
    uint32_t             lastRecordOffset;
    uint16_t             realRecordSize = 0U;

    /* compute the 'real record size' taking into consideration that the flash controller only writes in
     * phrases PGM_SIZE_BYTE bytes
     */
    /* CERT INT31-C / MISRA 10.3: guard recordSize fits in uint16_t before NvUpdateSize */

    do
    {
        uint32_t new_record_offset;
        uint16_t lastMetaOffset = mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset;

        if (recordSize != 0U)
        {
            realRecordSize = recordSize;
            realRecordSize = NvUpdateSize(realRecordSize);
            if (realRecordSize == 0xFFFFU)
            {
                status = gNVM_Error_c;
                assert(false);
                break;
            }
        }

        /* compute the total size (record + meta info size) */

        pageFreeSpace &= (uint32_t)UINT16_MAX;

        if (realRecordSize >= (uint16_t)pageFreeSpace)
        {
            status                    = gNVM_PageCopyPending_c;
            mNvCopyOperationIsPending = TRUE;
            break;
        }

        if (gNvInvalidMetaOffset_c == lastMetaOffset)
        {
            /* empty page, first write */
            /* set new record address */
            new_record_offset = mNvTotalPageSize - sizeof(NVM_TableInfo_t);
            new_record_offset -= realRecordSize;
            if (new_record_offset < (gNvFirstMetaOffset_c + sizeof(NVM_TableInfo_t)))
            {
                assert(false); /* Would be the sign of a data corruption */
                status = gNVM_AddressOutOfRange_c;
                break;
            }
            /* page was empty: initialise meta offset to the first valid meta slot */
            *newRecordOffset = new_record_offset;
            *metaInfoOffset  = gNvFirstMetaOffset_c;
            status           = gNVM_OK_c;
            break;
        }

        /* page was not empty : get the meta information of the last successfully written record */
        *metaInfoOffset = (uint32_t)lastMetaOffset;
        /* CERT INT30-C: guard subtraction does not wrap */
        {
            uint32_t minPageSz = (uint32_t)sizeof(NVM_RecordMetaInfo_t) + (uint32_t)sizeof(NVM_TableAndEntryInfo_t);
            assert(mNvTotalPageSize >= minPageSz);
            if (*metaInfoOffset >= ((mNvTotalPageSize >= minPageSz) ? (mNvTotalPageSize - minPageSz) : 0U))
            {
                assert(false); /* Would be the sign of a data corruption */
                status = gNVM_AddressOutOfRange_c;
                break;
            }
        }
#if gUnmirroredFeatureSet_d
        status = NvGetMetaInfo(mNvActivePageId, mNvVirtualPageProperty[mNvActivePageId].NvLastMetaUnerasedInfoOffset,
                               &metaInfo);
#else
        /* get the last record start address (the address is always aligned) */
        status = NvGetMetaInfo(mNvActivePageId, *metaInfoOffset, &metaInfo);
#endif
        if (status != gNVM_OK_c)
        {
            break;
        }
        /* If Meta Info that was read from flash is invalid, do not use it */
        lastRecordOffset = metaInfo.u.fields.NvmRecordOffset;
        /* set new record address */
        /* CERT INT30-C: guard subtraction does not wrap */
        assert(lastRecordOffset >= realRecordSize);
        if (lastRecordOffset < realRecordSize)
        {
            status = gNVM_AddressOutOfRange_c;
            break;
        }
        *newRecordOffset = lastRecordOffset - realRecordSize;

        *metaInfoOffset += sizeof(NVM_RecordMetaInfo_t);

    } while (false);

    if (status == gNVM_OK_c)
    {
        uint32_t totalRecordSize;
        /* Cannot overflow because realRecordSize is smaller than pageFreeSpace that itself is less than UINT16_MAX*/
        totalRecordSize = realRecordSize + sizeof(NVM_RecordMetaInfo_t);

        /* make sure there is at least a free space for a meta between the last one and the data*/
        while (true)
        {
            /* check if the record fits the page's free space.
             * one extra meta info space must be kept always free, to be able to perform the meta info search */
            if (totalRecordSize + sizeof(NVM_RecordMetaInfo_t) >= pageFreeSpace)
            {
                /* there is no space to save the record, try to copy the current active page latest records
                 * to the other page
                 */
                status = gNVM_PageCopyPending_c;
                break;
            }

            /* check if the space for the record is free */
            if ((FALSE == NV_PartitionBlankCheckAtOffset(mNvActivePageId, *newRecordOffset, realRecordSize) &&
                 (realRecordSize != 0U)))
            {
                /* the memory space is not blank */
                if (pageFreeSpace < realRecordSize)
                {
                    /* I am unable to write the record on this page, trigger copy page */
                    status = gNVM_PageCopyPending_c;
                    break;
                }
                pageFreeSpace -= realRecordSize;

                *newRecordOffset -= realRecordSize;
            }
            /* check if the space for the meta is free */
            else if (!NV_PartitionBlankCheckAtOffset(mNvActivePageId, *metaInfoOffset, sizeof(NVM_RecordMetaInfo_t)))
            {
                /* the memory space is not blank */
                if (pageFreeSpace < realRecordSize)
                {
                    /* I am unable to write the meta on this page, trigger copy page */
                    status = gNVM_PageCopyPending_c;
                    break;
                }
                pageFreeSpace -= sizeof(NVM_RecordMetaInfo_t);
                *metaInfoOffset += sizeof(NVM_RecordMetaInfo_t);
            }
            else
            {
                /* the memory space is blank */
                /* status is already gNVM_OK_c */
                break;
            }
        }
    }

    return status;
}

/******************************************************************************
 * Name: NvWriteRecordToFlash
 * Description: writes a record
 * Parameter(s): [IN] pg_id targetted virtual page
 *               [IN] tblIndexes - a pointer to table and element indexes
 *               [IN] tableEntryIdx - the table EntryIdx
 *               [IN] metaInfo - the meta infomation
 *               [IN] metaInfoOffset - the address of meta info will write to
 *               [IN] newRecordOffset - the address of record info will write to
  *              [IN] recordSize - the address of record will write to
 *               [IN] mirroredSrcAddress - the mirrored source address of will write to
 * Return: the status of the operation

 *****************************************************************************/
NVM_STATIC NVM_Status_t NvWriteRecordToFlash(NVM_VirtualPageID_t   pg_id,
                                             NVM_TableEntryInfo_t *tblIndexes,
                                             uint16_t              tableEntryIdx,
                                             NVM_RecordMetaInfo_t *p_metaInfo,
                                             uint32_t              metaInfoOffset,
                                             uint32_t              newRecordOffset,
                                             uint32_t              recordSize,
                                             uint32_t              mirroredSrcAddress)
{
    NVM_Status_t status = gNVM_OK_c;

    do
    {
        uint32_t srcAddress;
#if gUnmirroredFeatureSet_d
        if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[tableEntryIdx].DataEntryType)
        {
            srcAddress =
                (uint32_t)(uint8_t *)((uint8_t **)pNVM_DataTable[tableEntryIdx].pData)[tblIndexes->elementIndex];
        }
        else
#endif
        {
            srcAddress = mirroredSrcAddress;
        }

#if gUnmirroredFeatureSet_d
        if (0U == srcAddress)
        {
            /* It's an erased unmirrored dataset : actually cannot fail because 0 is 32 bit aligned and VSB/VEB were
             * previously validated */
            NvSetMetaInfo(p_metaInfo, p_metaInfo->u.fields.NvmDataEntryID, p_metaInfo->u.fields.NvmElementIndex, 0U,
                          p_metaInfo->u.fields.NvValidationStartByte);
            status = gNVM_OK_c;
        }
        else
        {
            status = NV_PartitionProgramUnalignedAtOffset(pg_id, newRecordOffset, recordSize, (uint8_t *)srcAddress);
        }
#else
        status = NV_PartitionProgramUnalignedAtOffset(pg_id, newRecordOffset, recordSize, (uint8_t *)srcAddress);
#endif
        if (gNVM_OK_c != status)
        {
            break;
        }
        /* record successfully written, now write the associated record meta information */
        status =
            NV_PartitionProgramAtOffset(pg_id, metaInfoOffset, (uint8_t *)p_metaInfo, sizeof(NVM_RecordMetaInfo_t));
        if (gNVM_OK_c != status)
        {
            if (gNVM_EccFault_c == status)
            {
                status = gNVM_EccFaultWritingMeta_c;
            }
            else
            {
                status = gNVM_MetaInfoWriteError_c;
            }
            break;
        }

        /* update the last record meta information */
        mNvVirtualPageProperty[pg_id].NvLastMetaInfoOffset = (uint16_t)metaInfoOffset;
        /* update the last unerased meta info address */
#if gUnmirroredFeatureSet_d
        if (0U != p_metaInfo->u.fields.NvmRecordOffset)
        {
            mNvVirtualPageProperty[pg_id].NvLastMetaUnerasedInfoOffset = (uint16_t)metaInfoOffset;
        }
#endif
        /* Empty macro when nvm monitoring is not enabled */
        FSCI_NV_WRITE_MONITOR(p_metaInfo->u.fields.NvmDataEntryID, tblIndexes->elementIndex,
                              (tblIndexes->op_type == OP_SAVE_ALL) ? TRUE : FALSE);
#if gUnmirroredFeatureSet_d
        if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[tableEntryIdx].DataEntryType)
        {
            if (0U != p_metaInfo->u.fields.NvmRecordOffset)
            {
                uint32_t flash_addr = 0U;
                if (NvAddOffsetToAddr(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress, newRecordOffset,
                                      &flash_addr) != 0)
                {
                    status = gNVM_Error_c;
                    break;
                }
                else
                {
                    uint8_t *pTempAddress =
                        (uint8_t *)((uint8_t **)pNVM_DataTable[tableEntryIdx].pData)[tblIndexes->elementIndex];
                    ((uint8_t **)pNVM_DataTable[tableEntryIdx].pData)[tblIndexes->elementIndex] = (uint8_t *)flash_addr;
                    (void)MEM_BufferFree(pTempAddress);
                }
            }
        }
#endif
        status = gNVM_OK_c;
    } while (false);

    return status;
}

/******************************************************************************
 * Name: NvWriteRecord
 * Description: writes a record
 * Parameter(s): [IN] tblIndexes - a pointer to table and element indexes
 * Return: gNVM_InvalidPageID_c - if the active page is not valid
 *         gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_MetaInfoWriteError_c - if the meta information couldn't be
 *                                     written
 *         gNVM_RecordWriteError_c - if the record couldn't be written
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvWriteRecord(NVM_TableEntryInfo_t *tblIndexes)
{
    uint32_t             metaInfoOffset;
    NVM_RecordMetaInfo_t metaInfo = {0U};
    uint32_t             pageFreeSpace;
    uint32_t             mirroredSrcAddress;
    uint8_t              nvValidationByte;

    NVM_Status_t status = gNVM_OK_c;
    uint16_t     tableEntryIdx;
    uint16_t     recordSize;

    do
    {
        uint32_t newRecordOffset = 0U;

        tableEntryIdx = NvGetTableEntryIndexFromId(tblIndexes->entryId);

        if (gNvInvalidTableEntryIndex_c == tableEntryIdx)
        {
            status = gNVM_InvalidTableEntry_c;
            break;
        }
        /* make sure i don't process the save if page copy is active */
        if (mNvCopyOperationIsPending)
        {
            status = gNVM_PageCopyPending_c;
            break;
        }
#if gUnmirroredFeatureSet_d
        /* For data sets not mirrored in ram a table entry is saved separate */
        if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[tableEntryIdx].DataEntryType)
        {
            tblIndexes->op_type = OP_SAVE_SINGLE;
        }
#endif
        if (tblIndexes->op_type == OP_SAVE_ALL)
        {
            uint32_t max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
            if (NvMultEltSzByNb(pNVM_DataTable[tableEntryIdx].ElementsCount, pNVM_DataTable[tableEntryIdx].ElementSize,
                                max_val, &recordSize) < 0)
            {
                status = gNVM_Error_c;
                break;
            }
            nvValidationByte   = gValidationByteAllRecords_c;
            mirroredSrcAddress = (uint32_t)((uint8_t *)(((uint8_t *)(pNVM_DataTable[tableEntryIdx]).pData)));
        }
        else
        {
            recordSize         = pNVM_DataTable[tableEntryIdx].ElementSize;
            nvValidationByte   = gValidationByteSingleRecord_c;
            mirroredSrcAddress = (uint32_t)((uint8_t *)(((uint8_t *)(pNVM_DataTable[tableEntryIdx]).pData)) +
                                            (tblIndexes->elementIndex * pNVM_DataTable[tableEntryIdx].ElementSize));
        }

#if gUnmirroredFeatureSet_d
        /* Check if is an erase for unmirrored dataset*/
        if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[tableEntryIdx].DataEntryType)
        {
            if (NULL == ((void **)pNVM_DataTable[tableEntryIdx].pData)[tblIndexes->elementIndex])
            {
                recordSize = 0U;
            }
            /*if the dataset is already in flash, ignore it*/
            else if (NvIsNVMFlashAddress(((void **)pNVM_DataTable[tableEntryIdx].pData)[tblIndexes->elementIndex]))
            {
                /*it returns OK, because atomic save must not fail, this is not an error*/
                status = gNVM_OK_c;
                break;
            }
            else
            {
                /* Empty statement : MISRA C 2012 Rule 15.7 */
            }
        }
#endif
        /* get active page free space : if NvGetPageFreeSpace returns an error pageFreeSpace is 0 anyway */
        (void)NvGetPageFreeSpace(&pageFreeSpace, FALSE);

        /* check if the space needed by the record is really free (erased).
         * this check is necessary because it may happens that a record to be successfully written,
         * but the system fails (e.g. POR) before the associated meta information has been written.
         * the theoretically free space is computed as the difference between the last meta info
         * address and the start address of the last successfully written record. This information
         * is valuable but may not reflect the reality, as mentioned in the explanation above */

        status = NvMetaAndRecordAddressRegulate(pageFreeSpace, recordSize, &metaInfoOffset, &newRecordOffset);

        /* Write the record and associated meta information */
        if (status == gNVM_PageCopyPending_c)
        {
            /* there is no space to save the record, try to copy the current active page latest records
             * to the other page
             */
            mNvCopyOperationIsPending = TRUE;
            break;
        }
        if (status != gNVM_OK_c)
        {
            /* an error here would be fatal */
            break;
        }
        /* set associated meta info : no use checking return status because :
         *    - nvValidationByte is set within this function to a correct value
         *    - newRecordAddress is necessarily set to a consistent value if we reached here,
         *      since NvMetaAndRecordAddressRegulate returned TRUE
         */
        NvSetMetaInfo(&metaInfo, pNVM_DataTable[tableEntryIdx].DataEntryID, tblIndexes->elementIndex,
                      (uint16_t)newRecordOffset, nvValidationByte);

        /* the offset has to be 4 bytes aligned, an extra check is performed to avoid further hard
         * faults caused by FTFx controller */
        status = NvWriteRecordToFlash(mNvActivePageId, tblIndexes, tableEntryIdx, &metaInfo, metaInfoOffset,
                                      newRecordOffset, recordSize, mirroredSrcAddress);
        if (gNVM_EccFaultWritingMeta_c == status || gNVM_EccFaultWritingRecord_c == status)
        {
            mNvCopyOperationIsPending                              = TRUE;
            mNvVirtualPageProperty[mNvActivePageId].has_ecc_faults = TRUE;
            status                                                 = gNVM_PageCopyPending_c;
        }
    } while (FALSE);
    return status;
}

/******************************************************************************
 * Name: NvRestoreData
 * Description: restore an element from NVM storage to its original RAM location
 * Parameter(s): [IN] tblIdx - pointer to table and element indexes
 * Return: gNVM_NullPointer_c - if the provided pointer is NULL
 *         gNVM_PageIsEmpty_c - if page is empty
 *         gNVM_Error_c - in case of error(s)
 *         gNVM_OK_c - if the operation completed successfully
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvRestoreData(NVM_TableEntryInfo_t *tblIdx)
{
    NVM_Status_t status = gNVM_MetaNotFound_c;
    do
    {
        NVM_RecordMetaInfo_t metaInfo = {0U};
        uint32_t             metaInfoOffset;
        uint16_t             tableEntryIdx;

        tableEntryIdx = NvGetTableEntryIndexFromId(tblIdx->entryId);

        if (gNvInvalidTableEntryIndex_c == tableEntryIdx)
        {
            status = gNVM_InvalidTableEntry_c;
            break;
        }

        /* get the last meta information address */
        metaInfoOffset = (uint32_t)mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset;
        if (metaInfoOffset == gNvInvalidMetaOffset_c)
        {
            /* blank page, no data to restore */
            status = gNVM_PageIsEmpty_c;
            break;
        }
        if (tblIdx->entryId == gNvInvalidDataEntry_c)
        {
            /* invalid table entry */
            status = gNVM_InvalidTableEntry_c;
            break;
        }
        /*
         * If the meta info is found, the associated record is restored,
         * otherwise the gNVM_MetaNotFound_c will be returned
         */
        status = gNVM_MetaNotFound_c;
#if gNvFragmentation_Enabled_d
        /* clear the buffer */
        FLib_MemSet(maNvRecordsCpyOffsets, 0U,
                    (uint32_t)sizeof(uint16_t) * pNVM_DataTable[tableEntryIdx].ElementsCount);
#endif
        /* parse meta info backwards */
        for (; metaInfoOffset >= gNvFirstMetaOffset_c; metaInfoOffset -= sizeof(NVM_RecordMetaInfo_t))
        {
            NVM_Status_t st;
            uint16_t     ram_offset;
            uint32_t     max_val = MIN(mNvTotalPageSize, (uint32_t)UINT16_MAX);
            uint16_t     rec_offset;
            uintptr_t    tb_data_addr;
            uintptr_t    wr_addr;
            /* get the meta information */
            st = NvGetMetaInfo(mNvActivePageId, metaInfoOffset, &metaInfo);
            if (st != gNVM_OK_c)
            {
                /* invalid meta info, move to the previous meta info */
                continue;
            }
            tb_data_addr = (uintptr_t)pNVM_DataTable[tableEntryIdx].pData;

            if (metaInfo.u.fields.NvmDataEntryID != tblIdx->entryId)
            {
                continue;
            }
            rec_offset = metaInfo.u.fields.NvmRecordOffset;

            if (tblIdx->op_type == OP_SAVE_ALL)
            {
                uint16_t elt_sz = pNVM_DataTable[tableEntryIdx].ElementSize;
#if gNvFragmentation_Enabled_d
                uint16_t elt_index = metaInfo.u.fields.NvmElementIndex;
                /* single save found */
                if ((metaInfo.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c) &&
                    (0U == maNvRecordsCpyOffsets[elt_index]))
                {
                    /* Coverity: Speculative execution data leak
                     * Insert a barrier between the comparison and the memory accesses to prevent speculative execution */
                    __DSB();
                    maNvRecordsCpyOffsets[elt_index] = rec_offset;

                    /* Prevent overflow when computing RAM offset */
                    if (NvMultEltSzByNb(elt_index, elt_sz, max_val, &ram_offset) < 0)
                    {
                        status = gNVM_AddressOutOfRange_c;
                        break;
                    }

                    wr_addr = tb_data_addr + (uintptr_t)ram_offset;
                    if (wr_addr < tb_data_addr) /* wraparound check */
                    {
                        status = gNVM_AddressOutOfRange_c;
                        break;
                    }
                    status = NV_PartitionReadAtOffset(mNvActivePageId, rec_offset, (uint8_t *)wr_addr, elt_sz);
                }
                /* full save found */
                else if (metaInfo.u.fields.NvValidationStartByte == gValidationByteAllRecords_c)
                {
                    for (uint16_t cnt = 0U; cnt < pNVM_DataTable[tableEntryIdx].ElementsCount; cnt++)
                    {
                        uint16_t src_offset;

                        /* skip already restored elements */
                        if (0U != maNvRecordsCpyOffsets[cnt])
                        {
                            continue;
                        }

                        /* Prevent overflow when computing source offset */
                        if (NvComputeEltOffset(cnt, elt_sz, 0U, rec_offset, &src_offset) < 0)
                        {
                            status = gNVM_AddressOutOfRange_c;
                            break;
                        }

                        /* Prevent overflow when computing RAM offset */
                        if (NvMultEltSzByNb(cnt, elt_sz, max_val, &ram_offset) < 0)
                        {
                            status = gNVM_AddressOutOfRange_c;
                            break;
                        }

                        wr_addr = tb_data_addr + (uintptr_t)ram_offset;

                        if (wr_addr < tb_data_addr) /* wraparound check */
                        {
                            status = gNVM_AddressOutOfRange_c;
                            break;
                        }

                        status = NV_PartitionReadAtOffset(mNvActivePageId, src_offset, (uint8_t *)wr_addr, elt_sz);
                    } /* for */
                    break;
                }
                else
                {
                    /*MISRA rule 15.7*/
                }
#else /* gNvFragmentation_Enabled_d */
                /* single saves are not allowed if fragmentation is off */
                if (metaInfo.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c)
                {
                    status = gNVM_FragmentedEntry_c;
                    break;
                }

                if (NvMultEltSzByNb(pNVM_DataTable[tableEntryIdx].ElementsCount,
                                    pNVM_DataTable[tableEntryIdx].ElementSize, max_val, &ram_offset) < 0)
                {
                    status = gNVM_AddressOutOfRange_c;
                    break;
                }
                wr_addr = tb_data_addr + (uintptr_t)ram_offset;
                if (wr_addr < tb_data_addr) /* wraparound check */
                {
                    status = gNVM_AddressOutOfRange_c;
                    break;
                }
                status = NV_PartitionReadAtOffset(mNvActivePageId, rec_offset, (uint8_t *)wr_addr, elt_sz);
                break;
#endif
            }
            else
            {
                if (metaInfo.u.fields.NvValidationStartByte == gValidationByteSingleRecord_c &&
                    metaInfo.u.fields.NvmElementIndex == tblIdx->elementIndex)
                {
#if gUnmirroredFeatureSet_d
                    if ((uint16_t)gNVM_MirroredInRam_c != pNVM_DataTable[tableEntryIdx].DataEntryType)
                    {
                        uint32_t flash_addr;
                        if (0U == metaInfo.u.fields.NvmRecordOffset)
                        {
                            ((uint8_t **)pNVM_DataTable[tableEntryIdx].pData)[tblIdx->elementIndex] = NULL;
                        }
                        else
                        {
                            /* Prevent overflow when computing flash address */
                            flash_addr = mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress;
                            if (metaInfo.u.fields.NvmRecordOffset > ((uint32_t)UINT32_MAX - flash_addr))
                            {
                                status = gNVM_AddressOutOfRange_c;
                                break;
                            }
                            flash_addr += metaInfo.u.fields.NvmRecordOffset;

                            ((uint8_t **)pNVM_DataTable[tableEntryIdx].pData)[tblIdx->elementIndex] =
                                (uint8_t *)flash_addr;
                        }
                        status = gNVM_OK_c;
                        break;
                    }
                    else
#endif
                    {
                        /* Prevent overflow when computing RAM offset */
                        if (NvMultEltSzByNb(metaInfo.u.fields.NvmElementIndex,
                                            pNVM_DataTable[tableEntryIdx].ElementSize, max_val, &ram_offset) < 0)
                        {
                            status = gNVM_AddressOutOfRange_c;
                            break;
                        }

                        wr_addr = tb_data_addr + (uintptr_t)ram_offset;
                        if (wr_addr < tb_data_addr) /* wraparound check */
                        {
                            status = gNVM_AddressOutOfRange_c;
                            break;
                        }

                        /* restore the element */
                        status = NV_PartitionReadAtOffset(mNvActivePageId, rec_offset, (uint8_t *)wr_addr,
                                                          pNVM_DataTable[tableEntryIdx].ElementSize);

                        break;
                    } /* else */
                }     /* gValidationByteSingleRecord_c and same index */

                if (metaInfo.u.fields.NvValidationStartByte == gValidationByteAllRecords_c)
                {
                    uint16_t rd_offset = 0U;
                    /* restore the single element from the entire table entry record */
                    if (NvComputeEltOffset(tblIdx->elementIndex, pNVM_DataTable[tableEntryIdx].ElementSize, 0U,
                                           metaInfo.u.fields.NvmRecordOffset, &rd_offset) < 0)
                    {
                        status = gNVM_AddressOutOfRange_c;
                        break;
                    }

                    /* Prevent overflow when computing RAM offset */
                    if (NvMultEltSzByNb(tblIdx->elementIndex, pNVM_DataTable[tableEntryIdx].ElementSize, max_val,
                                        &ram_offset) < 0)
                    {
                        status = gNVM_AddressOutOfRange_c;
                        break;
                    }
                    wr_addr = tb_data_addr + (uintptr_t)ram_offset;

                    if (wr_addr < tb_data_addr) /* wraparound check */
                    {
                        status = gNVM_AddressOutOfRange_c;
                        break;
                    }
                    /* restore the element */
                    status = NV_PartitionReadAtOffset(mNvActivePageId, rd_offset, (uint8_t *)wr_addr,
                                                      pNVM_DataTable[tableEntryIdx].ElementSize);
                    break;
                }
            }       /* else */
        } /* for */ /* move to the previous meta info */

    } while (false);

    return status;
}

/******************************************************************************
 * Name: NvGetTableEntryIndex
 * Description: get the table entry index from the provided ID
 * Parameter(s): [IN] entryId - the ID of the table entry
 * Return: table entry index of gNvInvalidTableEntryIndex_c
 *****************************************************************************/
NVM_STATIC uint16_t NvGetTableEntryIndexFromId(NvTableEntryId_t entryId)
{
    uint16_t loopCnt = 0U;

    while (loopCnt < mNVM_DataTableNbEntries)
    {
        if (pNVM_DataTable[loopCnt].DataEntryID == entryId)
        {
            break;
        }
        /* increment the loop counter */
        loopCnt++;
    }
    if (mNVM_DataTableNbEntries == loopCnt)
    {
        loopCnt = gNvInvalidTableEntryIndex_c;
    }
    return loopCnt;
}

/******************************************************************************
 * Name: NvProcessFirstSaveInQueue
 * Description: processes the first save in the queue so that the queue can accept another entry
 * Parameter(s): -
 * Return: TRUE if a save has been processed, ELSE otherwise
 *****************************************************************************/
NVM_STATIC NVM_Status_t NvProcessFirstSaveInQueue(NVM_TableEntryInfo_t *ptrTblIdx)
{
    NVM_TableEntryInfo_t tblIdx;
    NVM_Status_t         status = gNVM_OK_c;

    if (0U == mNvCriticalSectionFlag)
    {
        if (NvIsPendingOperation())
        {
            /* Dequeue pending operations */
            while (NvPopPendingSave(&tblIdx))
            {
                /* save tblIdx */
                *ptrTblIdx = tblIdx;
                if ((gNvCopyAll_c == tblIdx.entryId) && (gNvCopyAll_c == tblIdx.elementIndex) &&
                    (OP_SAVE_ALL == tblIdx.op_type))
                {
                    status = gNVM_AtomicSaveRecursive_c;
                    break;
                }
                else if (gNvInvalidDataEntry_c == tblIdx.entryId)
                {
                    /* Skip invalid entries */
                    continue;
                }
                else
                {
                    /*MISRA rule 15.7*/
                }

                if (NvWriteRecord(&tblIdx) == gNVM_PageCopyPending_c)
                {
                    /* A switch to the other page is required to perform garbage collection */
                    status = NvModuleSwitchPage(gNvCopyAll_c);
                    if (gNVM_OK_c == status)
                    {
                        /* Garbage collection has completed */
                        mNvCopyOperationIsPending = FALSE;
                    }
                    /* Retry NvWriteRecord operation */
                    if (gNVM_OK_c == NvWriteRecord(&tblIdx))
                    {
                        status = gNVM_OK_c;
                        break;
                    }
                    else
                    {
                        /* return gNVM_SaveRequestRecursive_c to run again */
                        status = gNVM_SaveRequestRecursive_c;
                        break;
                    }
                }
                else
                {
                    status = gNVM_OK_c;
                    break;
                }
            }
        }
    }
    return status;
}

/******************************************************************************
 * Name: NvAddSaveRequestToQueue
 * Description: Add save request to save requests queue; if the request is
 *              already stored, ignore the current request
 * Parameter(s): [IN] ptrTblIdx - pointer to table index
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_SaveRequestRejected_c - if the request couldn't be queued
 ******************************************************************************/
NVM_STATIC NVM_Status_t NvAddSaveRequestToQueue(NVM_TableEntryInfo_t *ptrTblIdx)
{
    uint8_t              loopIdx;
    bool_t               isQueued       = FALSE;
    bool_t               isInvalidEntry = FALSE;
    uint8_t              lastInvalidIdx = 0U;
    uint8_t              remaining_count;
    NVM_Status_t         status   = gNVM_OK_c;
    NVM_TableEntryInfo_t nvTblIdx = *ptrTblIdx;
    NVM_TableEntryInfo_t preNvTblIdx;

    do
    {
        if (mNvPendingSavesQueue.EntriesCount == 0U)
        {
            /* add request to queue */
            if (FALSE == NvPushPendingSave(nvTblIdx))
            {
                status = gNVM_SaveRequestRejected_c;
            }
        }
        else
        {
            /* start from the queue's head */
            loopIdx = (uint8_t)mNvPendingSavesQueue.Head;

            remaining_count = (uint8_t)mNvPendingSavesQueue.EntriesCount;
            /* check if the request is not already stored in queue */
            while (remaining_count != 0U)
            {
                if (nvTblIdx.entryId == mNvPendingSavesQueue.QData[loopIdx].entryId)
                {
                    if (mNvPendingSavesQueue.QData[loopIdx].op_type ==
                        OP_SAVE_ALL) /* full table entry already queued */
                    {
                        /* request is already queued */
                        isQueued = TRUE;
                        break;
                    }

                    /* single element from table entry is queued */
                    if (nvTblIdx.op_type == OP_SAVE_ALL) /* a full table entry is requested to be saved */
                    {
                        /* update only the flag of the already queued request */
                        mNvPendingSavesQueue.QData[loopIdx].op_type = OP_SAVE_ALL;
                        /* request is already queued */
                        isQueued = TRUE;
                        break;
                    }

                    /* The request is for a single element and the queued request is also for a single element;
                     * Check if the request is for the same element. If the request is for a different element,
                     * add the new request to queue.
                     */
                    if (nvTblIdx.elementIndex == mNvPendingSavesQueue.QData[loopIdx].elementIndex)
                    {
                        /* request is already queued */
                        isQueued = TRUE;
                        break;
                    }
                }
                /* Check if in the queue is an invalid entryId that can be used*/
                if ((gNvInvalidDataEntry_c == mNvPendingSavesQueue.QData[loopIdx].entryId) && (isInvalidEntry == FALSE))
                {
                    isInvalidEntry = TRUE;
                    lastInvalidIdx = loopIdx;
                }
                remaining_count--;
                /* increment and wrap the loop index */
                if (++loopIdx >= (uint8_t)gNvPendingSavesQueueSize_c)
                {
                    loopIdx = 0U;
                }
            }

            if (!isQueued)
            {
                /* Reuse an invalid entry from the queue*/
                if (TRUE == isInvalidEntry)
                {
                    mNvPendingSavesQueue.QData[lastInvalidIdx] = nvTblIdx;
                }
                else
                {
                    /* push the request to save operation pending queue */
                    if (!NvPushPendingSave(nvTblIdx))
                    {
                        preNvTblIdx = nvTblIdx;
                        /* free a space */
                        status = NvProcessFirstSaveInQueue(&nvTblIdx);
                        if (!NvPushPendingSave(preNvTblIdx))
                        {
                            status = gNVM_SaveRequestRejected_c;
                        }
                    }
                }
            }
        }
    } while (status == gNVM_SaveRequestRecursive_c);
    return status;
}

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerJitter_c)
/******************************************************************************
 * Name: GetRandomRange
 * Description: Returns a random number between 'low' and 'high'
 * Parameter(s): [IN] low, high - generated number range
 * Return: 0..255
 ******************************************************************************/
NVM_STATIC uint8_t GetRandomRange(uint8_t low, uint8_t high)
{
    uint32_t random;
    uint8_t  random_in_range;

    GET_RND_NB(&random);

    if (high <= low)
    {
        random_in_range = low;
    }
    else
    {
        random_in_range = (uint8_t)(low + (random % ((uint32_t)high - low + 1U)));
    }
    return random_in_range;
}
#endif /* gNvmUseSaveOnIntervalWithTimerJitter_c */

#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
static void Nv_ReportEccFault(uint32_t fault_address, int rNw)
{
    /* If a callback was registered prior to report address and direction of flash operation read or write */
    if (nv_fault_report_cb != NULL)
    {
        (*nv_fault_report_cb)(fault_address, rNw);
    }
    else
    {
        NOT_USED(fault_address);
        NOT_USED(rNw);
    }
}
#endif

/******************************************************************************
 * Name: NvCompletePendingOperationsUnsafe
 * Description: The function attemps to complete all the NVM related pending
 *              operations.
 * Parameter(s):  -
 * Return: -
 *****************************************************************************/
NVM_STATIC void NvCompletePendingOperationsUnsafe(void)
{
    uint16_t idx = 0U;

    while (idx < mNVM_DataTableNbEntries)
    {
        /* For all entries in the dataset mark operation as imminent  */
        if ((maDatasetInfo[idx].saveNextInterval) && (maDatasetInfo[idx].ticksToNextSave != 0U))
        {
            maDatasetInfo[idx].ticksToNextSave = 0U;
#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
            mNvSaveOnIntervalEvent = TRUE;
#endif
        }
        idx++;
    }
    /* Force operations that would normally be differed to be executed now calling NvIdle multiple
     * time until all entries get consumed */
    do
    {
        (void)__NvIdle();
    } while ((mNvErasePgCmdStatus.NvErasePending == TRUE) || (mNvCopyOperationIsPending == TRUE) ||
             (mNvPendingSavesQueue.EntriesCount != 0U));
}

/******************************************************************************
 * Name: __NvShutdown
 * Description: The function waits for all idle saves to be processed.
 * Parameter(s):  -
 * Return: -
 *****************************************************************************/
NVM_STATIC void __NvShutdown(void)
{
    /* wait for all operations to complete : calling unsafe API is good enough because
     * mutex already taken from NvShutdown */
    NvCompletePendingOperationsUnsafe();

    assert(NvGetPendingSavesCount() == 0U);
    assert(mNvCopyOperationIsPending == FALSE);
    for (uint16_t idx = 0; idx < mNVM_DataTableNbEntries; idx++)
    {
        /* for each dataset saveNextInterval must have been treated by now */
        assert(maDatasetInfo[idx].saveNextInterval == FALSE);
    }
}

/*!
 * Name:   NvAddOffsetToAddr
 *  Safely computes the sum of a base address and a byte offset.
 *
 * Adds offset to base_addr and writes the result to addr only when
 * the addition does not overflow a uint32_t.  The check uses the strict
 * inequality  base_addr < UINT32_MAX - offset  so the boundary value
 * UINT32_MAX itself is treated as an overflow and is rejected.
 *
 * Parameter(s): [in]  base_addr  Starting address to which the offset is added.
 *               [in]  offset     Byte offset to add to base_addr - limited to UINT16_MAX.
 *               [out] addr       Receives the computed address on success.  The
 *                        pointed-to value is not modified on overflow.
 *
 * Return:  0   Success: addr has been set to base_addr + offset.
 *         -1   Overflow detected: the result would exceed UINT32_MAX and
 *              addr was not written.
 */
/* HIS_CALLING: NvAddOffsetToAddr is a safety helper called by many functions by design. */
NVM_STATIC int NvAddOffsetToAddr(uint32_t base_addr, uint32_t offset, uint32_t *addr)
{
    int      ret = -1;
    uint32_t val = base_addr;

    /* CERT INT30-C: single overflow-safe check; no outer UINT16_MAX guard needed */
    if (val <= ((uint32_t)UINT32_MAX - offset))
    {
        val += offset;
        *addr = val;
        ret   = 0;
    }
    return ret;
}

/******************************************************************************
 * Name: NV_FlashRead
 * Description: Read from flash address.
 * Parameter(s):  [IN]  flash_addr address to read from
 *                [OUT] ram_buf pointer on buffer to receive read bytes from flash
 *                [IN]  size to be read
 *                [IN]  check_ecc_fault tells whether EEC verification must be done
 *                      if virtual page contains faults
 * Return: -
 *****************************************************************************/
NVM_STATIC NVM_Status_t NV_FlashRead(uint32_t flash_addr, uint8_t *ram_buf, size_t size, bool_t check_ecc_fault)
{
    NVM_Status_t st = gNVM_OK_c;
    NOT_USED(check_ecc_fault);
#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
    if (check_ecc_fault == TRUE)
    {
        if (HAL_FlashReadCheckEccFaults(flash_addr, size, ram_buf) == kStatus_HAL_Flash_EccError)
        {
            Nv_ReportEccFault(flash_addr, 1);
            st = gNVM_EccFault_c;
        }
    }
    else
#endif
    {
        /* Ignore HAL_FlashRead return value since always kStatus_HAL_Flash_Success*/
        (void)HAL_FlashRead(flash_addr, size, ram_buf);
    }
    return st;
}

/******************************************************************************
 * Name: NV_PartitionReadAtOffset
 * Description: Read from NV partition.
 * Parameter(s):  [IN]  page ID of the virtual page to read from
 *                [IN]  pg_offset offset in page to be read
 *                [OUT] ram_buf pointer on buffer to receive read bytes from flash
 *                [IN]  size to be read
 * Return: gNVM_OK_c if all good, gNVM_Error_c if error in conversion from offset to address,
 *         gNVM_EccFault_c n case of ECC fault
 *****************************************************************************/
NVM_STATIC NVM_Status_t NV_PartitionReadAtOffset(NVM_VirtualPageID_t pg_id,
                                                 uint32_t            pg_offset,
                                                 uint8_t            *ram_buf,
                                                 size_t              size)
{
    NVM_Status_t st         = gNVM_OK_c;
    uint32_t     flash_addr = 0U;

    /* Safe addition of base address with offset */
    if (NvAddOffsetToAddr(mNvVirtualPageProperty[pg_id].NvRawSectorStartAddress, pg_offset, &flash_addr) != 0)
    {
        st = gNVM_Error_c;
    }
    else
    {
        /* Read from flash */
        st = NV_FlashRead(flash_addr, ram_buf, size, mNvVirtualPageProperty[pg_id].has_ecc_faults);
    }
    return st;
}

/******************************************************************************
 * Name: NV_PartitionBlankCheckAtOffset
 * Description: The function performs a blank check from an offset in NV virtual page.
 * Parameter(s):  [IN]  page ID of the virtual page to be verified
 *                [IN]  offset offset in page to be blank checked
 *                [IN]  len  to be checked must be multiple of phrase size.
 * Return: TRUE is area is not 0 length and the range entirely belongs to the virtual page and is blank,
 *          FALSE otherwise.
 *
 *****************************************************************************/
NVM_STATIC bool_t NV_PartitionBlankCheckAtOffset(NVM_VirtualPageID_t pg_id, uint32_t offset, uint32_t len)
{
    bool_t status = FALSE;

    do
    {
        uint32_t address = 0U;

        if (len == 0U || (len % (uint8_t)PGM_SIZE_BYTE != 0U))
        {
            break;
        }
        /* mNvTotalPageSize is always <= 0x10000, so offset and len fit in uint32_t without overflow */
        if ((offset >= mNvTotalPageSize) || (len > mNvTotalPageSize))
        {
            break;
        }
        if ((offset + len) > mNvTotalPageSize)
        {
            break;
        }
        /* Safe addition of base address with offset */
        if (NvAddOffsetToAddr(mNvVirtualPageProperty[pg_id].NvRawSectorStartAddress, offset, &address) != 0)
        {
            break;
        }
        if (kStatus_HAL_Flash_Success == HAL_FlashVerifyErase(address, len, kHAL_Flash_MarginValueNormal))
        {
            /* blank check */
            status = TRUE;
        }
    } while (false);
    return status;
}

#if defined             gNvVerifyReadBackAfterProgram_d && (gNvVerifyReadBackAfterProgram_d > 0)
NVM_STATIC NVM_Status_t NV_VerifyProgram(uint32_t flash_addr, uint8_t *ram_buf, size_t size, bool_t catch_ecc_err)
{
    NVM_Status_t st                    = gNVM_OK_c;
    uint32_t     remaining_sz          = size;
    uint32_t     offset                = 0U;
    uint8_t      phrase[PGM_SIZE_BYTE] = {0U};

    NOT_USED(catch_ecc_err);

    while (remaining_sz > 0U)
    {
        uint32_t addr;
        size_t   read_sz;

        read_sz = MIN(remaining_sz, sizeof(phrase));

        addr = flash_addr + offset;
#if defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0)
        if (TRUE == catch_ecc_err)
        {
            if (HAL_FlashReadCheckEccFaults(addr, read_sz, &phrase[0]) != kStatus_HAL_Flash_Success)
            {
                /* HAL_FlashRead mays return kStatus_HAL_Flash_EccError */
                /* It means that the ECC Fault would have fired need to proceed to erase of active page to salvage  */
                st = gNVM_EccFault_c;
                Nv_ReportEccFault(addr, 0);
                break;
            }
        }
        else
#endif
        {
            /* coverity [overflow_sink:FALSE] */ /* read_sz cannot underflow */
            if (HAL_FlashRead(addr, read_sz, &phrase[0]) != kStatus_HAL_Flash_Success)
            {
                /* HAL_FlashRead always returns kStatus_HAL_Flash_Success, so not really attainable */
                /* If the ECC Fault fires we reset directly. On next reset the NVM recovery takes place */
                st = gNVM_RecordWriteError_c;
                break;
            }
        }

        if (FLib_MemCmp(&phrase[0], &ram_buf[offset], read_sz) != TRUE)
        {
            st = gNVM_RecordWriteError_c;
            break;
        }
        offset += read_sz;
        remaining_sz -= read_sz;
    }
    return st;
}
#endif

NVM_STATIC NVM_Status_t NV_FlashProgram(uint32_t flash_addr, size_t size, uint8_t *ram_buf, bool_t catch_ecc_faults)
{
    NVM_Status_t       status = gNVM_OK_c;
    hal_flash_status_t st;

    NOT_USED(catch_ecc_faults);
    st = HAL_FlashProgram(flash_addr, size, ram_buf);
    if (kStatus_HAL_Flash_Success == st)
    {
#if defined gNvVerifyReadBackAfterProgram_d && (gNvVerifyReadBackAfterProgram_d > 0)
        /* Read back contents right away : this may cause an ECC Fault but better know it at once. */
        status = NV_VerifyProgram(flash_addr, ram_buf, size, catch_ecc_faults);
        if (gNVM_EccFault_c == status)
        {
            status = gNVM_EccFaultWritingMeta_c;
        }
#endif
    }
    else
    {
        status = gNVM_MetaInfoWriteError_c;
    }
    return status;
}

NVM_STATIC NVM_Status_t NV_FlashProgramUnaligned(uint32_t flash_addr,
                                                 size_t   size,
                                                 uint8_t *ram_buf,
                                                 bool_t   catch_ecc_faults)
{
    NVM_Status_t st = gNVM_OK_c;
    NOT_USED(catch_ecc_faults);

    if (HAL_FlashProgramUnaligned(flash_addr, size, ram_buf) == kStatus_HAL_Flash_Success)
    {
#if defined gNvVerifyReadBackAfterProgram_d && (gNvVerifyReadBackAfterProgram_d > 0)
        /* Read back contents right away : this may cause an ECC Fault but better know it at once. */
        st = NV_VerifyProgram(flash_addr, ram_buf, size, catch_ecc_faults);
        if (gNVM_EccFault_c == st)
        {
            st = gNVM_EccFaultWritingRecord_c;
        }
#endif
    }
    else
    {
        st = gNVM_RecordWriteError_c;
    }
    return st;
}

NVM_STATIC NVM_Status_t NV_PartitionProgramUnalignedAtOffset(NVM_VirtualPageID_t pg_id,
                                                             uint32_t            pg_offset,
                                                             size_t              size,
                                                             uint8_t            *ram_buf)
{
    NVM_Status_t st = gNVM_OK_c;

    uint32_t flash_addr = 0U;
    /* Safe addition of base address with offset */
    if (NvAddOffsetToAddr(mNvVirtualPageProperty[pg_id].NvRawSectorStartAddress, pg_offset, &flash_addr) != 0)
    {
        st = gNVM_Error_c;
    }
    else
    {
        st = NV_FlashProgramUnaligned(flash_addr, size, ram_buf, mNvVirtualPageProperty[pg_id].has_ecc_faults);
    }
    return st;
}

NVM_STATIC NVM_Status_t NV_PartitionProgramAtOffset(NVM_VirtualPageID_t pg_id,
                                                    uint32_t            pg_offset,
                                                    uint8_t            *ram_buf,
                                                    size_t              size)
{
    NVM_Status_t st = gNVM_OK_c;

    uint32_t flash_addr = 0U;
    /* Safe addition of base address with offset */
    if (NvAddOffsetToAddr(mNvVirtualPageProperty[pg_id].NvRawSectorStartAddress, pg_offset, &flash_addr) != 0)
    {
        st = gNVM_Error_c;
    }
    else
    {
        st = NV_FlashProgram(flash_addr, size, ram_buf, mNvVirtualPageProperty[pg_id].has_ecc_faults);
    }
    return st;
}

#endif /* gNvStorageIncluded_d */

/*****************************************************************************
 *****************************************************************************
 * Public functions
 *****************************************************************************
 *****************************************************************************/
/******************************************************************************
 * Name: GetFlashTableVersion
 * Description: returns the flash table version
 * Parameter(s): -
 * Return: 0 or flash table version
 *****************************************************************************/
uint16_t GetFlashTableVersion(void)
{
    uint16_t ret = 0U;
#if gNvStorageIncluded_d && gNvUseExtendedFeatureSet_d
    /* Avoid recursion: read directly without calling InitNVMConfig() */
    if (mNvFlashConfigInitialised && (gVirtualPageNone_c != mNvActivePageId))
    {
        ret = (*(NVM_TableInfo_t *)(mNvVirtualPageProperty[mNvActivePageId].NvRawSectorStartAddress))
                  .u.fields.NvTableVersion;
    }
    else
    {
        /* If not initialized, return default/invalid version */
        ret = 0U;
    }
#endif
    return ret;
}

/******************************************************************************
 * Name: RecoverNvEntry
 * Description: Reads a flash entry so that the application can handle dynamic entries.
 *              Exposed as public API mostly for test purposes.
 * Parameter(s): [IN] index - the ram entry index
 *               [OUT] entry - the flash entry at the specified index
 * Return: gNVM_OK_c - if the operation completes successfully
           gNVM_RestoreFailure_c - if the operation failed
           gNVM_AddressOutOfRange_c - if the index is too large
           gNVM_Error_c - not supported, NVM table is stored in FLASH
 *****************************************************************************/
NVM_Status_t RecoverNvEntry(uint16_t index, NVM_DataEntry_t *entry)
{
#if gNvStorageIncluded_d && gNvUseExtendedFeatureSet_d
    NVM_EntryInfo_t entryInfo;
    NVM_Status_t    status = gNVM_OK_c;

    entry->pData = NULL;
    InitNVMConfig();
    if (mNvActivePageId == gVirtualPageNone_c)
    {
        status = gNVM_RestoreFailure_c;
    }
    else
    {
        if (index * sizeof(NVM_EntryInfo_t) >= mNvTableSizeInFlash)
        {
            status = gNVM_AddressOutOfRange_c;
        }
        else
        {
            uint16_t rd_offset = 0U;
            status             = gNVM_Error_c;
            /* MISRA 10.3: explicit cast of sizeof results to uint16_t */
            if (NvComputeEltOffset(index, (uint16_t)sizeof(NVM_EntryInfo_t), 0U, (uint16_t)sizeof(NVM_TableInfo_t),
                                   &rd_offset) == 0)
            {
                status = NV_PartitionReadAtOffset(mNvActivePageId, rd_offset, (uint8_t *)&entryInfo,
                                                  sizeof(NVM_EntryInfo_t));
                if (gNVM_OK_c == status)
                {
                    entry->DataEntryID   = entryInfo.u.fields.NvDataEntryID;
                    entry->DataEntryType = entryInfo.u.fields.NvDataEntryType;
                    entry->ElementsCount = entryInfo.u.fields.NvElementsCount;
                    entry->ElementSize   = entryInfo.u.fields.NvElementSize;
                }
            }
        }
    }
    return status;
#else  /*gNvUseExtendedFeatureSet_d*/
    (void)index;
    (void)entry;
    return gNVM_Error_c;
#endif /*gNvUseExtendedFeatureSet_d*/
}

/******************************************************************************
 * Name: NvSetNvmDataTable
 * Description: Set Data entry array and number of entries.
 *  Exposed as public API mostly for test purposes.
 * Parameter(s): none
 *
 * Return: none
 *****************************************************************************/
void NvSetNvmDataTable(NVM_DataEntry_t *tb_array, uint16_t nb_entries)
{
#if gNvStorageIncluded_d
    if ((tb_array == NULL) || (nb_entries == 0U))
    {
        /* By default if arguments are unspecified, the default applies */
        pNVM_DataTable          = (NVM_DataEntry_t *)gNVM_TABLE_startAddr_c;
        mNVM_DataTableNbEntries = gNVM_TABLE_entries_c;
    }
    else
    {
        pNVM_DataTable          = tb_array;
        mNVM_DataTableNbEntries = nb_entries;
    }
#endif
}

/******************************************************************************
 * Name: NvModuleInit
 * Description: Initialize the NV storage module
 * Parameter(s): -
 * Return: gNVM_ModuleAlreadyInitialized_c - if the module is already
 *                                           initialized
 *         gNVM_InvalidSectorsCount_c - if the sector count configured in the
 *                                      project linker file is invalid
 *         gNVM_MetaNotFound_c - if no meta information was found
 *         gNVM_OK_c - module was successfully initialized
 *         gNVM_CannotCreateMutex_c - no mutex available
 *****************************************************************************/
NVM_Status_t NvModuleInit(void)
{
#if gNvStorageIncluded_d
    NVM_Status_t status = gNVM_OK_c;
    if (mNvModuleInitialized)
    {
        status = gNVM_ModuleAlreadyInitialized_c;
    }
    else
    {
        status = __NvModuleInit(TRUE);
    }
    if ((gNVM_OK_c == status) && (FALSE == mNvMutexCreated))
    {
        /* Create the Mutex only the first time if module initialization was successful */
        if (OSA_MutexCreate(mNVMMutexId) != KOSA_StatusSuccess)
        {
            /* Reset mNvModuleInitialized : other actions are useless but by precaution */
            NvModuleDeInit();
            /* mNvMutexCreated remains FALSE */
            status = gNVM_CannotCreateMutex_c;
        }
        else
        {
            /* Remember that mutex was created so that we keep it without destroying it in case of NVM
             * reinitialization, which turns out useful in the context of tests  */
            mNvMutexCreated = TRUE;
        }
    }

    return status;
#else
    return gNVM_Error_c;
#endif /* #if gNvStorageIncluded_d */
}

/******************************************************************************
 * Name: NvModuleDeInit
 * Description: Reset NVM context variables.
 *  Exposed as public API mostly for test purposes.
 * Parameter(s): none
 *
 * Return: none
 *****************************************************************************/
void NvModuleDeInit(void)
{
#if gNvStorageIncluded_d
    mNvPageCounter          = ~0UL;
    mNVM_DataTableNbEntries = 0U;
    FLib_MemSet(&mNvVirtualPageProperty[0], 0U,
                gNvVirtualPagesCount_c * sizeof(NVM_VirtualPageProperties_t)); /*! virtual page properties */

    mNvCopyOperationIsPending = FALSE;

    mNvErasePgCmdStatus.NvErasePending = FALSE;
    mNvErasePgCmdStatus.NvPageToErase  = gVirtualPageNone_c;
    mNvErasePgCmdStatus.NvSectorIndex  = 0U; /* erase from start of page */

    mNvFlashConfigInitialised = FALSE;

#if gNvFragmentation_Enabled_d
    FLib_MemSet((void *)&maNvRecordsCpyOffsets[0], 0U, sizeof(maNvRecordsCpyOffsets));
#endif
    mNvIdleTaskId = NULL;
#if gNvUseExtendedFeatureSet_d
    mNvTableSizeInFlash  = 0U;
    mNvTableMarker       = 0U;
    mNvFlashTableVersion = 0U;
    mNvTableUpdated      = FALSE;
#endif /* gNvUseExtendedFeatureSet_d */

    mNvCriticalSectionFlag = 0U;

#if (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
    mNvMinimumTicksBetweenSaves = gNvMinimumTicksBetweenSaves_c;
    mNvSaveOnIntervalEvent      = FALSE; /*! flag used to signal an 'SaveOnInterval' event */
    mNvLastTimestampValue       = 0ULL;
#endif
    mNvCountsBetweenSaves = gNvCountsBetweenSaves_c;

#if gNvFragmentation_Enabled_d
    FLib_MemSet(&mNvPendingSavesQueue, 0U, sizeof(maNvRecordsCpyOffsets[0]));
#endif
    FLib_MemSet(&maDatasetInfo[0], 0U, gNvTableEntriesCountMax_c * sizeof(NVM_DatasetInfo_t));

    mNvActivePageId = gVirtualPageNone_c;

#if gNvUseExtendedFeatureSet_d
    mNvTableMarker       = gNvTableMarker_c;
    mNvFlashTableVersion = gNvFlashTableVersion_c;
    mNvTableUpdated      = FALSE;
#endif /* gNvUseExtendedFeatureSet_d */

#if gNvDualImageSupport_d
    mNvNeedAddEntryCnt = 0U;
    FLib_MemSet(mNvDiffEntryId, 0xffU, gNvTableEntriesCountMax_c * sizeof(mNvDiffEntryId[0]));
    mNvPreviousActivePageId = gVirtualPageNone_c;
#endif
    mNvModuleInitialized = FALSE;
#endif
}

/******************************************************************************
 * Name: NvModuleReInit
 * Description: Reinit the NV module , reload from Flash to RAM the latest NVM changes
 *      Useful for RAM off use case in lowpower
 *    Same as __NvModuleInit without call to NV_Init(); and no mNvModuleInitialized protection
 *
 *****************************************************************************/
NVM_Status_t NvModuleReInit(void)
{
    NVM_Status_t status = gNVM_OK_c;
#if gNvStorageIncluded_d
    status = __NvModuleInit(FALSE);
#endif
    return status;
}

/******************************************************************************
 * Name: NvMoveToRam
 * Description: Move from NVM to Ram an unmirrored dataset
 * Parameter(s):  ppData     double pointer to the entity to be moved from flash to RAM
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_NoMemory_c - in case there is not a memory block free
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 *****************************************************************************/
NVM_Status_t NvMoveToRam(void **ppData)
{
#if gNvStorageIncluded_d && gUnmirroredFeatureSet_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvmMoveToRam under mutex protection */
        status = __NvmMoveToRam(ppData);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    ppData = ppData;
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvErase
 * Description: Erase from NVM an unmirrored dataset
 * Parameter(s):  ppData     double pointer to the entity to be moved from flash to RAM
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_NoMemory_c - in case there is not a memory block free
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 *****************************************************************************/

NVM_Status_t NvErase(void **ppData)
{
#if gNvStorageIncluded_d && gUnmirroredFeatureSet_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvmErase under mutex protection */
        status = __NvmErase(ppData);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    ppData = ppData;
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvSaveOnIdle
 * Description: Save the data pointed by ptrData on the next call to NvIdle()
 * Parameter(s): [IN] ptrData - pointer to data to be saved
 *               [IN] saveAll - specify if all the elements from the NVM table
 *                              entry shall be saved
 * Return: gNVM_OK_c - if operation completed successfully
 *         gNVM_Error_c - in case of error(s)
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_Status_t NvSaveOnIdle(void *ptrData, bool_t saveAll)
{
#if gNvStorageIncluded_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvSaveOnIdle under mutex protection */
        status = __NvSaveOnIdle(ptrData, saveAll);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    NOT_USED(ptrData);
    NOT_USED(saveAll);
    return gNVM_Error_c;
#endif /* # gNvStorageIncluded_d */
}

/******************************************************************************
 * Name: NvSaveOnInterval
 * Description:  save no more often than a given time interval. If it has
 *               been at least that long since the last save,
 *               this function will cause a save the next time the idle
 *               task runs.
 * Parameters: [IN] ptrData - pointer to data to be saved
 * NOTE: this function saves all the element of the table entry pointed by
 *       ptrData
 * Return: NVM_OK_c - if operation completed successfully
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_Status_t NvSaveOnInterval(void *ptrData)
{
#if gNvStorageIncluded_d && (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvSaveOnInterval under mutex protection */
        status = __NvSaveOnInterval(ptrData);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    NOT_USED(ptrData);
    return gNVM_Error_c;
#endif
} /* NvSaveOnInterval() */

/******************************************************************************
 * Name: NvSaveOnCount
 * Description: Decrement the counter. Once it reaches 0, the next call to
 *              NvIdle() will save the entire table entry (all elements).
 * Parameters: [IN] ptrData - pointer to data to be saved
 * Return: NVM_OK_c - if operation completed successfully
 *         Note: see also return codes of NvGetEntryFromDataPtr() function
 ******************************************************************************/
NVM_Status_t NvSaveOnCount(void *ptrData)
{
#if gNvStorageIncluded_d && (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnIdleCount_c)

    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvSaveOnCount under mutex protection */
        status = __NvSaveOnCount(ptrData);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;

#else
    ptrData = ptrData;
    return gNVM_Error_c;
#endif
} /* NvSaveOnCount() */

/******************************************************************************
 * Name: NvSetMinimumTicksBetweenSaves
 * Description: Set the timer used by NvSaveOnInterval(). Takes effect after
 *              the next save.
 * Parameters: [IN] newInterval - new time interval
 * Return: -
 ******************************************************************************/
void NvSetMinimumTicksBetweenSaves(NvSaveInterval_t newInterval)
{
#if gNvStorageIncluded_d && (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)
    mNvMinimumTicksBetweenSaves = newInterval;
#else
    newInterval = newInterval;
#endif
} /* NvSetMinimumTicksBetweenSaves() */

/******************************************************************************
 * Name: NvSetCountsBetweenSaves
 * Description: Set the counter trigger value used by NvSaveOnCount().
 *              Takes effect after the next save.
 * Parameters: [IN] newCounter - new counter value
 * Return: -
 ******************************************************************************/
void NvSetCountsBetweenSaves(NvSaveCounter_t newCounter)
{
#if gNvStorageIncluded_d
    mNvCountsBetweenSaves = newCounter;
#else
    newCounter  = newCounter;
#endif
} /* NvSetCountsBetweenSaves() */

/******************************************************************************
 * Name: NvTimerTick
 * Description: Called from the idle task to process save-on-interval requests
 * Parameters: [IN] countTick - enable/disable tick count
 * Return: FALSE if the timer tick counters for all data sets have reached
 *         zero. In this case, the timer can be turned off.
 *         TRUE if any of the data sets' timer tick counters have not yet
 *         counted down to zero. In this case, the timer should be active
 ******************************************************************************/
bool_t NvTimerTick(bool_t countTick)
{
    bool_t fTicksLeft = FALSE;
#if gNvStorageIncluded_d && (gNvmSaveOnIdlePolicy_d & gNvmUseSaveOnTimerOn_c)

    if (mNvModuleInitialized)
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        fTicksLeft = __NvTimerTick(countTick);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
#else
    NOT_USED(countTick);
#endif /* #if gNvStorageIncluded_d */

    return fTicksLeft;
} /* NvTimerTick() */

/******************************************************************************
 * Name: NvRestoreDataSet
 * Description: copy the most recent version of the element/table entry pointed
 *              by ptrData from NVM storage system to RAM memory
 * Parameter(s): [IN] ptrData - pointer to data (element) to be restored
 *               [IN] restoreAll - if FALSE restores a single element
 *                               - if TRUE restores an entire table entry
 * Return: status of the restore operation
 *****************************************************************************/
NVM_Status_t NvRestoreDataSet(void *ptrData, bool_t restoreAll)
{
#if gNvStorageIncluded_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* before any data restore, complete any NVM pending operations */
        NvCompletePendingOperations();
        status = __NvRestoreDataSet(ptrData, restoreAll);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    ptrData    = ptrData;
    restoreAll = restoreAll;
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvClearCriticalSection
 * Description: leave critical section
 * Parameters: -
 * Return: -
 ******************************************************************************/
void NvClearCriticalSection(void)
{
#if (gNvStorageIncluded_d && gNvEnableCriticalSection_c)
    OSA_SR_ALLOC();
    OSA_ENTER_CRITICAL();
    if (mNvCriticalSectionFlag > 0U) /* in case of set/clear mismatch */
    {
        --mNvCriticalSectionFlag;
    }

    OSA_EXIT_CRITICAL();
#endif
}

/******************************************************************************
 * Name: NvSetCriticalSection
 * Description: enter critical section
 * Parameters: -
 * Return: -
 ******************************************************************************/
void NvSetCriticalSection(void)
{
#if (gNvStorageIncluded_d && gNvEnableCriticalSection_c)
    OSA_SR_ALLOC();
    OSA_ENTER_CRITICAL();
    if (mNvCriticalSectionFlag < (uint8_t)UINT8_MAX)
    {
        ++mNvCriticalSectionFlag;
    }
    OSA_EXIT_CRITICAL();
#endif
}
/******************************************************************************
 * Name: NvIdle
 * Description: Called from the idle task (bare-metal) or NVM_Task (MQX,
 *              FreeRTOS) to process the pending saves, erase or copy
 *              operations.
 * Parameters: -
 * Return: Number of operations executed.
 ******************************************************************************/
int NvIdle(void)
{
    int nb_operation = 0;
#if gNvStorageIncluded_d
    if (mNvModuleInitialized == TRUE)
    {
        if (mNvIdleTaskId == NULL)
        {
            mNvIdleTaskId = OSA_TaskGetCurrentHandle();
        }
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        nb_operation = __NvIdle();
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
#endif
    return nb_operation;
} /* NvIdle() */
/******************************************************************************
 * Name: NvGetNvIdleTaskId
 * Description:
 * Parameters: -
 * Return: returns the Id of the task which hosts NvIdle function
 ******************************************************************************/
void *NvGetNvIdleTaskId(void)
{
#if gNvStorageIncluded_d
    return (void *)mNvIdleTaskId;
#else
    return NULL;
#endif
} /* NvIdle() */

/******************************************************************************
 * Name: NvIsDataSetDirty
 * Description: return TRUE if the element pointed by ptrData is dirty
 * Parameters: [IN] ptrData - pointer to data to be checked
 * Return: TRUE if the element is dirty, FALSE otherwise
 ******************************************************************************/
bool_t NvIsDataSetDirty(void *ptrData)
{
#if gNvStorageIncluded_d
    bool_t res = FALSE;
    if (mNvModuleInitialized)
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvIsDataSetDirty under mutex protection */
        res = __NvIsDataSetDirty(ptrData);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return res;
#else
    ptrData = ptrData;
    return FALSE;
#endif
}

/******************************************************************************
 * Name: NvGetStatistics
 * Description:
 * Parameter(s): [OUT] ptrStat - pointer to a memory location where the pages
 *                               statistics (erase cycles of each page) will
 *                               be stored
 * Return: -
 *****************************************************************************/
void NvGetPagesStatistics(NVM_Statistics_t *ptrStat)
{
#if gNvStorageIncluded_d
    if (TRUE == mNvModuleInitialized)
    {
        if (NULL != ptrStat)
        {
            if (0U != (mNvPageCounter % 2U))
            {
                ptrStat->SecondPageEraseCyclesCount = (mNvPageCounter - 1U) / 2U;
                ptrStat->FirstPageEraseCyclesCount  = ptrStat->SecondPageEraseCyclesCount;
            }
            else
            {
                ptrStat->FirstPageEraseCyclesCount  = mNvPageCounter / 2U;
                ptrStat->SecondPageEraseCyclesCount = (mNvPageCounter - 2U) / 2U;
            }
        }
    }
#else
    ptrStat = ptrStat;
#endif
}

/******************************************************************************
 * Name: NvGetPagesSize
 * Description: Retrieves the NV Virtual Page size
 * Parameter(s): [OUT] pPageSize - pointer to a memory location where the page
 *                                 size will be stored
 * Return: -
 *****************************************************************************/
void NvGetPagesSize(uint32_t *pPageSize)
{
    if (NULL != pPageSize)
    {
#if gNvStorageIncluded_d
        *pPageSize = mNvTotalPageSize;
#else
        *pPageSize = 0U;
#endif
    }
}

/******************************************************************************
 * Name: NvFormat
 * Description: Format the NV storage system. The function erases both virtual
 *              pages and then writes the page counter/ram table to active page.
 * Parameter(s): -
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_FormatFailure_c - if the format operation fails
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_CriticalSectionActive_c - if the system has entered in a
 *                                        critical section
 *****************************************************************************/
NVM_Status_t NvFormat(void)
{
#if gNvStorageIncluded_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvFormat under mutex protection */
        status = __NvFormat();
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    return gNVM_Error_c;
#endif /* gNvStorageIncluded_d */
}

/******************************************************************************
 * Name: NvRegisterTableEntry
 * Description: The function tries to register a new table entry within an
 *              existing NV table. If the NV table contained an erased (invalid)
 *              entry, the entry will be overwritten with a new one (provided
 *              by the mean of this function arguments)
 * Parameter(s): [IN] ptrData - generic pointer to RAM data to be registered
 *                              within the NV storage system
 *               [IN] uniqueId - an unique ID of the table entry
 *               [IN] elemCount - how many elements the table entry contains
 *               [IN] elemSize - the size of an element
 *               [IN] dataEntryType - the type of the new entry
 *               [IN] overwrite - if an existing table entry shall be
 *                                overwritten
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *****************************************************************************/
NVM_Status_t NvRegisterTableEntry(void            *ptrData,
                                  NvTableEntryId_t uniqueId,
                                  uint16_t         elemCount,
                                  uint16_t         elemSize,
                                  uint16_t         dataEntryType,
                                  bool_t           overwrite)
{
#if gNvStorageIncluded_d && gNvUseExtendedFeatureSet_d && gNvTableKeptInRam_d

    NVM_Status_t status;
    (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
    /* Call __NvRegisterTableEntry under mutex protection */
    status = __NvRegisterTableEntry(ptrData, uniqueId, elemCount, elemSize, dataEntryType, overwrite);
    (void)OSA_MutexUnlock(mNVMMutexId);
    return status;
#else
    ptrData   = ptrData;
    uniqueId  = uniqueId;
    elemCount = elemCount;
    elemSize  = elemSize;
    overwrite = overwrite;
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvEraseEntryFromStorage
 * Description: The function removes a table entry within the existing NV
 *              table.
 * Parameter(s): [IN] ptrData - a pointer to an existing RAM data that is
 *                              managed by the NV storage system
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *****************************************************************************/
NVM_Status_t NvEraseEntryFromStorage(void *ptrData)
{
#if gNvStorageIncluded_d && gNvUseExtendedFeatureSet_d && gNvTableKeptInRam_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        NVM_TableEntryInfo_t tblIdx;
        uint16_t             tableEntryIdx;
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);

        status = NvGetTableEntryIndexFromDataPtr(ptrData, &tblIdx, &tableEntryIdx);
        if (gNVM_OK_c == status)
        {
            /* invalidate the table entry */
            pNVM_DataTable[tableEntryIdx].pData         = NULL;
            pNVM_DataTable[tableEntryIdx].ElementsCount = 0U;
            pNVM_DataTable[tableEntryIdx].ElementSize   = 0U;
            status                                      = __NvEraseEntryFromStorage(tblIdx.entryId, tableEntryIdx);
        }
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    ptrData = ptrData;
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvSyncSave
 * Description: The function saves the pointed element or the entire table
 *              entry to the storage system. The save operation is not
 *              performed on the idle task but within this function call.
 * Parameter(s): [IN] ptrData - a pointer to data to be saved
 *               [IN] saveAll - specifies if the entire table entry shall be
 *                              saved or only the pointed element
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *         gNVM_PointerOutOfRange_c - if the pointer is out of range
 *         gNVM_InvalidTableEntry_c - if the table entry is not valid
 *         gNVM_MetaInfoWriteError_c - meta tag couldn't be written
 *         gNVM_RecordWriteError_c - record couldn't be written
 *         gNVM_CriticalSectionActive_c - the module is in critical section
 *****************************************************************************/
NVM_Status_t NvSyncSave(void *ptrData, bool_t saveAll)
{
#if gNvStorageIncluded_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvSyncSave (unsafe) under mutex protection */
        status = __NvSyncSave(ptrData, saveAll);
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    ptrData = ptrData;
    saveAll = saveAll;
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvAtomicSave
 * Description: The function performs an atomic save of the entire NV table
 *              to the storage system. The operation is performed
 *              in place (atomic).
 * Parameter(s):  -
 * Return: gNVM_OK_c - if the operation completes successfully
 *         gNVM_ModuleNotInitialized_c - if the NVM  module is not initialized
 *         gNVM_NullPointer_c - if a NULL pointer is provided
 *         gNVM_PointerOutOfRange_c - if the pointer is out of range
 *         gNVM_InvalidTableEntry_c - if the table entry is not valid
 *         gNVM_MetaInfoWriteError_c - meta tag couldn't be written
 *         gNVM_RecordWriteError_c - record couldn't be written
 *         gNVM_CriticalSectionActive_c - the module is in critical section
 *****************************************************************************/
NVM_Status_t NvAtomicSave(void)
{
#if gNvStorageIncluded_d
    NVM_Status_t status;
    if (!mNvModuleInitialized)
    {
        status = gNVM_ModuleNotInitialized_c;
    }
    else
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvAtomicSave (unsafe) under mutex protection */
        status = __NvAtomicSave();
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return status;
#else
    return gNVM_Error_c;
#endif
}

/******************************************************************************
 * Name: NvShutdown
 * Description: The function waits for all idle saves to be processed.
 * Parameter(s):  -
 * Return: -
 *****************************************************************************/
void NvShutdown(void)
{
#if gNvStorageIncluded_d
    if (mNvModuleInitialized)
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call __NvShutdown (UNSAFE) under mutex protection */
        __NvShutdown();
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
#endif
}

/******************************************************************************
 * Name: NvCompletePendingOperations
 * Description: The function attemps to complete all the NVM related pending
 *              operations.
 * Parameter(s):  -
 * Return: -
 *****************************************************************************/
void NvCompletePendingOperations(void)
{
#if gNvStorageIncluded_d
    if (mNvModuleInitialized)
    {
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call NvCompletePendingOperationsUnsafe under mutex protection */
        NvCompletePendingOperationsUnsafe();
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
#endif
}

/******************************************************************************
 * Name: NvRegisterEccFaultNotificationCb
 * Description: Register fault notification callback.
 *
 * Parameter(s):  cb [IN] callback to register
 * Return: gNVM_OK_c if ok, gNVM_Error_c otherwise.
 *****************************************************************************/
int NvRegisterEccFaultNotificationCb(NVM_EccFaultNotifyCb_t cb)
{
    NVM_Status_t status = gNVM_OK_c;
#if gNvStorageIncluded_d && (defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0))
    /* nv_fault_report_cb global callback pointer. No error raised if a previous callback was already registered */
    nv_fault_report_cb = cb;
#else
    /* if gNvSalvageFromEccFault_d is undefined but registering a fault report callback is attempted,
     * it may denote a configuration error*/
    if (cb != NULL)
    {
        status = gNVM_Error_c;
    }
#endif
    return (int)status;
}

#ifdef USE_MSD_BOOTLOADER
/******************************************************************************
 * Name: NvEraseSector
 * Description: The function performs the storage system sector erase.
 *              The operation is performed in place (atomic).
 * Parameter(s):  [IN] ignoreCriticalSectionFlag - if set to TRUE, the critical
 *                                                section flag is ignored
 * Return: void
 *****************************************************************************/
void NvEraseSector(uint32_t sectorAddr)
{
#if gNvStorageIncluded_d
    /* erase sector */
    (void)NV_FlashEraseSector(sectorAddr, (uint32_t)((uint8_t *)NV_STORAGE_SECTOR_SIZE));
#else
    (void)sectorAddr;
#endif
}
#endif

/*! *********************************************************************************
 *  \brief Tell if there is a pending NVM operation in the queue
 *
 * \return bool_t Is there a pending operation in the queue
 ********************************************************************************* */
bool_t NvIsPendingOperation(void)
{
    bool_t IsPending = FALSE;
#if gNvStorageIncluded_d
    if (NvGetPendingSavesCount() != 0U)
    {
        IsPending = TRUE;
    }
#endif
    return IsPending;
}

#if gNvStorageIncluded_d && (defined gNvSalvageFromEccFault_d && (gNvSalvageFromEccFault_d > 0))
/*
 * NV_SweepRangeForEccFaults is required to be located in RAM mostly because of the
 * test mode to simulate ECC faults.
 */
NVM_STATIC uint32_t NV_SweepRangeForEccFaults(uint32_t start_addr, uint32_t size)
{
    uint32_t regPrimask     = DisableGlobalIRQ();
    uint32_t ecc_fault_addr = 0U;

    uint32_t addr = ROUND_FLOOR(start_addr, INT_FLASH_PHRASE_SZ_LOG2);
    uint32_t end  = addr + size - 1U;

    for (; addr < end; addr += FSL_FEATURE_FLASH_PFLASH_PHRASE_SIZE)
    {
        uint32_t read_val;
        /* Dummy read just to probe ECC error */
        if (HAL_FlashReadCheckEccFaults(addr, sizeof(uint32_t), (uint8_t *)&read_val) == kStatus_HAL_Flash_EccError)
        {
            ecc_fault_addr = addr;
            Nv_ReportEccFault(ecc_fault_addr, 1);
            break;
        }
    }
    EnableGlobalIRQ(regPrimask);
    return ecc_fault_addr;
}
#endif /* gNvSalvageFromEccFault_d */

uint16_t Nv_GetFirstMetaOffset(void)
{
#if gNvStorageIncluded_d
    return (uint16_t)gNvFirstMetaOffset_c;
#else
    return 0U;
#endif
}

uint32_t NvGetTableSizeInFlash(void)
{
#if gNvStorageIncluded_d && gNvUseExtendedFeatureSet_d
    return (uint32_t)mNvTableSizeInFlash;
#else
    return 0U;
#endif
}

/*! *********************************************************************************
 *  \brief NvSetFlashTableVersion
 *
 * Parameter(s):  [IN] version version number. Normally a set with gNvFlashTableVersion_c
 * constant but may need to be modified by application.
 *
 * \return none
 ********************************************************************************* */
void NvSetFlashTableVersion(uint16_t version)
{
#if gNvStorageIncluded_d && gNvUseExtendedFeatureSet_d
    mNvFlashTableVersion = version;
#else
    NOT_USED(version);
#endif
}

#if defined gNvmMetaCheckSum_d
/*! *********************************************************************************
 *  \brief NvSetChecksumEnable
 *
 * Parameter(s):  [IN] version version number. Normally a set with gNvFlashTableVersion_c
 * constant but may need to be modified by application.
 *
 * \return none
 ********************************************************************************* */
void NvSetChecksumEnable(int enabled)
{
#if gNvStorageIncluded_d
    mNvMetaInfoChecksumEnabled = enabled;
#else
    NOT_USED(enabled);
#endif
}
#endif

/******************************************************************************
 * \brief Return the current page free space, in bytes
 *
 * \return 0u is NVM not uninitialized or other error was encountered when parsing the NVM page.
 *         otherwise, remaining capacity of the current NVM virtual page in bytes
 *****************************************************************************/
uint32_t NvGetPageCapacityInBytes(void)
{
    NVM_Status_t status = gNVM_OK_c;

    uint32_t page_capacity = 0U;
    if (mNvModuleInitialized)
    {
        /* If the NVM is not initialized yet, no storage capacity available */
        (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
        /* Call NvCompletePendingOperationsUnsafe under mutex protection */
        status = NvGetPageFreeSpace(&page_capacity, FALSE);
        if (status != gNVM_OK_c)
        {
            /* In case of error return 0 */
            page_capacity = 0U;
        }
        (void)OSA_MutexUnlock(mNVMMutexId);
    }
    return page_capacity;
}

#if gNvStorageIncluded_d && (defined gNvDebugEnabled_d && (gNvDebugEnabled_d > 0))
/*
 * Internal static debug functions accessing static data of NVM module.
 */
/*! *********************************************************************************
 *  \brief NvFlashDump
 * Used only for debug purposes
 *
 * Parameter(s):
 *  [IN] ptr address from which to dum
 *  [IN] data_size number of bytes to dump.
 *
 * \return none
 ********************************************************************************* */
NVM_STATIC void NvFlashDump(uint8_t *ptr, uint16_t data_size)
{
    char message[128];
    int  lg = 0;

    uint16_t cnt = 0U;
    PRINTF("\r\nData(%d) @%x", data_size, ptr);
    int     remaining_size;
    uint8_t ram_buf[16u] = {0U};
    for (remaining_size = data_size; remaining_size > 0;)
    {
        uint16_t size;
        if (remaining_size > 16u)
        {
            size = 16u;
        }
        else
        {
            size = remaining_size;
        }
        if (NV_FlashRead((uint32_t)ptr, ram_buf, size, TRUE) != gNVM_OK_c)
        {
            lg = sprintf(message, "\r\n[%08lx]: xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx", (uint32_t)ptr);
        }
        else
        {
            lg = 0;
        }
        lg += sprintf(&message[lg], "\r\n[%08lx]: %02x", (uint32_t)ptr, ram_buf[0]);
        for (cnt = 1; cnt < size; cnt++)
        {
            lg += sprintf(&message[lg], " %02x", ram_buf[cnt]);
        }

        PRINTF(message);

        ptr += size;
        remaining_size -= size;
    }
    PRINTF("\r\n");
}

/*! *********************************************************************************
 *  \brief NV_ShowPageMetas
 * Used only for debug purposes. Does nothing if not built with gNvDebugEnabled_d.
 *
 * Parameter(s):
 *  [IN] ptr address from which to dum
 *  [IN] data_size number of bytes to dump.
 *
 * \return none
 ********************************************************************************* */
NVM_STATIC void NV_ShowPageMetas(NVM_VirtualPageID_t page_id, bool_t ecc_checks)
{
    char                 message[150];
    char                *record_type;
    uint32_t             metaInfoAddress;
    NVM_RecordMetaInfo_t metaInfo;
    uint16_t             bytes_to_read = 0U;
    uint16_t             entry_index;
#if defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d > 0)
    uint32_t checksum;
#endif
    NVM_VirtualPageProperties_t *vpage_prop = &mNvVirtualPageProperty[page_id];

    if ((metaInfoAddress = NV_PAGE_ADDR((uint8_t)(vpage_prop - &mNvVirtualPageProperty[0]),
                                        vpage_prop->NvLastMetaInfoOffset)) == 0U ||
        vpage_prop->NvLastMetaInfoOffset == gNvInvalidMetaOffset_c)
    {
        return;
    }
    NV_ShowPageTableInfo(page_id, ecc_checks);

    NvFlashDump((uint8_t *)vpage_prop->NvRawSectorStartAddress,
                (uint16_t)vpage_prop->NvLastMetaInfoOffset + sizeof(NVM_RecordMetaInfo_t));

    PRINTF("\r\nMost recent to oldest:\r\n");

    while (metaInfoAddress >= (vpage_prop->NvRawSectorStartAddress + gNvFirstMetaOffset_c))
    {
        bytes_to_read     = 0U;
        uint8_t *meta_ptr = (uint8_t *)&metaInfo;
        (void)NV_FlashRead(metaInfoAddress, (uint8_t *)&metaInfo, sizeof(NVM_RecordMetaInfo_t),
                           (vpage_prop->has_ecc_faults || ecc_checks));

        int lg = 0;
        lg += sprintf(message, "Meta @ 0x%lx:", metaInfoAddress);
        for (uint8_t cnt = 0U; cnt < sizeof(NVM_RecordMetaInfo_t); cnt++)
        {
            lg += sprintf(&message[lg], "%02x ", meta_ptr[cnt]);
        }
        PRINTF(message);
        PRINTF("\r\n");
#if defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d > 0)
        if (metaInfo.NvmMetaChecksum != 0xffffffffUL)
        {
            checksum = NvCalculateChecksum(&metaInfo);
            if (checksum != 0xffffffffU)
            {
                record_type   = "invalid";
                bytes_to_read = 0U;
            }
        }
        else
#endif
            if (metaInfo.u.fields.NvValidationStartByte != metaInfo.u.fields.NvValidationEndByte)
        {
            record_type   = "invalid";
            bytes_to_read = 0U;
        }
        else if (metaInfo.u.fields.NvValidationStartByte == gValidationByteAllRecords_c)
        {
            record_type   = "all";
            bytes_to_read = 0U;
            entry_index   = NvGetTableEntryIndexFromId(metaInfo.u.fields.NvmDataEntryID);
            if (entry_index != gNvInvalidTableEntryIndex_c)
            {
                bytes_to_read = pNVM_DataTable[entry_index].ElementsCount * pNVM_DataTable[entry_index].ElementSize;
            }
        }
        else
        {
            record_type = "single";
            entry_index = NvGetTableEntryIndexFromId(metaInfo.u.fields.NvmDataEntryID);
            if (entry_index != gNvInvalidTableEntryIndex_c)
            {
                bytes_to_read = pNVM_DataTable[entry_index].ElementSize;
            }
        }
#if !(defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d > 0))
        sprintf(message, "VSB=%02x ID=%04x Index=%04x Offset=%04x VEB=%02X type=%7s\r\n",
                metaInfo.u.fields.NvValidationStartByte, metaInfo.u.fields.NvmDataEntryID,
                metaInfo.u.fields.NvmElementIndex, metaInfo.u.fields.NvmRecordOffset,
                metaInfo.u.fields.NvValidationEndByte, record_type);
#else
        sprintf(message, "VSB=%02x ID=%04x Index=%04x Offset=%04x VEB=%02X Checksum=%08lx type=%7s\r\n",
                metaInfo.u.fields.NvValidationStartByte, metaInfo.u.fields.NvmDataEntryID,
                metaInfo.u.fields.NvmElementIndex, metaInfo.u.fields.NvmRecordOffset,
                metaInfo.u.fields.NvValidationEndByte, metaInfo.NvmMetaChecksum, record_type);
#endif
        PRINTF(message);
        if (bytes_to_read > 128u)
        {
            bytes_to_read = 128u;
        }
        if (bytes_to_read > 0U)
        {
            NV_ShowDataEntry((uint8_t *)vpage_prop->NvRawSectorStartAddress + metaInfo.u.fields.NvmRecordOffset,
                             bytes_to_read);
        }
        metaInfoAddress -= sizeof(NVM_RecordMetaInfo_t);
    }
    PRINTF("\r\n");
}

/*! *********************************************************************************
 *  \brief NV_ShowPageTableInfo
 * Used only for debug purposes. Does nothing if not built with gNvDebugEnabled_d.
 * see @NV_ShowPageMetas
 * Parameter(s):
 *  [IN] ptr address from which to dum
 *  [IN] data_size number of bytes to dump.
 *
 * \return none
 ********************************************************************************* */
NVM_STATIC void NV_ShowPageTableInfo(NVM_VirtualPageID_t page_id, bool_t ecc_checks)
{
    NVM_VirtualPageProperties_t *vpage_prop = &mNvVirtualPageProperty[page_id];

    NVM_TableInfo_t tableInfo;

    if (NV_FlashRead(vpage_prop->NvRawSectorStartAddress, (uint8_t *)&tableInfo, sizeof(NVM_TableInfo_t), TRUE) !=
        gNVM_OK_c)
    {
        PRINTF("Detected Ecc error in table info of page %d\r\n", page_id);
    }
    else
    {
#if gNvUseExtendedFeatureSet_d
        PRINTF("Table PageCounter=%08x\r\n", tableInfo.u.fields.NvPageCounter);
        PRINTF("Table Marker=%04x\r\n", tableInfo.u.fields.NvTableMarker);
        PRINTF("Table Version=%04x\r\n", tableInfo.u.fields.NvTableVersion);
#else
        PRINTF("Table PageCounter=%08x %08x\r\n", *(uint32_t *)&tableInfo.u.fields.NvPageCounter,
               *(((uint32_t *)&tableInfo.u.fields.NvPageCounter) + 1u));
#endif
        PRINTF("Table Padding %02x %02x %02x %02x %02x %02x %02x %02x\r\n", tableInfo.u.fields.Padding[0],
               tableInfo.u.fields.Padding[1], tableInfo.u.fields.Padding[2], tableInfo.u.fields.Padding[3],
               tableInfo.u.fields.Padding[4], tableInfo.u.fields.Padding[5], tableInfo.u.fields.Padding[6],
               tableInfo.u.fields.Padding[7]);
    }
}

/*! *********************************************************************************
 *  \brief NV_ShowDataEntry
 * Used only for debug purposes. Does nothing if not built with gNvDebugEnabled_d.
 *
 * Parameter(s):
 *  [IN] ptr address from which to dum
 *  [IN] data_size number of bytes to dump.
 *
 * \return none
 ********************************************************************************* */
void NV_ShowDataEntry(uint8_t *ptr, uint16_t data_size)
{
    NvFlashDump(ptr, data_size);
}

void NV_ShowMetas(void)
{
    NV_ShowPageMetas(mNvActivePageId, TRUE);
}

/*! *********************************************************************************
 *  \brief NV_ShowFlashTable iterate NV_ShowDataEntry calls until end_id found
 * Used for debug purposes only.
 *
 *  [IN] active_only dump only active page if true, both otherwise.
 *
 * \return none
 ********************************************************************************* */
void NV_ShowFlashTable(bool_t active_only)
{
    char                         message[128];
    NVM_VirtualPageID_t          page_id;
    NVM_VirtualPageProperties_t *vpage_prop;
    uint32_t                     address;
    for (page_id = gFirstVirtualPage_c; page_id <= gSecondVirtualPage_c; page_id++)
    {
        if (mNvActivePageId != page_id)
        {
            if (active_only)
                continue;
            else
            {
                PRINTF("Page%d\r\n", (int)page_id);
            }
        }
        else
        {
            PRINTF("Page%d active\r\n", (int)page_id);
        }
        vpage_prop = &mNvVirtualPageProperty[page_id];
        PRINTF("\r\n");
        int lg = 0;
        for (address = 0U; address < mNvTotalPageSize; address++)
        {
            if (address % 16U == 0U)
            {
                if (lg != 0)
                {
                    PRINTF(message);
                    lg = 0;
                }
                lg = sprintf(message, "\r\n[%08lx]: %02x", address,
                             *(uint8_t *)(address + vpage_prop->NvRawSectorStartAddress));
            }
            else
            {
                lg += sprintf(&message[lg], " %02X", *(uint8_t *)(address + vpage_prop->NvRawSectorStartAddress));
            }
        }
        PRINTF("\r\n\r\n");
    }
}

/*! *********************************************************************************
 *  \brief NV_ShowRamTable iterate NV_ShowDataEntry calls until end_id found
 * Used for debug purposes only.
 *
 *  [IN] end_id ID at which dump is stopped
 *
 * \return none
 ********************************************************************************* */
void NV_ShowRamTable(uint16_t end_id)
{
    uint8_t cnt;
    char    message[150];

    PRINTF("Ram table:\r\n");
    for (cnt = 0U; cnt < mNVM_DataTableNbEntries - 1U; cnt++)
    {
        NVM_DataEntry_t *pDataEntry = &pNVM_DataTable[cnt];
        if (pDataEntry->DataEntryID == end_id)
            break;

        PRINTF("Entry at index %d:\r\n", cnt);

        sprintf(message, "pData = 0x%08lx, EntriesCount = %04x, EntrySize = %04x, Id = %04x, Data type = %s\r\n",
                (uint32_t)pDataEntry->pData, pDataEntry->ElementsCount, pDataEntry->ElementSize,
                pDataEntry->DataEntryID,
                (pDataEntry->DataEntryType == (uint16_t)gNVM_MirroredInRam_c ? "mirrored" : "unmirrored"));
        PRINTF(message);

        if (pDataEntry->DataEntryType == (uint16_t)gNVM_MirroredInRam_c)
        {
            if (pDataEntry->pData)
            {
                NV_ShowDataEntry(pDataEntry->pData, pDataEntry->ElementsCount * pDataEntry->ElementSize);
            }
        }
        else
        {
            for (uint8_t cnt2 = 0U; cnt2 < pDataEntry->ElementsCount; cnt2++)
            {
                sprintf(message, "pData[%d] = 0x%.8lX\r\n", cnt2, (uint32_t)((uint8_t **)pDataEntry->pData)[cnt2]);
                PRINTF(message);
                if (((void **)pDataEntry->pData)[cnt2])
                {
                    NV_ShowDataEntry(((uint8_t **)pDataEntry->pData)[cnt2], pDataEntry->ElementSize);
                }
            }
        }
        PRINTF("\r\n");
    }
}

void dump_MIT(NVM_RecordMetaInfo_t *mit)
{
    PRINTF("metaInfo: VSB=%x VEB=%x\r\n", mit->u.fields.NvValidationStartByte, mit->u.fields.NvValidationEndByte);
    PRINTF("EntryID=%x\r\n", mit->u.fields.NvmDataEntryID);
    PRINTF("EltIdx=%x\r\n", mit->u.fields.NvmElementIndex);
    PRINTF("Offset=%x\r\n", mit->u.fields.NvmRecordOffset);
#if (defined gNvmMetaCheckSum_d && (gNvmMetaCheckSum_d != 0))
    PRINTF("chksum=%x\r\n", mit->NvmMetaChecksum);
#endif
}

#endif
/*! *********************************************************************************
 *  \brief Nv_GetLastMetaAddress returns address of the latest meta data.
 * Used for debug purposes only.
 *
 * \return address of meta
 ********************************************************************************* */
uint32_t Nv_GetLastMetaAddress(void)
{
    uint32_t addr = ~0UL;
#if gNvStorageIncluded_d
    if ((mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset != gNvInvalidMetaOffset_c) &&
        (mNvActivePageId != gVirtualPageNone_c))
    {
        addr = NV_PAGE_ADDR(mNvActivePageId, mNvVirtualPageProperty[mNvActivePageId].NvLastMetaInfoOffset);
    }
#endif
    return addr;
}

/*! *********************************************************************************
 *  \brief NV_MutexLock
 * Take NVM mutex.
 * Parameter(s): none
 *
 * \return none
 ********************************************************************************* */
void NV_MutexLock(void)
{
#if gNvStorageIncluded_d
    (void)OSA_MutexLock(mNVMMutexId, osaWaitForever_c);
#endif
}

/*! *********************************************************************************
 *  \brief NV_MutexUnlock
 * Release NVM mutex.
 * Parameter(s): none
 *
 * \return none
 ********************************************************************************* */
void NV_MutexUnlock(void)
{
#if gNvStorageIncluded_d
    (void)OSA_MutexUnlock(mNVMMutexId);
#endif
}

/*! *********************************************************************************
 *  Nv_GetPartitionAddressAndSize getter for NVM storage partition parameters.
 */
void Nv_GetPartitionAddressAndSize(uint32_t *partition_address, uint32_t *partition_size)
{
    /* Need to assume that arguments are not NULL pointers */
    assert(partition_address != NULL);
    assert(partition_size != NULL);

    /* By construction the MIT being coded on a uint16_t offset allows an addressing space of 64kB */
    const uint32_t max_page_sz = (uint32_t)UINT16_MAX + 1U;
    uint32_t       max_nb_sectors;
    uint32_t       nb_sectors      = 0U;
    uint32_t       flash_sector_sz = 0U;

    *partition_size    = 0UL;
    *partition_address = 0UL;

    int ret = 0;
    do
    {
        uint64_t check_sz;
        uint32_t start_addr;

        /* Read sector size from flash adapter */
        if (kStatus_HAL_Flash_Success != HAL_FlashGetProperty(kHAL_Flash_PropertyPflashSectorSize, &flash_sector_sz))
        {
            ret = -1;
            break;
        }
        /* Valid values for all known flash sector sizes are between 512B and 8kB .
         * Additionally the linker script must have defined NV_STORAGE_SECTOR_SIZE to match this value.
         */
        if ((flash_sector_sz > 8192U) || (flash_sector_sz < 512U) ||
            (flash_sector_sz != (uint32_t)(NV_STORAGE_SECTOR_SIZE)))
        {
            ret = -2;
            break;
        }
        /* The maximum possible number of sectors is limited by the 64kB addressable space
         * within the NVM virtual page..
         * The number of sectors is defined by the linker script, it must be an even number.
         */
        max_nb_sectors = (2U * max_page_sz) / flash_sector_sz;
        nb_sectors     = (uint32_t)(NV_STORAGE_MAX_SECTORS);
        nb_sectors &= (uint32_t)UINT16_MAX;

        if (((nb_sectors % 2U) != 0U) || (nb_sectors > max_nb_sectors))
        {
            ret = -3;
            break;
        }
        check_sz = (uint64_t)flash_sector_sz * (uint64_t)nb_sectors;

        if (check_sz > (uint64_t)max_page_sz * 2U)
        {
            ret = -4;
            break;
        }
        start_addr = (uint32_t)(NV_STORAGE_START_ADDRESS);
        if (start_addr >= ((uint32_t)UINT32_MAX - (uint32_t)check_sz))
        {
            assert(FALSE);
            break;
        }

        *partition_address = start_addr;
        *partition_size    = flash_sector_sz * nb_sectors;
    } while (false);
    /* Catch configuration errors in debug build only */
    assert(ret == 0);
    NOT_USED(ret);
}
