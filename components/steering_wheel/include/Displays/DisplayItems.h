/*
 * DisplayItems.h
 * This file contains typedefs and definitions for items that should
 * be displayed on the screen
 */

#pragma once

//***************************************************************************//
//                             I N C L U D E S                               //
//***************************************************************************//

// System includes
#include <stdint.h>

// Other includes
#include "Screen.h"
#include "Types.h"


//***************************************************************************//
//                             T Y P E D E F S                               //
//***************************************************************************//

typedef struct
{
    uint16_t x;
    uint16_t y;
} Coordinates_S;

typedef struct
{
    Color_t off;
    Color_t on;
} StateColors2_S;

typedef struct
{
    struct
    {
        Coordinates_S  coords;
        StateColors2_S colors;
        Color_t        color;
        uint16_t       size;
    } dot;

    struct
    {
        Coordinates_S coords;
        char          text[20];
        uint16_t      fontSize;
    } label;
} InfoDotText_S;

typedef struct
{
    struct
    {
        Coordinates_S coords;
        char          text[20];
        uint16_t      size;
        Color_t       color;
    } statusText;

    struct
    {
        Coordinates_S coords;
        char          text[20];
        uint16_t      fontSize;
    } labelText;
} StatusTextLabel_S;


//***************************************************************************//
//                           P U B L I C  V A R S                            //
//***************************************************************************//

typedef enum
{
    INFO_DOT_TOP_LEFT = 0,    // DRS
    INFO_DOT_TOP_RIGHT,       // Launch Control
    INFO_DOT_BOT_LEFT_1,      // Launch Control screen
    INFO_DOT_BOT_LEFT_2,      // Autoshift Toggle
    INFO_DOT_BOT_RIGHT_1,     //
    INFO_DOT_BOT_RIGHT_2,     // Wheel Circumference toggle (wet or dry)
    INFO_DOT_COUNT,
} dispCommonInfoDots_E;

static InfoDotText_S dispCommonInfoDots[] = {
    [INFO_DOT_TOP_LEFT] = {
        .dot.coords.x   = 20,
        .dot.coords.y   = 35,
        .dot.colors.off = WHITE,
        .dot.colors.on  = GREEN,
        .dot.color      = WHITE,
        .dot.size       = 160,
        .label.fontSize = 21,
        .label.text     = "DRS",
        .label.coords.x = 20,
        .label.coords.y = 15,
    },
    [INFO_DOT_TOP_RIGHT] = {
        .dot.coords.x   = 445,
        .dot.coords.y   = 35,
        .dot.colors.off = WHITE,
        .dot.colors.on  = GREEN,
        .dot.color      = WHITE,
        .dot.size       = 160,
        .label.fontSize = 21,
        .label.text     = "LC",
        .label.coords.x = 445,
        .label.coords.y = 15,
    },
    [INFO_DOT_BOT_LEFT_1] = {
        .dot.coords.x   = 150,
        .dot.coords.y   = 255,
        .dot.colors.off = WHITE,
        .dot.colors.on  = GREEN,
        .dot.color      = WHITE,
        .dot.size       = 160,
        .label.fontSize = 21,
        .label.text     = "LC",
        .label.coords.x = 150,
        .label.coords.y = 235,
    },
    [INFO_DOT_BOT_LEFT_2] = {
        .dot.coords.x   = 210,
        .dot.coords.y   = 255,
        .dot.colors.off = WHITE,
        .dot.colors.on  = GREEN,
        .dot.color      = WHITE,
        .dot.size       = 160,
        .label.fontSize = 21,
        .label.text     = "AS",
        .label.coords.x = 210,
        .label.coords.y = 235,
    },
    [INFO_DOT_BOT_RIGHT_1] = {
        .dot.coords.x   = 270,
        .dot.coords.y   = 255,
        .dot.colors.off = WHITE,
        .dot.colors.on  = GREEN,
        .dot.color      = WHITE,
        .dot.size       = 160,
        .label.fontSize = 21,
        .label.text     = "LC",
        .label.coords.x = 270,
        .label.coords.y = 235,
    },
    [INFO_DOT_BOT_RIGHT_2] = {
        .dot.coords.x   = 330,
        .dot.coords.y   = 255,
        .dot.colors.off = WHITE,
        .dot.colors.on  = GREEN,
        .dot.color      = WHITE,
        .dot.size       = 160,
        .label.fontSize = 28,
        .label.text     = "WC",
        .label.coords.x = 330,
        .label.coords.y = 235,
    },
};
