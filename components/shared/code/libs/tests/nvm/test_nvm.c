/**
 * @file test_nvm.c
 * @brief Real NVM implementation with deterministic flash, time and queue dependencies.
 *
 * Including the implementation gives the fixture access to volatile state for a
 * cold reboot, without adding reset/debug APIs to production firmware. Test
 * stimuli use public APIs except timing-policy checks and raw flash fixtures.
 *
 * Scope: NVM_TASK mode, even-sized entries, atomic halfword programming and
 * atomic page erase. This is not a concurrent RTOS scheduler or a model of
 * arbitrary partially erased pages. Driver false-return handling, odd-sized
 * entries and bare-metal mode remain separate coverage work. The declared
 * lib_nvm_check API has no implementation, so timing tests exercise its existing
 * private policy helper rather than claiming periodic integration coverage.
 */
#include "nvm_under_test.c"
#include "test_flash.h"
#include "test_freertos.h"
#include "unity.h"
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/* Match the firmware linker's exclusive end symbol. Non-PIE keeps addresses
 * representable by the production uint32_t flash ABI on a 64-bit Linux host. */
__asm__ (".pushsection .bss\n"
         ".balign 512\n"
         ".global __FLASH_NVM_ORIGIN\n"
         "__FLASH_NVM_ORIGIN:\n"
         ".global test_flash\n"
         "test_flash:\n"
         ".zero 1024\n"
         ".global __FLASH_NVM_END\n"
         "__FLASH_NVM_END:\n"
         ".popsection\n");

extern uint8_t        test_flash[2U * NVM_BLOCK_SIZE];

static uint32_t       value;
static uint32_t       other;
static const uint32_t value_default = 17U;
static const uint32_t other_default = 29U;
static unsigned       migration_calls;
static uint16_t       migrated_from;

static uint16_t migrate_value(uint16_t version, const storage_t *source)
{
    migration_calls++;
    migrated_from = version;
    if (version != 0U)
    {
        return version;
    }
    memcpy(&value, source, sizeof(value));
    value += 100U;
    return 1U;
}

const lib_nvm_entry_S lib_nvm_entries[NVM_ENTRYID_COUNT] = {
    [NVM_ENTRYID_LOG]   = { 0U, sizeof(recordLog), &recordLogDefault, &recordLog, 1000U, NULL          },
    [NVM_ENTRYID_CYCLE] = { 0U, sizeof(cycleLog),  &cycleLogDefault,  &cycleLog,  1000U, NULL          },
    [NVM_ENTRYID_VALUE] = { 1U, sizeof(value),     &value_default,    &value,      100U, migrate_value },
    [NVM_ENTRYID_OTHER] = { 0U, sizeof(other),     &other_default,    &other,      100U, NULL          },
};

static test_flash_S   flash;
static bool           shutting_down;
static bool           worker_active;
static jmp_buf        worker_idle;
static jmp_buf        power_loss;
/* -1 disables injection; otherwise cut before the next operation after N
 * halfword programs/page erases have completed. */
static int            operations_until_cut;

uint32_t    HW_TIM_getTimeMS(void)   { return test_freertos.ticks; }
bool        HW_mcuShuttingDown(void) { return shutting_down; }
uint32_t    test_page_size(void)     { return 128U; }
static void flash_boundary(void)
{
    if (operations_until_cut == 0)
    {
        longjmp(power_loss, 1);
    }
    if (operations_until_cut > 0)
    {
        operations_until_cut--;
    }
}

bool test_erase(uint32_t address, uint16_t pages)
{
    return test_flash_erase(&flash, address, pages);
}

bool test_program(uint32_t address, const void *source, uint32_t bytes)
{
    return test_flash_program(&flash, address, source, bytes);
}

static void worker_blocked(QueueHandle_t queue)
{
    (void)queue;
    TEST_ASSERT_TRUE(worker_active);
    longjmp(worker_idle, 1);
}

