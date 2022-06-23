/**
 * @file NVM.c
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @brief Source code of the NVM interface
 * @version 0.1
 * @date 2022-06-19
 *
 * @copyright Copyright (c) 2022
 * 
 * @note For compile-time memory allocation within the NVM, it is necessary to
 *        add VAR_IN_SECTION("NVMRAM") after the definition of each variable
 *
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Module Header
#include "NVM.h"

// System includes
#include "string.h"

// Other includes
#include "Utility.h"
#include "ModuleDesc.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define NVM_ORIGIN    0x0800fc00
#define NVMRAM_ORIGIN 0x20004c00
#define NVM_SIZE   0x0400
#define PAGE_SIZE  FLASH_PAGE_SIZE
#define PAGE_COUNT NVM_SIZE / FLASH_PAGE_SIZE


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Writes the specified page in NVMRAM to NVM in flash
 * 
 * @param[in] page_num which page of NVMRAM to write to NVM
 * @return HAL_StatusTypeDef The status of the write
 */
HAL_StatusTypeDef NVM_write_page(uint8_t page_num)
{
  HAL_StatusTypeDef status;
  uint32_t offset = page_num * PAGE_SIZE;
  status = NVM_clear_page(page_num);
  status = HAL_FLASH_Unlock();
  for (uint32_t i = 0; i < PAGE_SIZE; i += 8)
  {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, NVM_ORIGIN + i + offset, *((uint64_t *) NVMRAM_ORIGIN + i/8 + offset/8));
  }
  status = HAL_FLASH_Lock();
  return status;
}

/**
 * @brief Read's the specified page of NVM flash to NVMRAM
 * 
 * @param[in] page_num Page to be read from NVM in flash to NVMRAM
 */
void NVM_read_page(uint8_t page_num)
{
  uint32_t offset = page_num * PAGE_SIZE;
  for (uint32_t i = 0; i < PAGE_SIZE; i += 8)
  {
    *((uint64_t *) NVMRAM_ORIGIN + i/8 + offset/8) = *((uint64_t *) NVM_ORIGIN + i/8 + offset/8);
  }
}

/**
 * @brief Clears the specified page of NVM in flash to 0xFFFFFFFF
 * 
 * @param[in] page_num Specified page of NVM in flash to clear 
 * @return HAL_StatusTypeDef status of clear 
 */
HAL_StatusTypeDef NVM_clear_page(uint8_t page_num)
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


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void NVM_init(void);


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes the NVMRAM to the momory stored in NVM section of flash
 */
void NVM_init(void) 
{
  memset((void *) NVMRAM_ORIGIN, 0x00, PAGE_SIZE * PAGE_COUNT);
  NVM_read_page(0);
}

const ModuleDesc_S NVM_desc = {
    .moduleInit       = &NVM_init,
};