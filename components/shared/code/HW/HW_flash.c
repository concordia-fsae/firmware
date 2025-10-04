/*
 * HW_FLASH.c
 * This file describes low-level, mostly hardware-specific Flash behaviour
 * https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "HW_flash.h"
#include "lib_utility.h"

#include "stdbool.h"
#include "string.h"

#if FEATURE_IS_DISABLED(MCU_STM32_USE_HAL)
# define FLASH_R_BASE      (0x40022000UL)        // base address of flash registers

# define FLASH_KEY1        (0x45670123UL)        // first key to be written to unlock flash
# define FLASH_KEY2        (0xCDEF89ABUL)        // second key to be written to unlock flash
# define FLASH_SR_BSY      (0x00000001UL)        // flash busy bit
# define FLASH_CR_PG       (0x00000001UL)        // flash program enable bit
# define FLASH_CR_PER      (0x00000002UL)        // flash page erase enable bit
# define FLASH_CR_LOCK     (0x00000080UL)        // flash page lockelocked. Page unlocked when 0b1. Defaults to 0b0
#endif

#define FLASH_ACR          (FLASH_R_BASE + 0x00UL) // flash access control register
#define FLASH_KEYR         (FLASH_R_BASE + 0x04UL) // flash key register for unlocking flash
#define FLASH_OPTKEYR      (FLASH_R_BASE + 0x08UL) // flash key register for unlocking option bytes
#define FLASH_SR           (FLASH_R_BASE + 0x0CUL) // flash status register
#define FLASH_CR           (FLASH_R_BASE + 0x10UL) // flash control register
#define FLASH_AR           (FLASH_R_BASE + 0x14UL) // flash access register
#define FLASH_OBR          (FLASH_R_BASE + 0x1CUL) // flash option byte register
#define FLASH_WRPR         (FLASH_R_BASE + 0x20UL) // flash write protection register

#define FLASH_CR_START     (0x00000040UL)          // flash page erase start bit
#define FLASH_RDPRT        (0x000000A5UL)          // flash read protection key

#define FLASH_PAGE_LAST    (0x08007C00UL)          // address of the last flash page
#define FLASH_SIZE_REG     (0x1FFFF7E0UL)          // register whose 4 least significant bits
                                                 // contains the size of the flash for this chip

#define FLASH_BUSY()       ((bool)(GET_REG(FLASH_SR) &FLASH_SR_BSY))  // returns true if flash is busy

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/
typedef struct
{
    uint16_t pageSize;
} flash_S;

flash_S flash;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void FLASH_init(void)
{
    const uint16_t pageSize = (uint16_t)FLASH_getPageSize();

    memset(&flash, 0x00, sizeof(flash_S));
    flash.pageSize = pageSize;
}

/*
 * FLASH_lock
 * @brief Lock the Flash Programming and Erase Controller (FPEC) once
 *        we're done interacting with the flash
 */
void FLASH_lock(void)
{
    // should we take down the HSI as well?
    SET_REG(FLASH_CR, FLASH_CR_LOCK);
}


/*
 * FLASH_unlock
 * @brief Unlock the Flash Programming and Erase Controller (FPEC) so we can perform
 *        operations on the flash
 */
void FLASH_unlock(void)
{
    if (READ_BIT(FLASH_CR, FLASH_CR_LOCK))
    {
        return;
    }
    SET_REG(FLASH_KEYR, FLASH_KEY1);
    SET_REG(FLASH_KEYR, FLASH_KEY2);
    while (READ_BIT(FLASH_CR, FLASH_CR_LOCK))
    {
        ;
    }
}

/*
 * FLASH_erasePages
 * @brief Erase the given number of pages of flash, starting from the given page address
 * @param startPageAddr uint32_t the address of the first page to erase
 * @param pages uint16_t the number of pages to erase
 */