static void drain_worker(void)
{
    test_freertos.scheduler_state = taskSCHEDULER_RUNNING;
    worker_active                 = true;
    if (setjmp(worker_idle) == 0)
    {
        lib_nvm_run();
    }
    worker_active = false;
    TEST_ASSERT_EQUAL_UINT(0U, test_freertos.critical_depth);
    TEST_ASSERT_FALSE(lib_nvm_writesRequired());
}

static void task_delayed(void)
{
    // During cleanup, give the higher-priority NVM task a chance to run.
    if (!worker_active)
    {
        drain_worker();
    }
}

static void reset_ram(void)
{
    test_freertos_reset();
    test_freertos.on_block = worker_blocked;
    test_freertos.on_delay = task_delayed;
    memset(records,       0, sizeof(records));
    memset(&data,         0, sizeof(data));
    memset(recordHeaders, 0, sizeof(recordHeaders));
    memset(&blockHeader,  0, sizeof(blockHeader));
    memset(&recordLog,    0, sizeof(recordLog));
    memset(&cycleLog,     0, sizeof(cycleLog));
    value                = 0U;
    other                = 0U;
    shutting_down        = false;
    worker_active        = false;
    operations_until_cut = -1;
    migration_calls      = 0U;
    migrated_from        = 0U;
}

void setUp(void)
{
    // Bound an accidental init/recovery/queue infinite loop, including CI.
    alarm(10U);
    TEST_ASSERT_TRUE((uintptr_t)NVM_END <= UINT32_MAX);
    TEST_ASSERT_EQUAL_UINT32(2U * NVM_BLOCK_SIZE, (uintptr_t)NVM_END - (uintptr_t)NVM_ORIGIN);
    memset(test_flash, 0xff, 2U * NVM_BLOCK_SIZE);
    reset_ram();
    test_flash_init(&flash, test_flash, sizeof(test_flash), test_page_size());
    flash.before_operation = flash_boundary;
}

void        tearDown(void) { alarm(0U); }

static void boot(void)
{
    lib_nvm_init();
    drain_worker();
}

static void reboot(void)
{
    reset_ram();
    boot();
}

static void save_value(uint32_t next)
{
    value = next;
    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_VALUE));
    drain_worker();
}

static lib_nvm_recordHeader_S *current_header(lib_nvm_entry_t id)
{
    return (lib_nvm_recordHeader_S *)records[id].currentNvmAddr_Ptr;
}

static void assert_future_write(void)
{
    save_value(0x12345678U);
    reboot();
    TEST_ASSERT_EQUAL_HEX32(0x12345678U, value);
}

static void test_blank_flash_initializes_defaults_and_counters(void)
{
    boot();
    TEST_ASSERT_EQUAL_UINT32(value_default,     value);
    TEST_ASSERT_EQUAL_UINT32(other_default,     other);
    TEST_ASSERT_EQUAL_UINT32(1U,                lib_nvm_getTotalCycles());
    TEST_ASSERT_EQUAL_UINT32(1U,                lib_nvm_getTotalFailedRecordInit());
    TEST_ASSERT_EQUAL_UINT32(NVM_ENTRYID_COUNT, lib_nvm_getTotalEmptyRecordInit());
    TEST_ASSERT_EQUAL_UINT32(1U,                lib_nvm_getTotalBlockErases());
    TEST_ASSERT_EQUAL_UINT32(0U,                lib_nvm_getTotalFailedCrc());
    TEST_ASSERT_EQUAL_UINT32(0U,                lib_nvm_getTotalRecordsVersionFailed());
    TEST_ASSERT_GREATER_THAN_UINT32(0U, lib_nvm_getTotalRecordWrites());
    TEST_ASSERT_EQUAL_UINT(4U, flash.erased_pages);
}

