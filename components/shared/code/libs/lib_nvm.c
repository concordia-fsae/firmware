/** lib_nvm.c
 *     Source code for the NVM Manager
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

#if FEATURE_IS_DISABLED(NVM_LIB_ENABLED)
  #error "lib_nvm.c being compiled with nvm feature disabled"
#endif

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define LIB_NVM_VERSION 0U

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

typedef struct
{
    storage_t* currentNvmAddr_Ptr;
    uint32_t lastWrittenTimeMs;
    bool writeRequired;
} lib_nvm_recordData_S;

typedef struct
{
    bool mcuShuttingDown;
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
    .mcuShuttingDown = false,
};

LIB_NVM_MEMORY_REGION_ARRAY(lib_nvm_recordHeader_S recordHeaders, NVM_ENTRYID_COUNT) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_blockHeader_S blockHeader) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_nvmRecordLog_S recordLog) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_nvmCycleLog_S cycleLog) = { 0U };

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void initializeEmptyRecords(void);
static void evaluateWriteRequired(lib_nvm_entry_t entryId);
static void recordPopulateDefault(lib_nvm_entry_t entryId);
static void recordWrite(lib_nvm_entry_t entryId);

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
static void invalidateBlock(lib_nvm_blockHeader_S * const block);
static void initializeNVMBlock(uint32_t addr);
static uint32_t getBlockBaseAddress(uint32_t addr);
static uint32_t getNextBlockStart(uint32_t addr);
#endif

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void lib_nvm_init(void)
{
    lib_nvm_blockHeader_S* block_hdr = (lib_nvm_blockHeader_S*)NVM_ORIGIN;
    uint8_t total_failed_crc = 0U;
    uint8_t total_failed_init = 0U;
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    data.pageSize = LIB_NVM_GET_FLASH_PAGE_SIZE();

    while((block_hdr->initialized != (storage_t)SET_STATE) ||
          (block_hdr->discarded == (storage_t)SET_STATE))
    {
        block_hdr = (lib_nvm_blockHeader_S*)getNextBlockStart((uint32_t)block_hdr);
        if ((storage_t*)block_hdr == (NVM_END - (NVM_BLOCK_SIZE / sizeof(storage_t))))
        {
            break;
        }
    }

    lib_nvm_recordHeader_S* hdr = (lib_nvm_recordHeader_S*)(block_hdr + 1);

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

            hdr = (lib_nvm_recordHeader_S*)((uint32_t)hdr + hdr->entrySize);
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

void lib_nvm_run(void)
{
    for (lib_nvm_entry_t index = 0U; index < NVM_ENTRYID_COUNT; index++)
    {
        if (index == NVM_ENTRYID_LOG)
        {
            continue;
        }

        evaluateWriteRequired(index);
        if (records[index].writeRequired)
        {
            recordWrite(index);
        }
    }

    evaluateWriteRequired(NVM_ENTRYID_LOG);
    if (records[NVM_ENTRYID_LOG].writeRequired)
    {
        recordWrite(NVM_ENTRYID_LOG);
    }
}

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

void lib_nvm_requestWrite(lib_nvm_entryId_E entryId)
{
    records[entryId].writeRequired = true;;
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

void lib_nvm_cleanUp(void)
{
    portENTER_CRITICAL();
    data.mcuShuttingDown = true;

    for (lib_nvm_entry_t entry = 0U; entry < NVM_ENTRYID_COUNT; entry++)
    {
        if ((records[entry].currentNvmAddr_Ptr == NULL) ||
            (getBlockBaseAddress((uint32_t)records[entry].currentNvmAddr_Ptr) != getBlockBaseAddress((uint32_t)data.currentPtr)) ||
            (records[entry].writeRequired))
        {
            recordWrite(entry);
        }
    }
    portEXIT_CRITICAL();
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

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

static void evaluateWriteRequired(lib_nvm_entry_t entryId)
{
    if (records[entryId].writeRequired)
    {
        return;
    }

    lib_nvm_recordHeader_S* hdr = (lib_nvm_recordHeader_S*)records[entryId].currentNvmAddr_Ptr;
    uint32_t ms = LIB_NVM_GET_TIME_MS();

    if ((records[entryId].currentNvmAddr_Ptr == NULL) ||
        (getBlockBaseAddress((uint32_t)records[entryId].currentNvmAddr_Ptr) != getBlockBaseAddress((uint32_t)data.currentPtr)) ||
        ((ms < (records[entryId].lastWrittenTimeMs + lib_nvm_entries[entryId].minTimeBetweenWritesMs)) &&
         (memcmp((storage_t*)lib_nvm_entries[entryId].entryRam_Ptr, (storage_t*)(hdr + 1), lib_nvm_entries[entryId].entrySize))))
    {
        records[entryId].writeRequired = true;
    }

}

static void recordWrite(const lib_nvm_entry_t entryId)
{
    if ((lib_nvm_entries[entryId].minTimeBetweenWritesMs != 0U) &&
        (records[entryId].lastWrittenTimeMs != 0U) &&
        (LIB_NVM_GET_TIME_MS() > (records[entryId].lastWrittenTimeMs + lib_nvm_entries[entryId].minTimeBetweenWritesMs)) &&
        (data.mcuShuttingDown == false))
    {
        return;
    }

    lib_nvm_recordHeader_S recordHeader = { 0U };

    lib_nvm_recordHeader_S * const currentRecord = (lib_nvm_recordHeader_S*)records[entryId].currentNvmAddr_Ptr;
    lib_nvm_blockHeader_S * const blockHeaderCurrent = (lib_nvm_blockHeader_S * const)getBlockBaseAddress((uint32_t)data.currentPtr);
    uint16_t entrySize = lib_nvm_entries[entryId].entrySize;

    if (blockHeaderCurrent != (lib_nvm_blockHeader_S * const)getBlockBaseAddress((uint32_t)data.currentPtr + sizeof(lib_nvm_recordHeader_S) + entrySize))
    {
        initializeNVMBlock(getNextBlockStart((uint32_t)data.currentPtr));
        invalidateBlock(blockHeaderCurrent);
        return;
    }

    records[entryId].currentNvmAddr_Ptr = (storage_t*)data.currentPtr;
    recordHeader.entryId = entryId;
    recordHeader.entrySize = entrySize;
    recordHeader.initialized = SET_STATE;
    recordHeader.discarded = (storage_t)~SET_STATE;
    data.currentPtr += sizeof(lib_nvm_recordHeader_S) / sizeof(storage_t);

    recordLog.totalRecordWrites++;

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    LIB_NVM_WRITE_TO_FLASH((uint32_t)data.currentPtr,
                            lib_nvm_entries[entryId].entryRam_Ptr,
                            lib_nvm_entries[entryId].entrySize);
    recordHeader.entry_version = lib_nvm_entries[entryId].version;
    recordHeader.crc = (storage_t)crc8_calculate(0xff,
                                                    (uint8_t*)data.currentPtr,
                                                    lib_nvm_entries[entryId].entrySize);
    LIB_NVM_WRITE_TO_FLASH((uint32_t)records[entryId].currentNvmAddr_Ptr,
                           (storage_t*)&recordHeader,
                           sizeof(lib_nvm_recordHeader_S));
    data.currentPtr += lib_nvm_entries[entryId].entrySize / sizeof(storage_t);
#endif // NVM_FLASH_BACKED

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
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

    data.currentPtr = (storage_t*)getBlockBaseAddress(addr);
    LIB_NVM_CLEAR_FLASH_PAGES((uint32_t)data.currentPtr, (uint16_t)(NVM_BLOCK_SIZE / data.pageSize));

    data.currentPtr = (storage_t*)((lib_nvm_blockHeader_S*)data.currentPtr + 1);

    for (lib_nvm_entry_t index = 0U; index < NVM_ENTRYID_COUNT; index++)
    {
        records[index].writeRequired = true;
    }

    LIB_NVM_WRITE_TO_FLASH(getBlockBaseAddress((uint32_t)data.currentPtr),
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
#endif
