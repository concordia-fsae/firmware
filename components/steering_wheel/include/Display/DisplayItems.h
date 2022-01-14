/**
 * DisplayItems.h
 * This file contains definitions for items that should
 * be displayed on the screen
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System includes
#include <stdint.h>

// Other includes
#include "Display/Colors.h"
#include "Display/DisplayTypes.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

static InfoDotLabeled_S dispCommonInfoDots[INFO_DOT_COUNT] = {
    [INFO_DOT_TOP_LEFT] = {
        .dot.coords.x    = 20U,
        .dot.coords.y    = 35U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "DRS",
        .label.coords.x = 20U,
        .label.coords.y = 15U,
    },
    [INFO_DOT_TOP_RIGHT] = {
        .dot.coords.x    = 445U,
        .dot.coords.y    = 35U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "LC",
        .label.coords.x = 445U,
        .label.coords.y = 15U,
    },
    [INFO_DOT_BOT_LEFT_1] = {
        .dot.coords.x    = 150U,
        .dot.coords.y    = 255U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "LC",
        .label.coords.x = 150U,
        .label.coords.y = 235U,
    },
    [INFO_DOT_BOT_LEFT_2] = {
        .dot.coords.x    = 210U,
        .dot.coords.y    = 255U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "AS",
        .label.coords.x = 210U,
        .label.coords.y = 235U,
    },
    [INFO_DOT_BOT_RIGHT_1] = {
        .dot.coords.x    = 270U,
        .dot.coords.y    = 255U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "TC",
        .label.coords.x = 270U,
        .label.coords.y = 235U,
    },
    [INFO_DOT_RUN_STATUS] = {
        .dot.coords.x    = 20U,
        .dot.coords.y    = 80U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "RUN",
        .label.coords.x = 20U,
        .label.coords.y = 60U,
    },
    [INFO_DOT_ADC_CONV] = {
        .dot.coords.x    = 60U,
        .dot.coords.y    = 80U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "ADC",
        .label.coords.x = 60U,
        .label.coords.y = 60U,
    },
    [INFO_DOT_CAN_TX] = {
        .dot.coords.x    = 100U,
        .dot.coords.y    = 80U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "TX",
        .label.coords.x = 100U,
        .label.coords.y = 60U,
    },
    [INFO_DOT_CAN_RX] = {
        .dot.coords.x    = 140U,
        .dot.coords.y    = 80U,
        .dot.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = GREEN,
        },
        .dot.state      = DUAL_STATE_OFF,
        .dot.size       = 160U,
        .label.fontSize = 21U,
        .label.color    = WHITE,
        .label.text     = "RX",
        .label.coords.x = 140U,
        .label.coords.y = 60U,
    },
};

static InfoTextLabeled_S dispCommonInfoTexts[INFO_DOT_COUNT] = {
    [INFO_TEXT_BOTTOM_RIGHT] = {
        .statusText.coords.x    = 330U,
        .statusText.coords.y    = 255U,
        .statusText.stateColors = {
            [DUAL_STATE_OFF] = WHITE,
            [DUAL_STATE_ON]  = WHITE,
        },
        .statusText.stateTexts = {
            [DUAL_STATE_OFF] = { "DRY" },
            [DUAL_STATE_ON]  = { "WET" },

        },
        .statusText.state    = DUAL_STATE_OFF,
        .statusText.fontSize = 21U,
        .labelText.fontSize  = 21U,
        .labelText.color     = WHITE,
        .labelText.text      = "WC",
        .labelText.coords.x  = 330U,
        .labelText.coords.y  = 235U,
    },
};