static void test_reboot_restores_latest_values_and_increments_cycle(void)
{
    boot();
    save_value(41U);
    save_value(42U);
    other = 99U;
    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_OTHER));
    drain_worker();
    reboot();
    TEST_ASSERT_EQUAL_UINT32(42U, value);
    TEST_ASSERT_EQUAL_UINT32(99U, other);
    TEST_ASSERT_EQUAL_UINT32(2U,  lib_nvm_getTotalCycles());
    TEST_ASSERT_EQUAL_UINT32(1U,  lib_nvm_getTotalFailedRecordInit());
}

static void test_clear_entry_persists_default_without_changing_other(void)
{
    boot();
    save_value(123U);
    other = 87U;
    lib_nvm_requestWrite(NVM_ENTRYID_OTHER);
    drain_worker();
    TEST_ASSERT_TRUE(lib_nvm_clearEntry(NVM_ENTRYID_VALUE));
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    drain_worker();
    reboot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    TEST_ASSERT_EQUAL_UINT32(87U,           other);
}

static void test_pre_scheduler_request_is_seeded_by_worker(void)
{
    lib_nvm_init();
    value = 42U;
    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_VALUE));
    TEST_ASSERT_TRUE(lib_nvm_writeRequired(NVM_ENTRYID_VALUE));
    TEST_ASSERT_EQUAL_UINT(0U, uxQueueMessagesWaiting(data.queue_handle));
    drain_worker();
    reboot();
    TEST_ASSERT_EQUAL_UINT32(42U, value);
}

static void test_duplicate_requests_coalesce_latest_ram_value(void)
{
    boot();
    value = 41U;
    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_VALUE));
    value = 42U;
    TEST_ASSERT_TRUE(lib_nvm_requestWrite(NVM_ENTRYID_VALUE));
    TEST_ASSERT_EQUAL_UINT(1U, uxQueueMessagesWaiting(data.queue_handle));
    drain_worker();
    reboot();
    TEST_ASSERT_EQUAL_UINT32(42U, value);
}

static void test_failed_enqueue_is_recovered_from_pending_flag(void)
{
    boot();
    test_freertos.reject_enqueue = true;
    value                        = 55U;
    TEST_ASSERT_FALSE(lib_nvm_requestWrite(NVM_ENTRYID_VALUE));
    TEST_ASSERT_TRUE(lib_nvm_writeRequired(NVM_ENTRYID_VALUE));
    test_freertos.reject_enqueue = false;
    drain_worker();
    reboot();
    TEST_ASSERT_EQUAL_UINT32(55U, value);
}

static void test_full_queue_eventually_drains_all_pending_entries(void)
{
    boot();
    // Fill the queue with legitimate log actions before requesting a value.
    lib_nvm_entryAction_S action = { NVM_ENTRYID_LOG, NVM_ACTION_WRITE };
    for (unsigned i = 0U; i < NVM_ENTRYID_COUNT; i++)
    {
        TEST_ASSERT_TRUE(xQueueSend(data.queue_handle, &action, 0U));
    }
    value = 777U;
    TEST_ASSERT_FALSE(lib_nvm_requestWrite(NVM_ENTRYID_VALUE));
    TEST_ASSERT_TRUE(lib_nvm_writeRequired(NVM_ENTRYID_VALUE));
    drain_worker();
    reboot();
    TEST_ASSERT_EQUAL_UINT32(777U, value);
}

static void test_explicit_write_bypasses_minimum_interval(void)
{
    boot();
    test_freertos.ticks = records[NVM_ENTRYID_VALUE].lastWrittenTimeMs;
    save_value(4321U);
    reboot();
    TEST_ASSERT_EQUAL_UINT32(4321U, value);
}

static void test_unchanged_data_does_not_require_periodic_write(void)
{
    boot();
    test_freertos.ticks += 10000U;
    TEST_ASSERT_FALSE(evaluateWriteRequired(NVM_ENTRYID_VALUE));
}

static void test_write_interval_boundary(void)
{
    boot();
    value++;
    test_freertos.ticks = records[NVM_ENTRYID_VALUE].lastWrittenTimeMs + 99U;
    TEST_ASSERT_FALSE(evaluateWriteRequired(NVM_ENTRYID_VALUE));
    test_freertos.ticks++;
    TEST_ASSERT_TRUE(evaluateWriteRequired(NVM_ENTRYID_VALUE));
}

