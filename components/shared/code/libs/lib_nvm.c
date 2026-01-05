/**
 * @file lib_nvm.c
 * @brief Source code for the NVM Manager
 *
 * Description
 * This library implements an embedded non-volatile memory controller. It currently
 * only supports embedded flash memory as the long-term storage medium. The library
 * provides mechanisms for updating NVM based on library version and entry version
 * through callback functions. The library indicates to the application if any (or
 * a specific) entryId is waiting to be written to NVM.
 * _Flash Backed NVM_
 * The library bisects the flash memory allocated into NVM as two blocks for
 * redundancy when migrating pages. These blocks are given a header and then are
 * followed to a dynamic array of entries. To reduce memory cycling, each instance
 * is individually written to memory when required. When used with an RTOS, the
 * application may request NVM writes from the NVM task.
 *
 * Setup
 * - Enable the feature tree library and define the size of the NVM flash blocks
 *   in addition to the supporting hardware
 * - Define the lib_nvm_entryId_E and lib_nvm_entry_S of each element
 * - Prior to any consumer running, call lib_nvm_init
 * _RTOS_
 *   - Instantiate a task that calls lib_nvm_dequeue. This function never returns
 *     and will block on its inbound message queue. This task shall be higher
 *     priority than all other tasks which may call lib_nvm_cleanUp
 *   - Instantiate a periodic call to lib_nvm_check at the application desired
 *     frequency
 *   - Instantiate a message queue with size equal to the number of NVM entries
 * _Bare Metal_
 *   - Call lib_nvm_check at the rate required by the application, non-volatile
 *     memory will block the active execution context
 *   - Call lib_nvm_requestWrite on a lib_nvm_entryId_E, this operation will block
 *
 * Usage
 * - The application may update its variables asynchronously, and the NVM driver
 *   should save the value to NVM once the minTimeBetweenWritesMs is elapsed. If
 *   the device is graciously powered off, any entry that differs from what is
 *   stored in NVM will be written to memory
 * - If the application requires that the value is updated immediately, it may call
 *   lib_nvm_requestWrite on the respective lib_nvm_entryId_E
 * - To graciously power down the NVM library, call lib_nvm_cleanUp from a task
 *   other than the task running lib_nvm_dequeue.
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_nvm.h"
#include <stdint.h>
#include "lib_utility.h"
#include "libcrc.h"
#include "string.h"
#include "FreeRTOS.h"
#include "semphr.h"

#if FEATURE_IS_DISABLED(NVM_LIB_ENABLED)
  #error "lib_nvm.c being compiled with nvm feature disabled"
#endif

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define LIB_NVM_VERSION 0U
#define NVM_COALESCE_PERIOD_MS 100U
#define NVM_COALESCE_CHECKIN_MS 10U

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED) && (FEATURE_IS_DISABLED(MCU_STM32_PN) == false)
#define SET_STATE (0x00U)
#else
#error "Only flash backed nvm supported presently."
#endif

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const lib_nvm_entry_S lib_nvm_entries[NVM_ENTRYID_COUNT];

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
extern storage_t __FLASH_NVM_ORIGIN;
extern storage_t __FLASH_NVM_END;
storage_t * const NVM_ORIGIN = &__FLASH_NVM_ORIGIN;
storage_t * const NVM_END = &__FLASH_NVM_END;
#endif

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef uint8_t lib_nvm_crc_t;

typedef struct
{
    lib_nvm_entry_t entryId;
    storage_t entry_version;
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    uint16_t entrySize;
#endif
    lib_nvm_crc_t crc;
    storage_t initialized;
    storage_t discarded;
} LIB_NVM_STORAGE(lib_nvm_recordHeader_S);
_Static_assert(sizeof(lib_nvm_recordHeader_S) % sizeof(storage_t) == 0, "Header must be word-aligned");

typedef struct
{
    storage_t* currentNvmAddr_Ptr;
    uint32_t lastWrittenTimeMs;
    bool writeRequired;
} lib_nvm_recordData_S;

typedef struct
{
#if FEATURE_IS_ENABLED(NVM_TASK)
    QueueHandle_t queue_handle;
    StaticQueue_t entry_action_queue;
    uint8_t entry_action_queue_buff[NVM_ENTRYID_COUNT * sizeof(lib_nvm_entryAction_S)];
#endif
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    uint32_t pageSize;
    storage_t* currentPtr;
#endif
} lib_nvm_data_S;

typedef struct
{
    uint16_t nvm_version;
    storage_t initialized;
    storage_t discarded;
} LIB_NVM_STORAGE(lib_nvm_blockHeader_S);

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static lib_nvm_recordData_S records[NVM_ENTRYID_COUNT] = {0U};
static lib_nvm_data_S data = {
};

LIB_NVM_MEMORY_REGION_ARRAY(lib_nvm_recordHeader_S recordHeaders, NVM_ENTRYID_COUNT) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_blockHeader_S blockHeader) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_nvmRecordLog_S recordLog) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_nvmCycleLog_S cycleLog) = { 0U };

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void seed_queue_from_flags(void);
static void initializeEmptyRecords(void);
static bool evaluateWriteRequired(lib_nvm_entry_t entryId);
static void recordPopulateDefault(lib_nvm_entry_t entryId);
static void recordWrite(lib_nvm_entry_t entryId);

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
static void invalidateBlock(lib_nvm_blockHeader_S * const block);
static void initializeNVMBlock(uint32_t addr);
static uint32_t getBlockBaseAddress(uint32_t addr);
static uint32_t getNextBlockStart(uint32_t addr);
static inline uint32_t align_up_bytes(uint32_t bytes);
#endif

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initialize the NVM library. This must be called prior to any NVM consumer
 */
