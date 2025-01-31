/*
 * Types.h
 * Contains typedefs that will be included frequently
 */
#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <float.h>
#include <stdbool.h>
#include <stdint.h>


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// sizes
#define U8_MAX     ((uint8_t)0xFFU)
#define S8_MAX     ((int8_t)0x7F)
#define S8_MIN     ((int8_t)0x80)
#define U16_MAX    ((uint16_t)0xFFFFU)
#define S16_MAX    ((int16_t)0x7FFF)
#define S16_MIN    ((int16_t)0x8000)
#define U32_MAX    ((uint32_t)0xFFFFFFFFU)
#define S32_MAX    ((int32_t)0x7FFFFFFF)
#define S32_MIN    ((int32_t)0x80000000)


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef float    float32_t;
typedef double   float64_t;

typedef uint32_t Time_t;