static void test_write_interval_across_timer_wrap(void)
{
    boot();
    test_freertos.ticks = UINT32_MAX - 49U;
    // Flush immediately to establish a timestamp just before wraparound.
    shutting_down       = true;
    save_value(44U);
    shutting_down       = false;
    value++;
    test_freertos.ticks = 49U;
    TEST_ASSERT_FALSE(evaluateWriteRequired(NVM_ENTRYID_VALUE));
    test_freertos.ticks = 50U;
    TEST_ASSERT_TRUE(evaluateWriteRequired(NVM_ENTRYID_VALUE));
}

static void test_shutdown_bypasses_write_interval(void)
{
    boot();
    value++;
    test_freertos.ticks = records[NVM_ENTRYID_VALUE].lastWrittenTimeMs;
    shutting_down       = true;
    TEST_ASSERT_TRUE(evaluateWriteRequired(NVM_ENTRYID_VALUE));
}

static void test_cleanup_flushes_unrequested_changes(void)
{
    boot();
    value         = 100U;
    other         = 200U;
    shutting_down = true;
    lib_nvm_cleanUp();
    TEST_ASSERT_FALSE(lib_nvm_writesRequired());
    reboot();
    TEST_ASSERT_EQUAL_UINT32(100U, value);
    TEST_ASSERT_EQUAL_UINT32(200U, other);
}

static void test_successful_version_migration_is_persisted(void)
{
    boot();
    save_value(7U);
    current_header(NVM_ENTRYID_VALUE)->entry_version = 0U;
    reboot();
    TEST_ASSERT_EQUAL_UINT(1U, migration_calls);
    TEST_ASSERT_EQUAL_UINT16(0U, migrated_from);
    TEST_ASSERT_EQUAL_UINT32(107U, value);
    TEST_ASSERT_EQUAL_UINT32(0U,   lib_nvm_getTotalRecordsVersionFailed());
    reboot();
    TEST_ASSERT_EQUAL_UINT(0U, migration_calls);
    TEST_ASSERT_EQUAL_UINT32(107U, value);
}

static void test_rejected_version_uses_default_and_counts_failure(void)
{
    boot();
    save_value(7U);
    current_header(NVM_ENTRYID_VALUE)->entry_version = 9U;
    reboot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    TEST_ASSERT_EQUAL_UINT32(1U,            lib_nvm_getTotalRecordsVersionFailed());
    assert_future_write();
}

static void test_missing_version_handler_uses_default(void)
{
    boot();
    current_header(NVM_ENTRYID_OTHER)->entry_version = 1U;
    reboot();
    TEST_ASSERT_EQUAL_UINT32(other_default, other);
    TEST_ASSERT_EQUAL_UINT32(1U,            lib_nvm_getTotalRecordsVersionFailed());
}

static void test_corrupt_payload_defaults_only_affected_entry(void)
{
    boot();
    save_value(77U);
    ((uint8_t *)(current_header(NVM_ENTRYID_VALUE) + 1))[0] ^= 1U;
    reboot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    TEST_ASSERT_EQUAL_UINT32(other_default, other);
    TEST_ASSERT_EQUAL_UINT32(1U,            lib_nvm_getTotalFailedCrc());
    assert_future_write();
    TEST_ASSERT_EQUAL_UINT32(1U, lib_nvm_getTotalFailedCrc());
}

static void test_unknown_entry_id_is_skipped(void)
{
    boot();
    current_header(NVM_ENTRYID_VALUE)->entryId = UINT8_MAX;
    reboot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    TEST_ASSERT_EQUAL_UINT32(other_default, other);
    TEST_ASSERT_EQUAL_UINT32(2U,            lib_nvm_getTotalFailedRecordInit());
    assert_future_write();
}

