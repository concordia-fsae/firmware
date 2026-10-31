/** @brief Shared strict flash model for host C unit tests. */
#include "test_flash.h"
#include "unity.h"
#include <string.h>

void test_flash_init(test_flash_S *flash, void *memory, size_t bytes, uint32_t page_size)
{
    TEST_ASSERT_NOT_NULL(memory);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, page_size);
    TEST_ASSERT_EQUAL_UINT32(0U, page_size % sizeof(uint16_t));
    TEST_ASSERT_TRUE(bytes % page_size == 0U);
    TEST_ASSERT_TRUE((uintptr_t)memory % page_size == 0U);
    *flash = (test_flash_S){ .memory = memory, .bytes = bytes, .page_size = page_size };
}

static void operation_boundary(test_flash_S *flash)
{
    if (flash->before_operation != NULL)
    {
        flash->before_operation();
    }
    flash->operations++;
}

static void assert_flash_range(const test_flash_S *flash, uintptr_t address, size_t bytes)
{
    TEST_ASSERT_TRUE(address >= (uintptr_t)flash->memory);
    const uintptr_t offset = address - (uintptr_t)flash->memory;
    TEST_ASSERT_TRUE(offset <= flash->bytes);
    TEST_ASSERT_TRUE(bytes <= flash->bytes - offset);
    TEST_ASSERT_EQUAL_UINT32(0U, address % sizeof(uint16_t));
}

bool test_flash_erase(test_flash_S *flash, uintptr_t address, uint16_t pages)
{
    const uint32_t page_size = flash->page_size;

    assert_flash_range(flash, address, (size_t)pages * page_size);
    TEST_ASSERT_EQUAL_UINT32(0U, address % page_size);
    for (unsigned page = 0U; page < pages; page++)
    {
        operation_boundary(flash);
        memset((void *)(uintptr_t)(address + page * page_size), 0xff, page_size);
        flash->erased_pages++;
    }
    return true;
}

bool test_flash_program(test_flash_S *flash, uintptr_t address, const void *source, uint32_t bytes)
{
    assert_flash_range(flash, address, bytes);
    TEST_ASSERT_EQUAL_UINT32(0U, bytes % sizeof(uint16_t));
    for (uint32_t offset = 0U; offset < bytes; offset += sizeof(uint16_t))
    {
        uint16_t next;
        uint16_t *destination = (uint16_t *)(uintptr_t)(address + offset);
        memcpy(&next, (const uint8_t *)source + offset, sizeof(next));
        operation_boundary(flash);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(next, *destination & next,
                                        "Flash programming attempted a zero-to-one transition");
        *destination &= next;
        flash->program_words++;
    }
    return true;
}