bool FLASH_erasePages(uint32_t startPageAddr, uint16_t pages)
{
    FLASH_unlock();
    bool ret = true;

    // set the page erase bit
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_PER);

    for (uint16_t page = 0U; page < pages; page++)
    {
        uint32_t currPageAddr = startPageAddr + (page * flash.pageSize);
        uint32_t currPageEnd  = currPageAddr + flash.pageSize;
        while (FLASH_BUSY())
        {}

        // set the page address to erase
        SET_REG(FLASH_AR, currPageAddr);
        // start the erase process
        SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_START);

        while (FLASH_BUSY())
        {}

        // verify page has been erased
        // verify 64 bits at a time
        for (uint32_t flashWordAddr = currPageAddr; flashWordAddr < currPageEnd; flashWordAddr += 8)
        {
            if ((*(volatile uint64_t *)flashWordAddr) != 0xFFFFFFFFFFFFFFFFULL)
            {
                ret = false;
            }
        }
    }

    // clear the page erase bit
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) & ~FLASH_CR_PER);

    while (FLASH_BUSY())
    {}

    FLASH_lock();
    return ret;
}

/*
 * FLASH_writeWords
 * @brief write the given words of data starting at the given address in flash
 * @param addr uint32_t the address of the first word to write
 * @param data uint32_t* the words to write
 * @param dataLen uint16_t the number of words to write
 */
bool FLASH_writeWords(uint32_t addr, uint32_t *data, uint16_t dataLen)
{
    volatile uint16_t *addr_16;
    volatile uint16_t *data_16;
    bool              ret = true;

    FLASH_unlock();
    // Set the PG bit in FLASH_CR which enables writing
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_PG);

    // write all the data
    for (uint16_t dataOffset = 0U; dataOffset < dataLen; dataOffset++)
    {
        // get pointer to the word in flash, access as two half words.
        // word is 4 bytes, thus (offset * 4)
        addr_16 = (volatile uint16_t*)(addr + (dataOffset * 4U));

        // uint32_t cast to uint16_t array
        // so increment by 2 when we want to get to the next word
        data_16 = &(((volatile uint16_t*)data)[dataOffset * 2U]);

        while (FLASH_BUSY())
        {}
        addr_16[0] = data_16[0];

        while (FLASH_BUSY())
        {}
        addr_16[1] = data_16[1];

        while (FLASH_BUSY())
        {}

        // verify the data in flash,
        // compare as words
        if (*(volatile uint32_t *)(addr + (dataOffset * 4U)) != data[dataOffset])
        {
            ret = false;
            break;
        }
    }

    // Clear the PG bit in FLASH_CR
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) & ~FLASH_CR_PG);
    FLASH_lock();

    return ret;
}

/*
 * FLASH_writeHalfwords
 * @brief write the given halfwords of data starting at the given address in flash
 * @param addr uint32_t the address of the first halfword to write
 * @param data uint16_t* the halfwords to write
 * @param dataLen uint16_t the number of halfwords to write
 */
bool FLASH_writeHalfwords(uint32_t addr, uint16_t *data, uint16_t dataLen)
{
    bool ret = true;

    FLASH_unlock();
    // Set the PG bit in FLASH_CR which enables writing
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_PG);

    // write all the data
    for (uint16_t dataIdx = 0U; dataIdx < dataLen; dataIdx++)
    {
        while (FLASH_BUSY())
        {}
        volatile uint16_t* addr_16 = (volatile uint16_t*)(addr + dataIdx * sizeof(uint16_t));
        *addr_16 = data[dataIdx];

        while (FLASH_BUSY())
        {}

        // verify the data in flash,
        // compare as halfwords
        if (*addr_16 != data[dataIdx])
        {
            ret = false;
            break;
        }
    }

    // Clear the PG bit in FLASH_CR
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) & ~FLASH_CR_PG);
    FLASH_lock();

    return ret;
}

uint32_t FLASH_getPageSize(void)
{
    uint16_t *flashSize = (uint16_t *)(FLASH_SIZE_REG);

    // chips with more than 128 pages of flash have pages of size 2k
    // otherwise, pages are of size 1k
    if ((*flashSize & 0xFFFF) > 128U)
    {
        return 2048U;
    }
    else
    {
        return 1024U;
    }
}