void lib_nvm_init(void)
{
#if FEATURE_IS_ENABLED(NVM_TASK)
    data.queue_handle = xQueueCreateStatic(NVM_ENTRYID_COUNT,
                                           sizeof(lib_nvm_entryAction_S),
                                           data.entry_action_queue_buff,
                                           &data.entry_action_queue);
#endif

    lib_nvm_blockHeader_S* block_hdr = (lib_nvm_blockHeader_S*)NVM_ORIGIN;
    uint8_t total_failed_crc = 0U;
    uint8_t total_failed_init = 0U;
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    data.pageSize = LIB_NVM_GET_FLASH_PAGE_SIZE();

    // Iterate over every block until you find a block that hasnt been discarded
    while((block_hdr->initialized != (storage_t)SET_STATE) ||
          (block_hdr->discarded == (storage_t)SET_STATE))
    {
        block_hdr = (lib_nvm_blockHeader_S*)getNextBlockStart((uint32_t)block_hdr);
        if ((storage_t*)block_hdr == (NVM_END - (NVM_BLOCK_SIZE / sizeof(storage_t))))
        {
            break;
        }
    }

    // The first record is located immediately after the block header
    lib_nvm_recordHeader_S* hdr = (lib_nvm_recordHeader_S*)(block_hdr + 1);

    // If the found block is not valid, then initialize a block of default values
    if ((block_hdr->initialized != (storage_t)SET_STATE) ||
        (block_hdr->discarded == (storage_t)SET_STATE) ||
        (block_hdr->nvm_version != LIB_NVM_VERSION))
    {
        // TODO: Handle block versioning
        initializeNVMBlock((uint32_t)block_hdr);
        total_failed_init++;
    }
    else
    {
        // While the current record in NVM is initialized and we remain in the same block
        // bounds, continue iterating and updating the current record of each entry
        while ((hdr->initialized == SET_STATE) &&
            (getBlockBaseAddress((uint32_t)block_hdr) == getBlockBaseAddress((uint32_t)(hdr))))
        {
            storage_t* record = (storage_t*)(hdr + 1);

            if (hdr->discarded != SET_STATE)
            {
                lib_nvm_crc_t crc = 0xff;

                if (hdr->entryId >= NVM_ENTRYID_COUNT)
                {
                    // TODO : Handle failure
                    // initializeNVMBlock((uint32_t)data.currentPtr); // Immediately scrap the block and start with last elements?
                    total_failed_init++;
                    break;
                }

                crc = crc8_calculate(crc, (uint8_t*)record, hdr->entrySize);

                if (crc == hdr->crc)
                {
                    records[hdr->entryId].currentNvmAddr_Ptr = (storage_t*)hdr;
                }
                else
                {
                    storage_t disc = SET_STATE;
                    LIB_NVM_WRITE_TO_FLASH((uint32_t)&hdr->discarded,
                                        &disc,
                                        sizeof(storage_t));
                    total_failed_crc++;
                }
            }

            // Every NVM is comprised of a record header and the record data which is
            // has entrySize size
            hdr = (lib_nvm_recordHeader_S*)((uint32_t)hdr + align_up_bytes(hdr->entrySize));
            hdr += 1;
        }
    }

    data.currentPtr = (storage_t*)hdr;
#else
#error "No NVM implementation supported"
#endif

    uint8_t failed_records = 0U;

    for (uint8_t id = 0U; id < NVM_ENTRYID_COUNT; id++)
    {
        lib_nvm_recordData_S* const record = &records[id];
        hdr = (lib_nvm_recordHeader_S*)record->currentNvmAddr_Ptr;
        const lib_nvm_entry_S* const entry = &lib_nvm_entries[id];
        if (!hdr)
        {
            continue;
        }
        else if (entry->version != hdr->entry_version)
        {
            // If the version is different, handle the update
            // If the update fails, set the entry to the default value
            if (!entry->versionHandler_Fn ||
                !entry->versionHandler_Fn(
                    hdr->entry_version,
                    (storage_t*)(hdr + 1)))
            {
                record->currentNvmAddr_Ptr = NULL;
                failed_records++;
            }
            lib_nvm_requestWrite(id);
        }
        else
        {
            memcpy(entry->entryRam_Ptr,
                   (storage_t*)(hdr + 1),
                   entry->entrySize);
        }
    }
    initializeEmptyRecords();
    recordLog.totalRecordsVersionFailed += failed_records;
    recordLog.totalFailedRecordInit += total_failed_init;
    recordLog.totalFailedCrc += total_failed_crc;

    cycleLog.totalCycles++;
    lib_nvm_requestWrite(NVM_ENTRYID_CYCLE);
}