static void test_discarded_record_is_not_restored(void)
{
    boot();
    current_header(NVM_ENTRYID_VALUE)->discarded = SET_STATE;
    reboot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    TEST_ASSERT_EQUAL_UINT32(0U,            lib_nvm_getTotalFailedCrc());
    assert_future_write();
}

static void test_rollover_in_both_directions_preserves_values(void)
{
    boot();
    other = 987U;
    lib_nvm_requestWrite(NVM_ENTRYID_OTHER);
    drain_worker();
    for (unsigned i = 0U; i < 40U; i++)
    {
        save_value(100U + i);
        reboot();
        TEST_ASSERT_EQUAL_UINT32(100U + i, value);
        TEST_ASSERT_EQUAL_UINT32(987U,     other);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(2U, lib_nvm_getTotalBlockErases());
}

static void test_explicit_rebuild_preserves_ram_values(void)
{
    boot();
    value = 101U;
    other = 202U;
    TEST_ASSERT_TRUE(lib_nvm_nvmInitializeNewBlock());
    reboot();
    TEST_ASSERT_EQUAL_UINT32(101U, value);
    TEST_ASSERT_EQUAL_UINT32(202U, other);
}

static void test_invalid_blocks_recover_defaults(void)
{
    memset(test_flash, 0, 2U * NVM_BLOCK_SIZE);
    boot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    TEST_ASSERT_EQUAL_UINT32(other_default, other);
    assert_future_write();
}

static void test_unknown_block_version_rebuilds_defaults(void)
{
    boot();
    save_value(99U);
    lib_nvm_blockHeader_S *header = (void *)(uintptr_t)getBlockBaseAddress((uint32_t)data.currentPtr);
    header->nvm_version = 9U;
    reboot();
    TEST_ASSERT_EQUAL_UINT32(value_default, value);
    assert_future_write();
}

/* Raw image construction deliberately bypasses the flash driver to represent
 * old firmware images and corruption; normal library writes always use it. */
static uint32_t seed_block(void)
{
    lib_nvm_blockHeader_S header = { 0U, SET_STATE, UINT16_MAX };

    memcpy(test_flash, &header, sizeof(header));
    return sizeof(header);
}

static uint32_t seed_record(uint32_t offset, uint8_t id, uint16_t version,
                            const void *payload, uint16_t size)
{
    lib_nvm_recordHeader_S header = {
        .entryId       = id,
        .entry_version = version,
        .entrySize     = size,
        .crc           = crc8_calculate(0xff, payload, size),
        .initialized   = SET_STATE,
        .discarded     = UINT16_MAX,
    };

    TEST_ASSERT_TRUE(offset + sizeof(header) + size <= NVM_BLOCK_SIZE);
    memcpy(test_flash + offset,                  &header, sizeof(header));
    memcpy(test_flash + offset + sizeof(header), payload, size);
    return offset + sizeof(header) + align_up_bytes(size);
}

static void test_same_version_size_mismatch_is_rejected(void)
{
    for (unsigned size = 2U; size <= 6U; size += 4U)
    {
        setUp();
        uint32_t       offset      = seed_block();
        const uint8_t  payload[6]  = { 1U, 2U, 3U, 4U, 5U, 6U };
        offset = seed_record(offset, NVM_ENTRYID_VALUE, 1U, payload, size);
        const uint32_t saved_other = 321U;
        seed_record(offset, NVM_ENTRYID_OTHER, 0U, &saved_other, sizeof(saved_other));
        boot();
        TEST_ASSERT_EQUAL_UINT32(value_default, value);
        TEST_ASSERT_EQUAL_UINT32(saved_other,   other);
        TEST_ASSERT_EQUAL_UINT32(1U,            lib_nvm_getTotalFailedRecordInit());
        assert_future_write();
    }
}

static void test_malformed_record_length_rebuilds_without_losing_prior_records(void)
{
    const uint16_t invalid_sizes[] = { 0U, NVM_BLOCK_SIZE, UINT16_MAX };

    for (unsigned i = 0U; i < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); i++)
    {
        setUp();
        uint32_t       offset      = seed_block();
        const uint32_t saved_other = 456U;
        offset                                                       = seed_record(offset, NVM_ENTRYID_OTHER, 0U, &saved_other, sizeof(saved_other));
        seed_record(offset, NVM_ENTRYID_VALUE, 1U, &value_default, sizeof(value_default));
        ((lib_nvm_recordHeader_S *)(test_flash + offset))->entrySize = invalid_sizes[i];
        boot();
        TEST_ASSERT_EQUAL_UINT32(value_default, value);
        TEST_ASSERT_EQUAL_UINT32(saved_other,   other);
        TEST_ASSERT_EQUAL_UINT32(1U,            lib_nvm_getTotalFailedRecordInit());
        assert_future_write();
        TEST_ASSERT_EQUAL_UINT32(saved_other, other);
    }
}

