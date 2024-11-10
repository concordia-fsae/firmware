/** LIB_nvm.c
 *     Source code for the NVM Manager
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_nvm.h"
#include <stdint.h>
#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
#include "LIB_utility.h"
#include "libcrc.h"
#include "string.h"
#include "FreeRTOS.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define LIB_NVM_VERSION 1U
#define NVM_TOTAL_ENTRYID_COUNT (NVM_ENTRYID_INTERNAL_COUNT + COUNTOF(lib_nvm_entries))

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
storage_t * const nvm_origin = &__FLASH_NVM_ORIGIN;
storage_t * const nvm_end = &__FLASH_NVM_END;
#endif

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef uint8_t lib_nvm_crc_t;

typedef enum
{
    NVM_ENTRYID_INTERNAL_LOG = 0U,
    NVM_ENTRYID_INTERNAL_CYCLE, // Anytime an entry is added, a spare should be removed
    NVM_ENTRYID_INTERNAL_SPARE_1,
    NVM_ENTRYID_INTERNAL_SPARE_2,
    NVM_ENTRYID_INTERNAL_SPARE_3,
    NVM_ENTRYID_INTERNAL_COUNT,
} lib_nvm_entryIdInternal_E;
#define NVM_LAST_USED_INTERNAL_ENTRYID NVM_ENTRYID_INTERNAL_CYCLE

typedef struct
{
    lib_nvm_entry_t entryId;
    storage_t entry_version;
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    uint8_t entrySize;
#endif
    lib_nvm_crc_t crc;
    storage_t initialized;
    storage_t discarded;
} LIB_NVM_STORAGE(lib_nvm_recordHeader_S);

typedef struct
{
    storage_t* currentNonVolatileAddress_Ptr;
    uint32_t lastWrittenTimeMs;
    uint8_t observedDifferences;
    bool writeRequired;
} lib_nvm_recordData_S;

typedef struct
{
    uint32_t totalRecordWrites;
    uint32_t totalFailedCrc;
    uint32_t totalBlockClears;
    uint32_t spare[4];
} LIB_NVM_STORAGE(lib_nvm_nvmRecordLog_S);

typedef struct
{
    uint32_t totalCycles;
} LIB_NVM_STORAGE(lib_nvm_nvmCycleLog_S);

typedef struct
{
    bool hardResetCompleted;
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

static lib_nvm_recordData_S records[NVM_TOTAL_ENTRYID_COUNT] = {0U};
static lib_nvm_data_S data = { 
    .hardResetCompleted = false,
    .mcuShuttingDown = false,
};

static const lib_nvm_nvmRecordLog_S recordLogDefault = {
    .totalRecordWrites = 0U,
    .totalFailedCrc = 0U,
    .totalBlockClears = 0U,
    .spare = { 0U },
};

static const lib_nvm_nvmCycleLog_S cycleLogDefault = {
    .totalCycles = 0U,
};

LIB_NVM_MEMORY_REGION_ARRAY(lib_nvm_recordHeader_S recordHeaders, NVM_TOTAL_ENTRYID_COUNT) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_blockHeader_S blockHeader) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_nvmRecordLog_S recordLog) = { 0U };
LIB_NVM_MEMORY_REGION(lib_nvm_nvmCycleLog_S cycleLog) = { 0U };

static const lib_nvm_entry_S lib_nvm_private_entries[] = {
    [NVM_ENTRYID_INTERNAL_LOG] = {
        .entryId = NVM_ENTRYID_INTERNAL_LOG,
        .entrySize = sizeof(lib_nvm_nvmRecordLog_S),
        .entryDefault_Ptr = &recordLogDefault,
        .entryRam_Ptr = &recordLog,
        .minTimeBetweenWritesMs = 10000U,
    },
    [NVM_ENTRYID_INTERNAL_CYCLE] = {
        .entryId = NVM_ENTRYID_INTERNAL_LOG,
        .entrySize = sizeof(lib_nvm_nvmCycleLog_S),
        .entryDefault_Ptr = &cycleLogDefault,
        .entryRam_Ptr = &cycleLog,
        .minTimeBetweenWritesMs = 1000U, // Should only change once per boot cycle
    },
};
_Static_assert(COUNTOF(lib_nvm_private_entries) <= NVM_ENTRYID_INTERNAL_COUNT, "Too many private NVM entries.");

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void initializeEmptyRecords(void);
static void evaluateWriteRequiredPrivate(lib_nvm_entry_t entryId);
static void requestWritePrivate(lib_nvm_entry_t entryId);
static void recordWrite(lib_nvm_entry_t entryId);
static void recordPopulateDefault(lib_nvm_entry_t entryId);

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
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    data.currentPtr = nvm_origin;
    data.pageSize = LIB_NVM_GET_FLASH_PAGE_SIZE();

    while((((lib_nvm_blockHeader_S*)data.currentPtr)->initialized != (storage_t)SET_STATE) ||
          (((lib_nvm_blockHeader_S*)data.currentPtr)->discarded == (storage_t)SET_STATE))
    {
        data.currentPtr = (storage_t*)getNextBlockStart((uint32_t)data.currentPtr);
        if (data.currentPtr == (nvm_end - NVM_BLOCK_SIZE / sizeof(storage_t)))
        {
            break;
        }
    }

    if ((((lib_nvm_blockHeader_S*)data.currentPtr)->initialized != (storage_t)SET_STATE) ||
        (((lib_nvm_blockHeader_S*)data.currentPtr)->discarded == (storage_t)SET_STATE))
    {
        initializeNVMBlock((uint32_t)data.currentPtr);
    }
    else
    {
        data.currentPtr += sizeof(lib_nvm_blockHeader_S) / sizeof(storage_t);

        while ((((lib_nvm_recordHeader_S*)data.currentPtr)->initialized == SET_STATE) &&
            (getBlockBaseAddress((uint32_t)data.currentPtr) == getBlockBaseAddress((uint32_t)data.currentPtr + sizeof(lib_nvm_recordHeader_S))))
        {
            storage_t* record = data.currentPtr + sizeof(lib_nvm_recordHeader_S) / sizeof(storage_t);
            lib_nvm_entry_t id = (((lib_nvm_recordHeader_S*)data.currentPtr)->entryId <= NVM_LAST_USED_INTERNAL_ENTRYID) ?
                                ((lib_nvm_recordHeader_S*)data.currentPtr)->entryId :
                                ((lib_nvm_recordHeader_S*)data.currentPtr)->entryId - NVM_ENTRYID_INTERNAL_COUNT;
            if (((lib_nvm_recordHeader_S*)data.currentPtr)->discarded != SET_STATE)
            {
                lib_nvm_crc_t crc = 0xff;

                if (((((lib_nvm_recordHeader_S*)data.currentPtr)->entryId > NVM_LAST_USED_INTERNAL_ENTRYID) &&
                    (((lib_nvm_recordHeader_S*)data.currentPtr)->entryId < NVM_ENTRYID_INTERNAL_COUNT)) ||
                    (((lib_nvm_recordHeader_S*)data.currentPtr)->entryId > NVM_TOTAL_ENTRYID_COUNT))
                {
                    // TODO : Handle failure
                    // initializeNVMBlock((uint32_t)data.currentPtr); // Immediately scrap the block and start with last elements?
                    break;
                }

                crc = crc8_calculate(crc, (uint8_t*)record, ((lib_nvm_recordHeader_S*)data.currentPtr)->entrySize);

                if (crc == ((lib_nvm_recordHeader_S*)data.currentPtr)->crc)
                {
                    records[((lib_nvm_recordHeader_S*)data.currentPtr)->entryId].currentNonVolatileAddress_Ptr = (storage_t*)data.currentPtr;
                    if (((lib_nvm_recordHeader_S*)data.currentPtr)->entryId <= NVM_LAST_USED_INTERNAL_ENTRYID)
                    {
                        memcpy(lib_nvm_private_entries[id].entryRam_Ptr, (uint8_t*)record, lib_nvm_private_entries[id].entrySize);
                    }
                    else if (((lib_nvm_recordHeader_S*)data.currentPtr)->entryId >= NVM_ENTRYID_INTERNAL_COUNT)
                    {
                        memcpy(lib_nvm_entries[id].entryRam_Ptr, (uint8_t*)record, lib_nvm_entries[id].entrySize);
                    }
                }
                else
                {
                    storage_t disc = SET_STATE;
                    LIB_NVM_WRITE_TO_FLASH((uint32_t)&(((lib_nvm_recordHeader_S*)data.currentPtr)->discarded),
                                        &disc,
                                        sizeof(storage_t));
                    recordLog.totalFailedCrc++;
                }
            }

            data.currentPtr += ((lib_nvm_recordHeader_S*)data.currentPtr)->entrySize / sizeof(storage_t);
            data.currentPtr += sizeof(lib_nvm_recordHeader_S) / sizeof(storage_t);
        }

        initializeEmptyRecords();
    }
#else
#error "No NVM implementation supported"
#endif

    cycleLog.totalCycles++;
}

void lib_nvm_run(void)
{
    for (lib_nvm_entry_t index = NVM_LAST_USED_INTERNAL_ENTRYID; index <= NVM_LAST_USED_INTERNAL_ENTRYID; index--)
    {
        evaluateWriteRequiredPrivate(index);
        if (records[index].writeRequired)
        {
            recordWrite(index);
        }
    }

    for (uint8_t index = NVM_ENTRYID_INTERNAL_COUNT; index < (uint8_t)NVM_TOTAL_ENTRYID_COUNT; index++)
    {
        if (records[index].writeRequired)
        {
            recordWrite(index);
        }
    }
}

bool lib_nvm_nvmHardReset(void)
{
    bool ret = false;

    for (lib_nvm_entry_t entry = 0U; entry < NVM_TOTAL_ENTRYID_COUNT; entry++)
    {
        records[entry].currentNonVolatileAddress_Ptr = NULL;
    }

    lib_nvm_blockHeader_S* currentBlock = (lib_nvm_blockHeader_S*)getBlockBaseAddress((uint32_t)data.currentPtr);
    initializeNVMBlock(getNextBlockStart((uint32_t)data.currentPtr));
    invalidateBlock(currentBlock);
    data.hardResetCompleted = true;

    return ret;
}

bool lib_nvm_nvmHardResetGetStatus(void)
{
    bool ret = data.hardResetCompleted;
    data.hardResetCompleted = false;
    return ret;
}

void lib_nvm_requestWrite(lib_nvm_entryId_E entryId)
{
    requestWritePrivate(entryId + NVM_ENTRYID_INTERNAL_COUNT);
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

uint32_t lib_nvm_getTotalCycles(void)
{
    return cycleLog.totalCycles;
}

void lib_nvm_cleanUp(void)
{
    portENTER_CRITICAL();
    data.mcuShuttingDown = true;

    for (lib_nvm_entry_t entry = 0U; entry <= NVM_LAST_USED_INTERNAL_ENTRYID; entry++)
    {
        if ((records[entry].currentNonVolatileAddress_Ptr == NULL) ||
            (getBlockBaseAddress((uint32_t)records[entry].currentNonVolatileAddress_Ptr) != getBlockBaseAddress((uint32_t)data.currentPtr)) ||
            (records[entry].writeRequired))
        {
            recordWrite(entry);
        }
    }

    for (lib_nvm_entry_t entry = 0U; entry < NVM_ENTRYID_COUNT; entry++)
    {
        if ((records[entry + NVM_ENTRYID_INTERNAL_COUNT].currentNonVolatileAddress_Ptr == NULL) ||
            (getBlockBaseAddress((uint32_t)records[entry].currentNonVolatileAddress_Ptr) != getBlockBaseAddress((uint32_t)data.currentPtr)) ||
            (records[entry].writeRequired))
        {
            recordWrite(entry + NVM_ENTRYID_INTERNAL_COUNT);
        }
    }
    portEXIT_CRITICAL();
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void initializeEmptyRecords(void)
{
    for (lib_nvm_entry_t entry = 0U; entry <= NVM_LAST_USED_INTERNAL_ENTRYID; entry++)
    {
        if (records[entry].currentNonVolatileAddress_Ptr == NULL)
        {
            recordPopulateDefault(entry);
        }

        requestWritePrivate(entry);
    }

    for (lib_nvm_entry_t entry = 0U; entry < NVM_ENTRYID_COUNT; entry++)
    {
        if (records[entry + NVM_ENTRYID_INTERNAL_COUNT].currentNonVolatileAddress_Ptr == NULL)
        {
            recordPopulateDefault(entry + NVM_ENTRYID_INTERNAL_COUNT);
        }

        lib_nvm_requestWrite(entry);
    }
}

static void evaluateWriteRequiredPrivate(lib_nvm_entry_t entryId)
{
    switch (entryId)
    {
        case NVM_ENTRYID_INTERNAL_LOG:
        {
            if (records[NVM_ENTRYID_INTERNAL_LOG].currentNonVolatileAddress_Ptr == NULL)
            {
                records[NVM_ENTRYID_INTERNAL_LOG].writeRequired = true;
                break;
            }

            if (memcmp((storage_t*)&recordLog, records[NVM_ENTRYID_INTERNAL_LOG].currentNonVolatileAddress_Ptr + sizeof(lib_nvm_recordHeader_S) / sizeof(storage_t), sizeof(lib_nvm_nvmRecordLog_S)))
            {
                records[NVM_ENTRYID_INTERNAL_LOG].writeRequired = true;
            }
        }
            break;

        default:
            break;
    }
}

static void requestWritePrivate(lib_nvm_entry_t entryId)
{
    records[entryId].writeRequired = true;
}

static void recordWrite(lib_nvm_entry_t entryId)
{
    if (entryId <= NVM_LAST_USED_INTERNAL_ENTRYID)
    {
        if ((lib_nvm_private_entries[entryId].minTimeBetweenWritesMs != 0U) &&
            (records[entryId].lastWrittenTimeMs != 0U) &&
            (LIB_NVM_GET_TIME_MS() < (records[entryId].lastWrittenTimeMs + lib_nvm_private_entries[entryId].minTimeBetweenWritesMs)) &&
            (data.mcuShuttingDown == false))
        {
            return;
        }
    }
    else if (entryId >= NVM_ENTRYID_INTERNAL_COUNT)
    {
        uint8_t relative_entry = entryId - NVM_ENTRYID_INTERNAL_COUNT;
        if ((lib_nvm_entries[relative_entry].minTimeBetweenWritesMs != 0U) &&
            (records[entryId].lastWrittenTimeMs != 0U) &&
            (LIB_NVM_GET_TIME_MS() < (records[entryId].lastWrittenTimeMs + lib_nvm_entries[entryId].minTimeBetweenWritesMs)) &&
            (data.mcuShuttingDown == false))
        {
            return;
        }
    }

    lib_nvm_recordHeader_S recordHeader = { 0U };

    lib_nvm_recordHeader_S * const currentRecord = (lib_nvm_recordHeader_S*)records[entryId].currentNonVolatileAddress_Ptr;
    lib_nvm_blockHeader_S * const blockHeaderCurrent = (lib_nvm_blockHeader_S * const)getBlockBaseAddress((uint32_t)data.currentPtr);
    uint8_t entrySize = (entryId <=  NVM_LAST_USED_INTERNAL_ENTRYID) ?
                        lib_nvm_private_entries[entryId].entrySize :
                        lib_nvm_entries[entryId - NVM_ENTRYID_INTERNAL_COUNT].entrySize;

    if (blockHeaderCurrent != (lib_nvm_blockHeader_S * const)getBlockBaseAddress((uint32_t)data.currentPtr + sizeof(lib_nvm_recordHeader_S) + entrySize))
    {
        initializeNVMBlock(getNextBlockStart((uint32_t)data.currentPtr));
        invalidateBlock(blockHeaderCurrent);
        return;
    }

    records[entryId].currentNonVolatileAddress_Ptr = (storage_t*)data.currentPtr;
    recordHeader.entryId = entryId;
    recordHeader.entrySize = entrySize;
    recordHeader.initialized = SET_STATE;
    recordHeader.discarded = (storage_t)~SET_STATE;
    data.currentPtr += sizeof(lib_nvm_recordHeader_S) / sizeof(storage_t);

    recordLog.totalRecordWrites++;

    if (entryId <= NVM_LAST_USED_INTERNAL_ENTRYID)
    {
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
        LIB_NVM_WRITE_TO_FLASH((uint32_t)data.currentPtr,
                               lib_nvm_private_entries[entryId].entryRam_Ptr,
                               lib_nvm_private_entries[entryId].entrySize);
        recordHeader.entry_version = lib_nvm_private_entries[entryId].version;
        recordHeader.crc = (storage_t)crc8_calculate(0xff,
                                                     (uint8_t*)data.currentPtr,
                                                     lib_nvm_private_entries[entryId].entrySize);
        data.currentPtr += lib_nvm_private_entries[entryId].entrySize / sizeof(storage_t);
#endif // NVM_FLASH_BACKED
    }
    else if (entryId >= NVM_ENTRYID_INTERNAL_COUNT)
    {
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
        uint8_t relative_entry = entryId - NVM_ENTRYID_INTERNAL_COUNT;
        LIB_NVM_WRITE_TO_FLASH((uint32_t)data.currentPtr,
                               lib_nvm_entries[relative_entry].entryRam_Ptr,
                               lib_nvm_entries[relative_entry].entrySize);
        recordHeader.entry_version = lib_nvm_entries[relative_entry].version;
        recordHeader.crc = (storage_t)crc8_calculate(0xff,
                                                     (uint8_t*)data.currentPtr,
                                                     lib_nvm_entries[relative_entry].entrySize);
        data.currentPtr += lib_nvm_entries[relative_entry].entrySize / sizeof(storage_t);
#endif // NVM_FLASH_BACKED
    }

#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    LIB_NVM_WRITE_TO_FLASH((uint32_t)records[entryId].currentNonVolatileAddress_Ptr,
                           (storage_t*)&recordHeader,
                           sizeof(lib_nvm_recordHeader_S));
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

void recordPopulateDefault(lib_nvm_entry_t entryId)
{
#if FEATURE_IS_ENABLED(NVM_FLASH_BACKED)
    if (entryId <= NVM_LAST_USED_INTERNAL_ENTRYID)
    {
        memcpy((uint32_t*)lib_nvm_private_entries[entryId].entryRam_Ptr, lib_nvm_private_entries[entryId].entryDefault_Ptr, lib_nvm_private_entries[entryId].entrySize);
    }
    else if (entryId >= NVM_ENTRYID_INTERNAL_COUNT)
    {
        uint8_t relative_entry = entryId - NVM_ENTRYID_INTERNAL_COUNT;
        memcpy((uint32_t*)lib_nvm_entries[relative_entry].entryRam_Ptr, lib_nvm_entries[relative_entry].entryDefault_Ptr, lib_nvm_entries[relative_entry].entrySize);
    }
#endif // NVM_FLASH_BACKED
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

    data.currentPtr += sizeof(lib_nvm_blockHeader_S) / sizeof(storage_t);

    for (lib_nvm_entry_t index = NVM_LAST_USED_INTERNAL_ENTRYID; index <= NVM_LAST_USED_INTERNAL_ENTRYID; index--)
    {
        records[index].writeRequired = true;
    }

    for (uint8_t index = NVM_ENTRYID_INTERNAL_COUNT; index < (uint8_t)NVM_TOTAL_ENTRYID_COUNT; index++)
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
    return (uint32_t)nvm_origin + (((addr - (uint32_t)nvm_origin) / NVM_BLOCK_SIZE) * NVM_BLOCK_SIZE);
}

static uint32_t getNextBlockStart(uint32_t addr)
{
    return ((getBlockBaseAddress(addr + NVM_BLOCK_SIZE) < (uint32_t)nvm_end) ?
            (uint32_t)((addr + NVM_BLOCK_SIZE) - (addr % NVM_BLOCK_SIZE)) :
            (uint32_t)nvm_origin);
}
#endif

#endif // NVM_LIB_ENABLED