#if FEATURE_IS_ENABLED(NVM_TASK)
/**
 * @brief Operate on the NVM from a task. This function never returns
 */
void lib_nvm_run(void)
{
    seed_queue_from_flags();

    for (;;)
    {
        lib_nvm_entryAction_S action;
        const uint32_t drain_at = HW_TIM_getTimeMS() + NVM_COALESCE_PERIOD_MS;
        bool wrote_any = false;

        // Block until there is an element in the queue
        // If were not shutting down, wait for a record to be writable and
        // then coalesce writes for upto some amount of time
        xQueuePeek(data.queue_handle,
                    (void*)&action,
                    portMAX_DELAY);
        while ((HW_TIM_getTimeMS() < drain_at) && !HW_mcuShuttingDown())
        {
            vTaskDelay(pdMS_TO_TICKS(NVM_COALESCE_CHECKIN_MS));
        }

        // Loop through queued actions for as long as there is one
        // immediately available
        while(xQueueReceive(data.queue_handle,
                            &action,
                            0))
        {
            switch (action.action)
            {
                case NVM_ACTION_WRITE:
                    recordWrite(action.entryId);
                    if (action.entryId != NVM_ENTRYID_LOG) {
                        wrote_any = true;
                    }
                    break;
                default:
                    break;
            }
        }

        if (wrote_any)
        {
            recordWrite(NVM_ENTRYID_LOG);
        }

        if (uxQueueMessagesWaiting(data.queue_handle) == 0) {
            seed_queue_from_flags();
        }
    }
}
#endif

/**
 * @brief Iniatialize a new block of memory from the default values
 */
