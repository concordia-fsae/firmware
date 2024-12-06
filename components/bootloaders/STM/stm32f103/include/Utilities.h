/*
 * Utilities.h
 * Common utility definitions
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define UNUSED(x)                ((void)x)
#define SATURATE(x, min, max)    (x > max) ? (max) : ((x < min) ? (min) : (x))
// Count number of items in array
#define COUNTOF(x)               ((uint16_t)(sizeof(x) / sizeof(x[0]))) // return size of an array as uint16

// taken from rogerclark
#define SET_REG(addr, val)       do { *(volatile uint32_t*)(addr) = val; } while (0)
#define GET_REG(addr)            (*(volatile uint32_t*)(addr))


// taken from stm32 hal
#define SET_BIT(REG, BIT)                      ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)                    ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)                     ((REG)&(BIT))
#define CLEAR_REG(REG)                         ((REG) = (0x0))
#define WRITE_REG(REG, VAL)                    ((REG) = (VAL))
#define READ_REG(REG)                          ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK)    WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

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

// blocking delay
#define BLOCKING_DELAY(cycles)    for (uint16_t i = 0U; i < cycles; i++) { asm volatile ("nop"); }


// pragma macros
#define PRAGMA(x)         _Pragma(#x)
#define DIAG_PUSH()       PRAGMA(GCC diagnostic push)
#define DIAG_POP()        PRAGMA(GCC diagnostic pop)
#define DIAG_IGNORE(x)    PRAGMA(GCC diagnostic ignored #x)


// atomic bit stuff
#define setBitAtomic(bit)      ((bit) = true)
#define clearBitAtomic(bit)    ((bit) = false)
#define assignBitAtomic(bit, condition) \
        do {                            \
            if (condition) {            \
                (bit) = true;           \
            } else {                    \
                (bit) = false;          \
            }                           \
        } while (zero())
