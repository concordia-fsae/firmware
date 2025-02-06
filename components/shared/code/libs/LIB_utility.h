/** LIB_utility.h
 *  Header file for utility functions
 *  Hardware and component a-specific
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define COUNTOF(x) (sizeof(x)/sizeof(x[0U]))

// taken from rogerclark
#define SET_REG(addr, val)       do { *(volatile uint32_t*)(addr) = val; } while (0)
#define GET_REG(addr)            (*(volatile uint32_t*)(addr))

#if FEATURE_IS_ENABLED(MCU_STM32_USE_HAL)
#include "stm32f1xx.h"
#elif FEATURE_IS_DISABLED(MCU_STM32_USE_HAL)
// taken from stm32 hal
#define SET_BIT(REG, BIT)                      ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)                    ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)                     ((REG)&(BIT))
#define CLEAR_REG(REG)                         ((REG) = (0x0))
#define WRITE_REG(REG, VAL)                    ((REG) = (VAL))
#define READ_REG(REG)                          ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK)    WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))
#endif

// FLAGS
// This should get moved elsewhere at some point
#define FLAG_bits_each            16
#define WORDS_FROM_COUNT(count)   (((uint16_t)count + (FLAG_bits_each - 1)) / FLAG_bits_each)
#define FLAG_GET_WORD(name, flag) (name[(uint16_t)flag / FLAG_bits_each])
#define FLAG_GET_MASK(flag)       (1U << ((uint16_t)flag % FLAG_bits_each))

#define FLAG_create(name, size) uint16_t(name)[WORDS_FROM_COUNT(size)]
#define FLAG_set(name, pos)     FLAG_GET_WORD(name, pos) |= (uint16_t)FLAG_GET_MASK(pos)
#define FLAG_clear(name, pos)   FLAG_GET_WORD(name, pos) &= (uint16_t)~FLAG_GET_MASK(pos)
#define FLAG_get(name, pos)     ((bool)((FLAG_GET_WORD(name, pos) & FLAG_GET_MASK(pos)) == FLAG_GET_MASK(pos)))
#define FLAG_assign(name, pos, value) \
 do {                                 \
  if (value)                          \
  {                                   \
   FLAG_set(name, pos);               \
  }                                   \
  else                                \
  {                                   \
   FLAG_clear(name, pos);             \
  }                                   \
 } while (zero())

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * FLAG_setAll
 * @brief Sets all of the flags in the given flag word
 * @param name Flag word name
 * @param count Number of flags in the given flag
 */
static inline void FLAG_setAll(uint16_t* name, uint16_t count)
{
    uint16_t numWords  = (uint16_t)WORDS_FROM_COUNT(count);
    uint16_t extraBits = count % FLAG_bits_each;

    if (numWords > 1U)
    {
        memset(name, 0xFF, (uint16_t)(numWords * FLAG_bits_each));
    }
    if (extraBits > 0U)
    {
        name[numWords] |= (0xFF >> (FLAG_bits_each - extraBits));
    }
}


/*
 * FLAG_clearAll
 * @brief clears all of the bits in the given flag
 * @param name Flag word name
 * @param count Number of bits in the given flag
 */
static inline void FLAG_clearAll(uint16_t* name, uint16_t count)
{
    memset(name, 0x00, (uint16_t)(WORDS_FROM_COUNT(count) * FLAG_bits_each));
}


/*
 * FLAG_any
 * @brief check if any of the flags in the flag word is set
 * @param name Flag word name
 * @param count Number of flags in the given flag
 */
static inline bool FLAG_any(uint16_t* name, uint16_t count)
{
    for (uint16_t word = 0U; word < WORDS_FROM_COUNT(count); word++)
    {
        if ((FLAG_GET_WORD(name, word) & 0xFF) != 0U)
        {
            return true;
        }
    }
    return false;
}


/*
 * FLAG_all
 * @brief checks if all flags in the flag word are set
 * @param name Flag word name
 * @param count Number of bits in the given flag word
 */
static inline bool FLAG_all(uint16_t* name, uint16_t count)
{
    for (uint16_t word = 0U; word < WORDS_FROM_COUNT(count); word++)
    {
        if ((FLAG_GET_WORD(name, word) & 0xFF) != 0xFF)
        {
            return false;
        }
    }
    return true;
}


/*
 * FLAG_none
 * @brief checks if none of the flags in the flag word are set
 * @param name Flag word name
 * @param count Number of bits in the given flag word
 */
static inline bool FLAG_none(uint16_t* name, uint16_t count)
{
    for (uint16_t word = 0U; word < WORDS_FROM_COUNT(count); word++)
    {
        if ((FLAG_GET_WORD(name, word) & 0xFF) != 0U)
        {
            return false;
        }
    }
    return true;
}