bool lib_nvm_nvmInitializeNewBlock(void)
{
    for (lib_nvm_entry_t entry = 0U; entry < NVM_ENTRYID_COUNT; entry++)
    {
        records[entry].currentNvmAddr_Ptr = NULL;
    }

    lib_nvm_blockHeader_S* currentBlock = (lib_nvm_blockHeader_S*)getBlockBaseAddress((uint32_t)data.currentPtr);
    initializeNVMBlock(getNextBlockStart((uint32_t)data.currentPtr));
    invalidateBlock(currentBlock);

    return true;
}

/**
 * @brief Request a write on an NVM entry. This call may block in an RT
 *        environment if the queue is full
 * @param entryId The entryId to save to NVM
 */
bool lib_nvm_requestWrite(lib_nvm_entryId_E entryId)
{
    if (records[entryId].writeRequired)
    {
        return true;
    }

    records[entryId].writeRequired = true;
#if FEATURE_IS_ENABLED(NVM_TASK)
    lib_nvm_entryAction_S action = {
        .entryId = entryId,
        .action = NVM_ACTION_WRITE,
    };

    // Let the NVM task handle the entry write due to blocking flash
    // operations. Queue a write operation on the provided entryId
    // If the queue is full, block until it is available (this condition
    // should never occur)
    const bool enqueue = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    if (enqueue && (xQueueSend(data.queue_handle,
                               (void*)&action,
                               0) == errQUEUE_FULL))
    {
        return false;
    }
#endif

#if FEATURE_IS_DISABLED(NVM_TASK)
    recordWrite(entryId);
#endif

    return true;
}

uint32_t lib_nvm_getTotalRecordWrites(void)
{
    return recordLog.totalRecordWrites;
}

uint32_t lib_nvm_getTotalFailedCrc(void)
{
    return recordLog.totalFailedCrc;
}

uint32_t lib_nvm_getTotalBlockErases(void)
{
    return recordLog.totalBlockClears;
}

uint32_t lib_nvm_getTotalFailedRecordInit(void)
{
    return recordLog.totalFailedRecordInit;
}

uint32_t lib_nvm_getTotalEmptyRecordInit(void)
{
    return recordLog.totalEmptyRecordInit;
}

uint32_t lib_nvm_getTotalRecordsVersionFailed(void)
{
    return recordLog.totalRecordsVersionFailed;
}

uint32_t lib_nvm_getTotalCycles(void)
{
    return cycleLog.totalCycles;
}

/**
 * @brief Write all NVM entries to storage. Only return once all entries
 *        have completed writing. This function may only be called from
 *        tasks with lower priority than the NVM task
 */
void lib_nvm_cleanUp(void)
{
    for (lib_nvm_entry_t index = 0U; index < NVM_ENTRYID_COUNT; index++)
    {
        if ((index != NVM_ENTRYID_LOG) && evaluateWriteRequired(index))
        {
            lib_nvm_requestWrite(index);
        }
    }

    // There may be upstream failures - if there are wait until the queue is
    // empty and double check if any still need to be written
    const bool scheduler_running =  xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    while(scheduler_running && uxQueueMessagesWaiting(data.queue_handle))
    {
        vTaskDelay(1);
    }

    // Force write any outstanding items
    for (lib_nvm_entry_t index = 0U; index < NVM_ENTRYID_COUNT; index++)
    {
        recordWrite(index);
    }

    // This loop will block until all NVM entries are written to storage.
    // If the calling task has higher priority than the NVM task, this
    // operation will cause a deadlock
    while(lib_nvm_writesRequired());
}

/**
 * @brief Check if any entry requires a write to storage
 * @return true if any entry needs to be written
 */
