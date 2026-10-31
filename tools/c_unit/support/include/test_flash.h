/**
 * @brief Caller-owned flash model with atomic halfword programs and page erases.
 *
 * Enforces bounds, alignment and one-to-zero programming. The before_operation
 * hook runs before each halfword program/page erase, allowing fixture-owned
 * power-loss injection. Initialization resets metadata, not the storage bytes.
 * This model does not simulate driver errors or partially erased pages.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint8_t  *memory;
    size_t   bytes;
    uint32_t page_size;
    unsigned operations;
    unsigned program_words;
    unsigned erased_pages;
    void (*before_operation)(void);
} test_flash_S;

void test_flash_init(test_flash_S *flash, void *memory, size_t bytes, uint32_t page_size);
bool test_flash_erase(test_flash_S *flash, uintptr_t address, uint16_t pages);
bool test_flash_program(test_flash_S *flash, uintptr_t address, const void *source, uint32_t bytes);
