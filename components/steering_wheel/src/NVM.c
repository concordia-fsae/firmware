/**
 * @file NVM.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-06-19
 *
 * @copyright Copyright (c) 2022
 *
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "NVM.h"
#include "Utility.h"
#include "string.h"


#define NVM_ORIGIN    0x0800fc00
#define NVMRAM_ORIGIN 0x20004c00
#define NVM_SIZE   0x0400
#define PAGE_SIZE  FLASH_PAGE_SIZE
#define PAGE_COUNT NVM_SIZE / FLASH_PAGE_SIZE

void nvm_init(void) 
{
  memset((void *) NVMRAM_ORIGIN, 0x00, PAGE_SIZE * PAGE_COUNT);
  nvm_read_page(0);
}

HAL_StatusTypeDef nvm_write_page(uint8_t page_num)
{
  HAL_StatusTypeDef status;
  uint32_t offset = page_num * PAGE_SIZE;
  status = nvm_clear_page(page_num);
  status = HAL_FLASH_Unlock();
  //status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, NVM_ORIGIN, *((uint64_t *) NVMRAM_ORIGIN));
  for (uint32_t i = 0; i < PAGE_SIZE; i += 8)
  {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, NVM_ORIGIN + i + offset, *((uint64_t *) NVMRAM_ORIGIN + i/8 + offset/8));
  }
  status = HAL_FLASH_Lock();
  return status;
}

void nvm_read_page(uint8_t page_num)
{
  uint32_t offset = page_num * PAGE_SIZE;
  for (uint32_t i = 0; i < PAGE_SIZE; i += 8)
  {
    *((uint64_t *) NVMRAM_ORIGIN + i/8 + offset/8) = *((uint64_t *) NVM_ORIGIN + i/8 + offset/8);
  }
}

HAL_StatusTypeDef nvm_clear_page(uint8_t page_num)
{
    FLASH_EraseInitTypeDef erase;
    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = NVM_ORIGIN + page_num * PAGE_SIZE;
    erase.NbPages     = 1;

    uint32_t          err;
    HAL_StatusTypeDef status;

    status = HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &err);
    status = HAL_FLASH_Lock();

    return status;
}