bool lib_nvm_writesRequired(void)
{
    for (lib_nvm_entry_t entry = 0U; entry < NVM_ENTRYID_COUNT; entry++)
    {
        if (lib_nvm_writeRequired(entry))
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Check if a given entry requires a write to storage
 * @return true if the entry needs to be written
 */
bool lib_nvm_writeRequired(lib_nvm_entry_t entryId)
{
    return records[entryId].writeRequired;
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(NVM_TASK)
static void seed_queue_from_flags(void)
{
    for (lib_nvm_entry_t i = 0; i < NVM_ENTRYID_COUNT; i++) {
        if (records[i].writeRequired) {
            lib_nvm_entryAction_S a = { .entryId = i, .action = NVM_ACTION_WRITE };
            (void)xQueueSend(data.queue_handle, &a, 0);
        }
    }
}
#endif

static void recordPopulateDefault(lib_nvm_entry_t entryId)
{
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    memcpy((uint32_t*)lib_nvm_entries[entryId].entryRam_Ptr, lib_nvm_entries[entryId].entryDefault_Ptr, lib_nvm_entries[entryId].entrySize);
#endif
}

static void initializeEmptyRecords(void)
{
    for (lib_nvm_entry_t entry = 0U; entry < NVM_ENTRYID_COUNT; entry++)
    {
        if (records[entry].currentNvmAddr_Ptr == NULL)
        {
            recordPopulateDefault(entry);
            recordLog.totalEmptyRecordInit++;
            lib_nvm_requestWrite(entry);
        }
    }
}

static bool evaluateWriteRequired(lib_nvm_entry_t entryId)
{
    if (records[entryId].writeRequired)
    {
        return true;
    }

    lib_nvm_recordHeader_S* hdr = (lib_nvm_recordHeader_S*)records[entryId].currentNvmAddr_Ptr;
    uint32_t ms = LIB_NVM_GET_TIME_MS();

    // Only update a value if any of the following is true
    // 1. The current record address is not in the current NVM page
    // 2. The record and entry are different, and the minTimeBetweenWritesMs has elapsed
    // 3. There is no current NVM record for the entry
    if ((records[entryId].currentNvmAddr_Ptr == NULL) ||
        (getBlockBaseAddress((uint32_t)records[entryId].currentNvmAddr_Ptr) != getBlockBaseAddress((uint32_t)data.currentPtr)) ||
        ((((ms - records[entryId].lastWrittenTimeMs) >= (lib_nvm_entries[entryId].minTimeBetweenWritesMs)) || (HW_mcuShuttingDown())) &&
         (memcmp((storage_t*)lib_nvm_entries[entryId].entryRam_Ptr, (storage_t*)(hdr + 1), lib_nvm_entries[entryId].entrySize))))
    {
        records[entryId].writeRequired = true;
    }

    return records[entryId].writeRequired;
}

static void recordWrite(const lib_nvm_entry_t entryId)
{
    const uint32_t data_bytes = align_up_bytes(lib_nvm_entries[entryId].entrySize);

    for (;;) {
        // Validate that the record write is not egregious
        lib_nvm_recordHeader_S recordHeader = { 0U };
        lib_nvm_recordHeader_S * const currentRecord = (lib_nvm_recordHeader_S*)records[entryId].currentNvmAddr_Ptr;
        uint16_t entrySize = lib_nvm_entries[entryId].entrySize;

#if FEATURE_IS_ENABLED(NVM_TASK)
        taskENTER_CRITICAL();
#endif
        // Obtain the current pointer into NVM with a space for this entry
        uint32_t current_hdr = (uint32_t)data.currentPtr;
        uint32_t current_record = (uint32_t)((lib_nvm_recordHeader_S*)data.currentPtr + 1);
        lib_nvm_blockHeader_S * const block_header = (lib_nvm_blockHeader_S * const)getBlockBaseAddress(current_hdr);
        data.currentPtr += sizeof(lib_nvm_recordHeader_S) / sizeof(storage_t);
        data.currentPtr += data_bytes / sizeof(storage_t);
#if FEATURE_IS_ENABLED(NVM_TASK)
        taskEXIT_CRITICAL();
#endif
        if ((uint32_t)block_header != getBlockBaseAddress((uint32_t)data.currentPtr))
        {
            initializeNVMBlock(getNextBlockStart((uint32_t)current_hdr));
            invalidateBlock(block_header);
            continue;
        }


        // Create and write the entry to NVM
        records[entryId].currentNvmAddr_Ptr = (storage_t*)current_hdr;
        recordHeader.entryId = entryId;
        recordHeader.entrySize = entrySize;
        recordHeader.initialized = SET_STATE;
        recordHeader.discarded = (storage_t)~SET_STATE;

        recordLog.totalRecordWrites++;

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
        LIB_NVM_WRITE_TO_FLASH(current_record,
                            lib_nvm_entries[entryId].entryRam_Ptr,
                            lib_nvm_entries[entryId].entrySize);
        recordHeader.entry_version = lib_nvm_entries[entryId].version;
        recordHeader.crc = (storage_t)crc8_calculate(0xff,
                                                    (uint8_t*)current_record,
                                                    lib_nvm_entries[entryId].entrySize);
        LIB_NVM_WRITE_TO_FLASH(current_hdr,
                            (storage_t*)&recordHeader,
                            sizeof(lib_nvm_recordHeader_S));
#endif // NVM_FLASH_BACKED

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
        // Once the record has been written to NVM, set the record as valid in flash
        if (currentRecord != NULL)
        {
            storage_t disc = SET_STATE;
            LIB_NVM_WRITE_TO_FLASH((uint32_t)&currentRecord->discarded,
                                &disc,
                                sizeof(storage_t));
        }
#endif // NVM_FLASH_BACKED
        records[entryId].writeRequired = false;
        records[entryId].lastWrittenTimeMs = LIB_NVM_GET_TIME_MS();
        return;
    }
}

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
static void invalidateBlock(lib_nvm_blockHeader_S * const block)
{
    storage_t disc = SET_STATE;
    lib_nvm_blockHeader_S* header = (lib_nvm_blockHeader_S*)getBlockBaseAddress((uint32_t)block);
    LIB_NVM_WRITE_TO_FLASH((uint32_t)&header->discarded, &disc, sizeof(header->discarded));
}

static void initializeNVMBlock(uint32_t addr)
{
    lib_nvm_blockHeader_S blockHeader_tmp = { 0U };
    blockHeader_tmp.discarded  = (storage_t)~SET_STATE;
    blockHeader_tmp.initialized = SET_STATE;
    blockHeader_tmp.nvm_version = LIB_NVM_VERSION;

#if FEATURE_IS_ENABLED(NVM_TASK)
    taskENTER_CRITICAL();
#endif
    uint32_t page_base = getBlockBaseAddress(addr);
    data.currentPtr = (storage_t*)((lib_nvm_blockHeader_S*)page_base + 1);
#if FEATURE_IS_ENABLED(NVM_TASK)
    taskEXIT_CRITICAL();
#endif
    // In a new block, clear all the relevant flash pages and intialize the block header
    LIB_NVM_CLEAR_FLASH_PAGES(page_base, (uint16_t)(NVM_BLOCK_SIZE / data.pageSize));

    for (lib_nvm_entry_t index = 0U; index < NVM_ENTRYID_COUNT; index++)
    {
        records[index].writeRequired = true;
        if (HW_mcuShuttingDown())
        {
            recordWrite(index);
        }
    }

    LIB_NVM_WRITE_TO_FLASH(page_base,
                           (storage_t*)&blockHeader_tmp,
                           sizeof(lib_nvm_blockHeader_S));

    recordLog.totalBlockClears++;
}

static uint32_t getBlockBaseAddress(uint32_t addr)
{
    return (uint32_t)NVM_ORIGIN + (((addr - (uint32_t)NVM_ORIGIN) / NVM_BLOCK_SIZE) * NVM_BLOCK_SIZE);
}

static uint32_t getNextBlockStart(uint32_t addr)
{
    return ((getBlockBaseAddress(addr + NVM_BLOCK_SIZE) < (uint32_t)NVM_END) ?
            (uint32_t)((addr + NVM_BLOCK_SIZE) - (addr % NVM_BLOCK_SIZE)) :
            (uint32_t)NVM_ORIGIN);
}

static inline uint32_t align_up_bytes(uint32_t bytes)
{
    const uint32_t w = sizeof(storage_t);
    return (bytes + (w - 1U)) & ~(w - 1U);
}
#endif
