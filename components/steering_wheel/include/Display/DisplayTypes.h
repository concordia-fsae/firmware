
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "Types.h"


//***************************************************************************//
//                             T Y P E D E F S                               //
//***************************************************************************//

typedef struct
{
    uint16_t x;
    uint16_t y;
} Coordinates_S;

typedef enum
{
    DUAL_STATE_OFF = 0U,
    DUAL_STATE_ON,
    DUAL_STATE_COUNT,
} DualStateStates_E;

typedef struct
{
    struct
    {
        Coordinates_S     coords;
        Color_t           stateColors[DUAL_STATE_COUNT];
        DualStateStates_E state;
        uint16_t          size;
    } dot;

    struct
    {
        Coordinates_S coords;
        Color_t       color;
        char          text[10];
        uint16_t      fontSize;
    } label;
} InfoDotLabeled_S;

typedef struct
{
    struct
    {
        Coordinates_S     coords;
        Color_t           stateColors[DUAL_STATE_COUNT];
        char              stateTexts[DUAL_STATE_COUNT][10];
        DualStateStates_E state;
        uint16_t          fontSize;
    } statusText;

    struct
    {
        Coordinates_S coords;
        Color_t       color;
        char          text[10];
        uint16_t      fontSize;
    } labelText;
} InfoTextLabeled_S;

typedef enum
{
    INFO_DOT_TOP_LEFT = 0,    // DRS
    INFO_DOT_TOP_RIGHT,       // Launch Control
    INFO_DOT_BOT_LEFT_1,      // Launch Control screen
    INFO_DOT_BOT_LEFT_2,      // Autoshift Toggle
    INFO_DOT_BOT_RIGHT_1,     // Traction Control
    INFO_DOT_RUN_STATUS,
    INFO_DOT_ADC_CONV,
    INFO_DOT_CAN_TX,
    INFO_DOT_CAN_RX,
    INFO_DOT_COUNT,
} dispCommonInfoDots_E;

typedef enum
{
    INFO_TEXT_BOTTOM_RIGHT = 0,    // Wheel Circumference toggle (wet or dry)
    INFO_TEXT_COUNT,
} dispCommonInfoTexts_E;
