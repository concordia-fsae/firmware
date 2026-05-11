#define _GNU_SOURCE

#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "lib_nvm.h"
#include "unity.h"

#ifndef MAP_32BIT
#define MAP_32BIT 0
#endif

storage_t __FLASH_NVM_ORIGIN;
storage_t __FLASH_NVM_END;

#define TEST_FLASH_BYTES (NVM_BLOCK_SIZE * 2U)
#define TEST_FLASH_WORDS (TEST_FLASH_BYTES / sizeof(storage_t))

static lib_nvm_nvmRecordLog_S record_log_ram;
static lib_nvm_nvmCycleLog_S cycle_log_ram;
static uint32_t test_value_ram;
static const uint32_t test_value_default = 0x12345678UL;
static uint32_t test_time_ms;
static bool test_shutting_down;
static storage_t* test_flash;

const lib_nvm_entry_S lib_nvm_entries[NVM_ENTRYID_COUNT] = {
    [NVM_ENTRYID_LOG] = {
        .version = 0U,
        .entrySize = sizeof(record_log_ram),
        .entryDefault_Ptr = &recordLogDefault,
        .entryRam_Ptr = &record_log_ram,
        .minTimeBetweenWritesMs = 0U,
        .versionHandler_Fn = NULL,
    },
    [NVM_ENTRYID_CYCLE] = {
        .version = 0U,
        .entrySize = sizeof(cycle_log_ram),
        .entryDefault_Ptr = &cycleLogDefault,
        .entryRam_Ptr = &cycle_log_ram,
        .minTimeBetweenWritesMs = 0U,
        .versionHandler_Fn = NULL,
    },
    [NVM_ENTRYID_TEST_VALUE] = {
        .version = 1U,
        .entrySize = sizeof(test_value_ram),
        .entryDefault_Ptr = &test_value_default,
        .entryRam_Ptr = &test_value_ram,
        .minTimeBetweenWritesMs = 100U,
        .versionHandler_Fn = NULL,
    },
};

void lib_nvm_test_flash_write(uint32_t addr, const void* data, uint16_t len)
{
    memcpy((void*)(uintptr_t)addr, data, len);
}

void lib_nvm_test_flash_clear(uint32_t addr, uint16_t pages)
{
    memset((void*)(uintptr_t)addr, 0xFF, pages * lib_nvm_test_flash_page_size());
}

uint32_t lib_nvm_test_flash_page_size(void)
{
    return 128U;
}

uint32_t lib_nvm_test_time_ms(void)
{
    return test_time_ms;
}

bool HW_mcuShuttingDown(void)
{
    return test_shutting_down;
}

static void ensure_test_flash(void)
{
    if (test_flash == NULL)
    {
        void* const flash = mmap(NULL,
                                 TEST_FLASH_BYTES,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT,
                                 -1,
                                 0);
        TEST_ASSERT_TRUE(flash != MAP_FAILED);
        test_flash = (storage_t*)flash;
        TEST_ASSERT_TRUE(((uintptr_t)test_flash + TEST_FLASH_BYTES) <= UINT32_MAX);
    }
}

void setUp(void)
{
    ensure_test_flash();
    memset(test_flash, 0xFF, TEST_FLASH_BYTES);
    lib_nvm_test_reset();
    lib_nvm_test_setFlashRange(test_flash, test_flash + TEST_FLASH_WORDS);
    test_time_ms = 0U;
    test_shutting_down = false;
    test_value_ram = 0U;
    memset(&record_log_ram, 0, sizeof(record_log_ram));
    memset(&cycle_log_ram, 0, sizeof(cycle_log_ram));
}

void tearDown(void)
{
}

static void request_write_tracks_pending_entries(void)
{
    TEST_ASSERT_FALSE(lib_nvm_writeRequired(NVM_ENTRYID_TEST_VALUE));

    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_TEST_VALUE));

    TEST_ASSERT_TRUE(lib_nvm_writeRequired(NVM_ENTRYID_TEST_VALUE));
    TEST_ASSERT_TRUE(lib_nvm_writesRequired());
    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_TEST_VALUE));
}

static void clear_entry_restores_default_and_marks_write_required(void)
{
    test_value_ram = 0xFFFFFFFFUL;

    TEST_ASSERT_TRUE(lib_nvm_clearEntry(NVM_ENTRYID_TEST_VALUE));

    TEST_ASSERT_EQUAL_UINT32(test_value_default, test_value_ram);
    TEST_ASSERT_TRUE(lib_nvm_writeRequired(NVM_ENTRYID_TEST_VALUE));
}

static void getters_expose_zeroed_log_counters_before_flash_activity(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalRecordWrites());
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalFailedCrc());
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalBlockErases());
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalCycles());
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalFailedRecordInit());
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalEmptyRecordInit());
    TEST_ASSERT_EQUAL_UINT32(0U, lib_nvm_getTotalRecordsVersionFailed());
}

static void init_initializes_empty_flash_with_defaults(void)
{
    lib_nvm_init();

    TEST_ASSERT_EQUAL_UINT32(test_value_default, test_value_ram);
    TEST_ASSERT_TRUE(lib_nvm_writesRequired());
    TEST_ASSERT_EQUAL_UINT32(1U, lib_nvm_getTotalBlockErases());
    TEST_ASSERT_EQUAL_UINT32(1U, lib_nvm_getTotalFailedRecordInit());
    TEST_ASSERT_EQUAL_UINT32(NVM_ENTRYID_COUNT, lib_nvm_getTotalEmptyRecordInit());
    TEST_ASSERT_EQUAL_UINT32(1U, lib_nvm_getTotalCycles());
}

static void cleanup_writes_pending_entries_to_flash(void)
{
    lib_nvm_init();

    lib_nvm_cleanUp();

    TEST_ASSERT_FALSE(lib_nvm_writesRequired());
    TEST_ASSERT_TRUE(lib_nvm_getTotalRecordWrites() >= NVM_ENTRYID_COUNT);
}

static void initializes_next_flash_block(void)
{
    lib_nvm_init();

    TEST_ASSERT_TRUE(lib_nvm_nvmInitializeNewBlock());

    TEST_ASSERT_EQUAL_UINT32(2U, lib_nvm_getTotalBlockErases());
    TEST_ASSERT_TRUE(lib_nvm_writesRequired());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(request_write_tracks_pending_entries);
    RUN_TEST(clear_entry_restores_default_and_marks_write_required);
    RUN_TEST(getters_expose_zeroed_log_counters_before_flash_activity);
    RUN_TEST(init_initializes_empty_flash_with_defaults);
    RUN_TEST(cleanup_writes_pending_entries_to_flash);
    RUN_TEST(initializes_next_flash_block);
    return UNITY_END();
}
