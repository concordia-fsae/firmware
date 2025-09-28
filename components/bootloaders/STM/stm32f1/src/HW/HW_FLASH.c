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
#include "UDS.h"

#include "Utilities.h"

#include "string.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/
typedef struct
{
    FLASH_eraseState_S eraseState;
    const uint16_t     pageSize;
    const uint16_t     appPageCount;
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
    bool updaterHasErased;
#endif
} flash_S;

flash_S flash;


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static uint16_t getPageSize(void)
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

void FLASH_init(void)
{
    const uint16_t pageSize = getPageSize();
    flash_S        fl       = {
        .eraseState   = { 0U },
        .pageSize     = pageSize,
        .appPageCount = (APP_FLASH_END - APP_FLASH_START) / pageSize,
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
        .updaterHasErased = false,
#endif
    };

    memcpy(&flash, &fl, sizeof(flash_S));
}

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
    if (READ_BIT(FLASH_CR, FLASH_CR_LOCK)) return;
    SET_REG(FLASH_KEYR, FLASH_KEY1);
    SET_REG(FLASH_KEYR, FLASH_KEY2);
}


FLASH_eraseState_S FLASH_getEraseState(void)
{
    return flash.eraseState;
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

#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
bool FLASH_updaterHasErasedFlash(void)
{
    return flash.updaterHasErased;
}
#endif

/*
 * FLASH_eraseAppStart
 * @brief start erasing the app a few pages at a time, so we don't block
 *        for the whole duration of the erase
 */
bool FLASH_eraseAppStart(void)
{
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
    flash.updaterHasErased = true;
#endif
    // reset the state
    setBitAtomic(flash.eraseState.started);
    clearBitAtomic(flash.eraseState.completed);
    clearBitAtomic(flash.eraseState.status);
    flash.eraseState.currPage = 0U;

    // trigger the first erase
    return FLASH_eraseAppContinue();
}


/*
 * FLASH_eraseAppContinue
 * @brief continue erasing the app once started
 */
bool FLASH_eraseAppContinue(void)
{
    // only continue if erase has been started and not completed
    if (!flash.eraseState.started || flash.eraseState.completed)
    {
        return false;
    }

    // if we've erased the last page of flash, we're done
    if (flash.eraseState.currPage == flash.appPageCount)
    {
        flash.eraseState.status    = true;
        flash.eraseState.completed = true;
        return true;
    }

    // otherwise, we should erase the page

    // make sure we don't boot in the middle of erasing
    UDS_extendBootTimeout(10);

    // erase the page
    FLASH_erasePage(APP_FLASH_START + (flash.eraseState.currPage * flash.pageSize));

    flash.eraseState.currPage++;
    return true;
}

/*
 * FLASH_eraseCompleteAck
 * @brief clear the flash erase state when requested, typically after
 *        UDS routine control request results
 */
void FLASH_eraseCompleteAck(void)
{
    if (flash.eraseState.completed)
    {
        clearBitAtomic(flash.eraseState.started);
        clearBitAtomic(flash.eraseState.completed);
        clearBitAtomic(flash.eraseState.status);
        flash.eraseState.currPage = 0U;
    }
}


/*
 * FLASH_eraseAppBlocking
 * @brief erase the application's flash, blocks for the whole duration
 */
bool FLASH_eraseAppBlocking(void)
{
    bool result = false;

    if (flash.appPageCount > 0U)
    {
        result = FLASH_erasePages(APP_FLASH_START, flash.appPageCount);
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
        addr = data[dataIdx];

        while (FLASH_BUSY())
        {}

        // verify the data in flash,
        // compare as halfwords
        if (*(volatile uint32_t *)(addr + (dataIdx * 2U)) != data[dataIdx])
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
