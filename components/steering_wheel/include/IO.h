/**
 * IO.h
 * IO Module Header
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FloatTypes.h"
#include "Types.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t instCurrent;
    struct
    {
        float32_t board;
        float32_t gpu;
        float32_t mcu;
    } temp;

    // tmp for testing
    volatile struct
    {
        bool switch0 : 1;
        bool switch1 : 1;
        bool switch3 : 1;
        bool switch4 : 1;
        bool btn0    : 1;
        bool btn1    : 1;
    }    dig;

    bool heartbeat;
} IO_S;


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

extern IO_S IO;
