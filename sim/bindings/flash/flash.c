#include "rig_runtime.h"

#include "HW_flash.h"

void FLASH_init(void)
{
}

void FLASH_lock(void)
{
}

void FLASH_unlock(void)
{
}

uint32_t FLASH_getPageSize(void)
{
    return 1024U;
}

bool FLASH_erasePages(uint32_t pageAddr, uint16_t pages)
{
    (void)pageAddr;
    (void)pages;
    return true;
}

bool FLASH_writeHalfwords(uint32_t addr, uint16_t* data, uint16_t dataLen)
{
    (void)addr;
    (void)data;
    (void)dataLen;
    return true;
}

bool FLASH_writeWords(uint32_t addr, uint32_t* data, uint16_t dataLen)
{
    (void)addr;
    (void)data;
    (void)dataLen;
    return true;
}
