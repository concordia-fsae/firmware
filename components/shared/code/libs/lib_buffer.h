/**
 * @file lib_buffer.h
 * @brief Header file for ring buffer library
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_utility.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define LIB_BUFFER_CLEAR(name) memset(&(name)->buffer, 0U, COUNTOF((name)->buffer) * sizeof((name)->buffer[0]))
#define LIB_BUFFER_GETNEXTINDEX(name, pos) ((pos + 1) % COUNTOF((name)->buffer))

#define LIB_BUFFER_CIRC_CREATE(name, type, elements) \
    struct ring_##name##_S { \
        type buffer[elements]; \
        size_t currentPos; \
    } name
#define LIB_BUFFER_CIRC_PEEK(name) (name)->buffer[(name)->currentPos]
#define LIB_BUFFER_CIRC_PEEKN(name, index) \
    (name)->buffer[({ \
        int32_t _pos = (int32_t)(name)->currentPos + (int32_t)(index); \
        int32_t _len = (int32_t)COUNTOF((name)->buffer); \
        _pos %= _len; \
        if (_pos < 0) { _pos += _len; } \
        _pos; \
    })]
#define LIB_BUFFER_CIRC_GET(name) LIB_BUFFER_CIRC_PEEK(name); \
                                  (name)->currentPos = LIB_BUFFER_GETNEXTINDEX(name, (name)->currentPos)
#define LIB_BUFFER_CIRC_GETSET(name, newVal) LIB_BUFFER_CIRC_PEEK(name); \
                                             LIB_BUFFER_CIRC_PEEK(name) = newVal; \
                                             (name)->currentPos = LIB_BUFFER_GETNEXTINDEX(name, (name)->currentPos)
#define LIB_BUFFER_CIRC_CLEAR(name) LIB_BUFFER_CLEAR(name); \
                                    (name)->currentPos = 0U;

#define LIB_BUFFER_FIFO_CREATE(name, type, elements) \
    struct fifo_##name##_S { \
        type buffer[elements]; \
        size_t startPos; \
        size_t endPos; \
    } name

#define LIB_BUFFER_FIFO_RESERVE(name, n) \
    do { \
        size_t _n = (size_t)(n); \
        if (_n != 0U) { \
            (name)->endPos = ((name)->endPos + _n) % COUNTOF((name)->buffer); \
        } \
    } while (0)

#define LIB_BUFFER_FIFO_INSERT(name, val) \
    do { \
        (name)->buffer[(name)->endPos] = (val); \
        (name)->endPos = ((name)->endPos + 1U) % COUNTOF((name)->buffer); \
    } while (0)

#define LIB_BUFFER_FIFO_PEEK(name) (name)->buffer[(name)->startPos]
#define LIB_BUFFER_FIFO_PEEKN(name, index) (name)->buffer[((int32_t)(name)->startPos) + index > 0 ? \
                                           ((int32_t)(name)->startPos + index) % COUNTOF((name)->buffer) : \
                                           COUNTOF((name)->buffer) - ((-((int32_t)((name)->startPos) + index)) % COUNTOF((name)->buffer))]
#define LIB_BUFFER_FIFO_PEEKEND(name) (name)->buffer[(name)->endPos]

#define LIB_BUFFER_FIFO_POP(name) \
        LIB_BUFFER_FIFO_PEEK(name); \
        (name)->startPos = ((name)->startPos + 1U) % COUNTOF((name)->buffer) \

#define LIB_BUFFER_FIFO_GETMAXCONTINUOUS(name) \
    ((name)->endPos < (name)->startPos ? \
        ((name)->startPos - (name)->endPos - 1U) : \
        ((COUNTOF((name)->buffer) - (name)->endPos) - ((name)->startPos == 0U ? 1U : 0U)))

#define LIB_BUFFER_FIFO_GETLENGTH(name) \
    (((name)->endPos + COUNTOF((name)->buffer) - (name)->startPos) % COUNTOF((name)->buffer))

#define LIB_BUFFER_FIFO_CLEAR(name) LIB_BUFFER_CLEAR(name); \
                                    (name)->endPos = 0U; (name)->startPos = 0U
