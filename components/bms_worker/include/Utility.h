/**
 * Utility.h
 * This file contains various useful tools
 */

#pragma once

#include "Types.h"
#include "stddef.h"
#include "string.h"
#include <stdint.h>

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
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

//uint8_t reverse_byte(uint8_t);


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

static inline uint8_t reverse_byte(uint8_t x)
{
    static const uint8_t table[] = {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
        0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
        0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
        0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
        0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
        0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
        0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
        0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
        0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
        0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
        0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
        0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
        0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
        0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
        0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
        0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
        0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
        0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
        0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
        0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
        0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
        0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
        0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
        0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
        0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
        0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
        0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
        0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
        0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
        0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
    };
    return table[x];
}

static inline uint8_t* reverse_bytes(uint8_t *in, uint8_t len)
{
    for(uint8_t i = 0; i < (len/2); i++) {
        uint8_t tmp = in[i];

        in[i] = in[len - i - 1];
        in[len - i - 1] = tmp;
    }

    return in;
}