static void test_latest_crc_failure_falls_back_to_previous_undiscarded_record(void)
{
    uint32_t       offset    = seed_block();
    const uint32_t old_value = 42U;
    const uint32_t new_value = 99U;

    offset                                                  = seed_record(offset, NVM_ENTRYID_VALUE, 1U, &old_value, sizeof(old_value));
    seed_record(offset, NVM_ENTRYID_VALUE, 1U, &new_value, sizeof(new_value));
    ((lib_nvm_recordHeader_S *)(test_flash + offset))->crc ^= 1U;
    boot();
    TEST_ASSERT_EQUAL_UINT32(old_value, value);
    TEST_ASSERT_EQUAL_UINT32(1U,        lib_nvm_getTotalFailedCrc());
    assert_future_write();
}

static void test_latest_undiscarded_record_wins(void)
{
    uint32_t       offset    = seed_block();
    const uint32_t old_value = 42U;
    const uint32_t new_value = 99U;

    offset = seed_record(offset, NVM_ENTRYID_VALUE, 1U, &old_value, sizeof(old_value));
    seed_record(offset, NVM_ENTRYID_VALUE, 1U, &new_value, sizeof(new_value));
    boot();
    TEST_ASSERT_EQUAL_UINT32(new_value, value);
    assert_future_write();
}

static void test_record_ending_exactly_at_block_boundary(void)
{
    uint32_t offset = seed_block();
    uint8_t  padding[NVM_BLOCK_SIZE - sizeof(lib_nvm_blockHeader_S) -
                     2U * sizeof(lib_nvm_recordHeader_S) - sizeof(value)] = { 0U };

    offset                                                                = seed_record(offset, UINT8_MAX, 0U, padding, sizeof(padding));
    const uint32_t saved_value = 765U;
    TEST_ASSERT_EQUAL_UINT32(NVM_BLOCK_SIZE,
                             seed_record(offset, NVM_ENTRYID_VALUE, 1U, &saved_value, sizeof(saved_value)));
    boot();
    TEST_ASSERT_EQUAL_UINT32(saved_value, value);
    assert_future_write();
}

static void test_power_loss_during_append_recovers_and_remains_writable(void)
{
    boot();
    save_value(0xaaaaaaaaU);
    unsigned       before     = flash.operations;
    save_value(0x55555555U);
    const unsigned operations = flash.operations - before;
    // Include the worker's complete trace, including diagnostic log updates.
    for (volatile unsigned cut = 0U; cut <= operations; cut++)
    {
        setUp();
        boot();
        save_value(0xaaaaaaaaU);
        value                = 0x55555555U;
        operations_until_cut = (int)cut;
        if (setjmp(power_loss) == 0)
        {
            lib_nvm_requestWrite(NVM_ENTRYID_VALUE);
            drain_worker();
        }
        reboot();
        TEST_ASSERT_TRUE(value == 0xaaaaaaaaU || value == 0x55555555U);
        TEST_ASSERT_EQUAL_UINT32(other_default, other);
        assert_future_write();
    }
}

