/*
 * HW_FLASH.c
 * This file describes low-level, mostly hardware-specific Flash behaviour
 * https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "HW_FLASH.h"

#include "Utilities.h"


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static uint32_t getPageSize(void)
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


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * FLASH_lock
 * @brief Lock the Flash Programming and Erase Controller (FPEC) once
 *        we're done interacting with the flash
 */
void FLASH_lock(void)
{
    // should we take down the HSI as well?
    SET_REG(FLASH_CR, 0x00000080);
}


/*
 * FLASH_unlock
 * @brief Unlock the Flash Programming and Erase Controller (FPEC) so we can perform
 *        operations on the flash
 */
void FLASH_unlock(void)
{
    SET_REG(FLASH_KEYR, FLASH_KEY1);
    SET_REG(FLASH_KEYR, FLASH_KEY2);
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
    // FIXME: this can be checked once in a module-level init
    // instead of every time
    const uint16_t pageSize = getPageSize();

    bool ret = true;

    // set the page erase bit
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_PER);

    for (uint16_t page = 0U; page < pages; page++)
    {
        while (FLASH_BUSY())
        {}

        // set the page address to erase
        SET_REG(FLASH_AR, startPageAddr + (page * pageSize));
        // start the erase process
        SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_START);

        while (FLASH_BUSY())
        {}

        // verify page has been erased
        // verify 64 bits at a time
        for (uint32_t flashWord = startPageAddr; flashWord < (startPageAddr + (pages * pageSize)); flashWord += 4)
        {
            if ((*(volatile uint32_t *)flashWord) != 0xFFFFFFFFULL)
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
 * FLASH_eraseApp
 * @brief erase the application's flash
 */
bool FLASH_eraseApp(void)
{
    // defined in the linker script
    extern const uint8_t __FLASH_END[];
    volatile const uint32_t appFlashPages = ((const uint32_t)__FLASH_END - APP_FLASH_START) / getPageSize();

    bool result = false;

    if (appFlashPages > 0U)
    {
        result = FLASH_erasePages(APP_FLASH_START, appFlashPages);
    }

    return result;
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

    // Set the PG bit in FLASH_CR which enables writing
    SET_REG(FLASH_CR, GET_REG(FLASH_CR) | FLASH_CR_PG);

    // write all the data
    for (uint16_t dataOffset = 0U; dataOffset < dataLen; dataOffset++)
    {
        // get pointer to the word in flash, access as two half words.
        // word is 4 bytes, thus (offset * 4)
        addr_16 = (volatile uint16_t *)(addr + (dataOffset * 4U));

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

    return ret;
}
