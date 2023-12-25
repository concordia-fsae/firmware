/**
 * Utility.h
 * This file contains various useful tools
 */

#pragma once

#include "Types.h"
#include "stddef.h"
#include "string.h"

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

// Count number of items in array
#define COUNTOF(x)    ((uint16_t)(sizeof(x) / sizeof(x[0])))    // return size of an array as uint16
// Count leading zeroes
static inline uint16_t u32CountLeadingZeroes(uint32_t x)
{
    uint16_t c = 0U; while (((x & 0x80000000UL) == 0UL) && (c < 32U))
    {
        c++; x <<= 1UL;
    }

    return c;
}

#define VAR_IN_SECTION(s)    __attribute__((section(s)))    // place a given variable in the specified linker section

#ifndef UNUSED
# define UNUSED(x)    (void)x
#endif

#define zero()    (0U)

// Join macros
// Convert token to string (doesn't expand)
#define STR(x)    #x

// Expand and convert token to string
#define XSTR(x)    STR(x)

// Join two tokens (doesn't expand)
#define DO_JOIN(x, y)    x ## y

// Expand and join tokens
#define JOIN(x, y)        DO_JOIN(x, y)
#define JOIN3(x, y, z)    JOIN(x, JOIN(y, z))

// Expand and join with `_` separating tokens
#define SNAKE(x, y)           JOIN3(x, _, y)
#define SNAKE3(x, y, z)       SNAKE(x, SNAKE(y, z))
#define SNAKE4(x, y, z, a)    SNAKE3(x, y, SNAKE(z, a))

// atomic bit stuff
// FIXME: check if these compile down to one instruction
// If not, make this atomic
#define setBitAtomic(bit)      ((bit) = true)
#define clearBitAtomic(bit)    ((bit) = false)
#define assignBitAtomic(bit, condition) \
    do {                                \
        if (condition) {                \
            (bit) = true;               \
        } else {                        \
            (bit) = false;              \
        }                               \
    } while (zero())


// FLAGS
// This should get moved elsewhere at some point
#define FLAG_bits_each               16
#define WORDS_FROM_COUNT(count)      (((uint16_t)count + (FLAG_bits_each - 1)) / FLAG_bits_each)
#define FLAG_GET_WORD(name, flag)    (name[(uint16_t)flag / FLAG_bits_each])
#define FLAG_GET_MASK(flag)          (1U << ((uint16_t)flag % FLAG_bits_each))

#define FLAG_create(name, size)      uint16_t(name)[WORDS_FROM_COUNT(size)]
#define FLAG_set(name, pos)          FLAG_GET_WORD(name, pos) |= FLAG_GET_MASK(pos)
#define FLAG_clear(name, pos)        FLAG_GET_WORD(name, pos) &= ~FLAG_GET_MASK(pos)
#define FLAG_get(name, pos)          ((bool)((FLAG_GET_WORD(name, pos) & FLAG_GET_MASK(pos)) == FLAG_GET_MASK(pos)))
#define FLAG_assign(name, pos, value) \
    do {                              \
        if (value)                    \
        {                             \
            FLAG_set(name, pos);      \
        }                             \
        else                          \
        {                             \
            FLAG_clear(name, pos);    \
        }                             \
    } while (zero())


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * FLAG_setAll
 * @brief Sets all of the flags in the given flag word
 * @param name Flag word name
 * @param count Number of flags in the given flag
 */
static inline void FLAG_setAll(uint16_t *name, uint16_t count)
{
    uint16_t numWords  = WORDS_FROM_COUNT(count);
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
static inline void FLAG_clearAll(uint16_t *name, uint16_t count)
{
    memset(name, 0x00, (uint16_t)(WORDS_FROM_COUNT(count) * FLAG_bits_each));
}


/*
 * FLAG_any
 * @brief check if any of the flags in the flag word is set
 * @param name Flag word name
 * @param count Number of flags in the given flag
 */
static inline bool FLAG_any(uint16_t *name, uint16_t count)
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
static inline bool FLAG_all(uint16_t *name, uint16_t count)
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
static inline bool FLAG_none(uint16_t *name, uint16_t count)
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