static void test_power_loss_during_block_migration_preserves_committed_entries(void)
{
    // Measure then sweep the complete operation trace in each direction.
    for (volatile unsigned direction = 0U; direction < 2U; direction++)
    {
        setUp();
        boot();
        if (direction != 0U)
        {
            lib_nvm_nvmInitializeNewBlock();
        }
        save_value(88U);
        unsigned before     = flash.operations;
        lib_nvm_nvmInitializeNewBlock();
        unsigned operations = flash.operations - before;
        for (volatile unsigned cut = 0U; cut <= operations; cut++)
        {
            setUp();
            boot();
            if (direction != 0U)
            {
                lib_nvm_nvmInitializeNewBlock();
            }
            save_value(88U);
            operations_until_cut = (int)cut;
            if (setjmp(power_loss) == 0)
            {
                lib_nvm_nvmInitializeNewBlock();
            }
            reboot();
            TEST_ASSERT_EQUAL_UINT32(88U,           value);
            TEST_ASSERT_EQUAL_UINT32(other_default, other);
            assert_future_write();
        }
    }
}

static void test_power_loss_during_first_boot_recovers_defaults(void)
{
    boot();
    const unsigned operations = flash.operations;
    for (volatile unsigned cut = 0U; cut <= operations; cut++)
    {
        setUp();
        operations_until_cut = (int)cut;
        if (setjmp(power_loss) == 0)
        {
            boot();
        }
        reboot();
        TEST_ASSERT_EQUAL_UINT32(value_default, value);
        TEST_ASSERT_EQUAL_UINT32(other_default, other);
        assert_future_write();
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blank_flash_initializes_defaults_and_counters);
    RUN_TEST(test_reboot_restores_latest_values_and_increments_cycle);
    RUN_TEST(test_clear_entry_persists_default_without_changing_other);
    RUN_TEST(test_pre_scheduler_request_is_seeded_by_worker);
    RUN_TEST(test_duplicate_requests_coalesce_latest_ram_value);
    RUN_TEST(test_failed_enqueue_is_recovered_from_pending_flag);
    RUN_TEST(test_full_queue_eventually_drains_all_pending_entries);
    RUN_TEST(test_explicit_write_bypasses_minimum_interval);
    RUN_TEST(test_unchanged_data_does_not_require_periodic_write);
    RUN_TEST(test_write_interval_boundary);
    RUN_TEST(test_write_interval_across_timer_wrap);
    RUN_TEST(test_shutdown_bypasses_write_interval);
    RUN_TEST(test_cleanup_flushes_unrequested_changes);
    RUN_TEST(test_successful_version_migration_is_persisted);
    RUN_TEST(test_rejected_version_uses_default_and_counts_failure);
    RUN_TEST(test_missing_version_handler_uses_default);
    RUN_TEST(test_corrupt_payload_defaults_only_affected_entry);
    RUN_TEST(test_unknown_entry_id_is_skipped);
    RUN_TEST(test_discarded_record_is_not_restored);
    RUN_TEST(test_rollover_in_both_directions_preserves_values);
    RUN_TEST(test_explicit_rebuild_preserves_ram_values);
    RUN_TEST(test_invalid_blocks_recover_defaults);
    RUN_TEST(test_unknown_block_version_rebuilds_defaults);
    RUN_TEST(test_same_version_size_mismatch_is_rejected);
    RUN_TEST(test_malformed_record_length_rebuilds_without_losing_prior_records);
    RUN_TEST(test_latest_crc_failure_falls_back_to_previous_undiscarded_record);
    RUN_TEST(test_latest_undiscarded_record_wins);
    RUN_TEST(test_record_ending_exactly_at_block_boundary);
    RUN_TEST(test_power_loss_during_append_recovers_and_remains_writable);
    RUN_TEST(test_power_loss_during_block_migration_preserves_committed_entries);
    RUN_TEST(test_power_loss_during_first_boot_recovers_defaults);
    return UNITY_END();
}
