/**
 * Utility.h
 * This file contains various useful tools
 */

#pragma once

#include "Types.h"
#include "stddef.h"

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

// Count number of items in array
#define COUNTOF(x)           ((uint16_t)(sizeof(x) / sizeof(x[0]))) // return size of an array as uint16
// Count leading zeroes
static inline uint16_t u32CountLeadingZeroes(uint32_t x) { uint16_t c = 0U; while(((x & 0x80000000UL) == 0UL) && (c < 32U)) { c++; x<<=1UL; } return c; }

#define VAR_IN_SECTION(s)    __attribute__((section(s)))            // place a given variable in the specified linker section

#ifndef UNUSED
# define UNUSED(x)    (void)x
#endif

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
