/**
 * Display/CommonDisplay.h
 * This file contains common functions for display purposes, including
 * the common page definition
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Screen includes
#include "DisplayImports.h"
#include "Dots.h"

// other includes
#include "Types.h"
#include "Utility.h"
#include "printf.h"
#include <string.h>

// includes for data access
#include "IO.h"
#include "Screen.h"

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

#define update_infoDot(dot, val)     if (commonInfoDots[dot].update != NULL) { commonInfoDots[dot].update(&commonInfoDots[dot], val); }
#define update_infoText(dot, val)    if (commonInfoTexts[dot].update != NULL) { commonInfoTexts[dot].update(&commonInfoTexts[dot], val); }


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    INFO_DOT_DRS = 0,
    INFO_DOT_LAUNCH_CTRL_STATE,
    INFO_DOT_LAUNCH_CTRL_PAGE,
    INFO_DOT_AUTOSHIFT_STATE,
    INFO_DOT_TRACTION_CTRL_STATE,
    // DEBUG DOTS
    INFO_DOT_RUN_STATUS,
    INFO_DOT_ADC_CONV,
    INFO_DOT_CAN_TX,
    INFO_DOT_CAN_RX,
    INFO_DOT_COUNT,
} commonInfoDots_E;

typedef enum
{
    INFO_TEXT_TIRE_CIRC = 0,
    INFO_TEXT_COUNT,
} commonInfoTexts_E;


/******************************************************************************
 *                       U P D A T E  F U N C T I O N S
 ******************************************************************************/

static void update_infoDotGeneric(struct s_InfoDot *dot, bool val)
{
    dot->dot.color   = (val == true) ? GREEN : WHITE;
    dot->label.color = WHITE;
}


DECL_infoDotUpdaterGeneric(INFO_DOT_DRS);
DECL_infoDotUpdaterGeneric(INFO_DOT_LAUNCH_CTRL_STATE);
DECL_infoDotUpdaterGeneric(INFO_DOT_LAUNCH_CTRL_PAGE);
DECL_infoDotUpdaterGeneric(INFO_DOT_AUTOSHIFT_STATE);
DECL_infoDotUpdaterGeneric(INFO_DOT_TRACTION_CTRL_STATE);
DECL_infoDotUpdaterGeneric(INFO_DOT_RUN_STATUS);
DECL_infoDotUpdaterGeneric(INFO_DOT_ADC_CONV);
// DECL_infoDotUpdater(INFO_DOT_CAN_TX);
// DECL_infoDotUpdater(INFO_DOT_CAN_RX);

DECL_infoTextUpdater(INFO_TEXT_TIRE_CIRC)
{
    const char *str = (state == true) ? "WET" : "DRY";

    strncpy(dot->status.text, str, 10);
}


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static InfoDot_S  commonInfoDots[INFO_DOT_COUNT] =
{
    DECL_infoDot(INFO_DOT_DRS,                 "DRS",   20U,  35U, 160U, INFO_REL_POS_ABOVE),
    DECL_infoDot(INFO_DOT_LAUNCH_CTRL_STATE,   "LC",   460U,  35U, 160U, INFO_REL_POS_ABOVE),
    DECL_infoDot(INFO_DOT_LAUNCH_CTRL_PAGE,    "LC",   150U, 255U, 160U, INFO_REL_POS_ABOVE),
    DECL_infoDot(INFO_DOT_AUTOSHIFT_STATE,     "AS",   210U, 255U, 160U, INFO_REL_POS_ABOVE),
    DECL_infoDot(INFO_DOT_TRACTION_CTRL_STATE, "DIAG", 270U, 255U, 160U, INFO_REL_POS_ABOVE),
    DECL_infoDot(INFO_DOT_RUN_STATUS,          "RUN",  160U,  35U, 160U, INFO_REL_POS_ABOVE),
    DECL_infoDot(INFO_DOT_ADC_CONV,            "ADC",  200U,  35U, 160U, INFO_REL_POS_ABOVE),
    // DECL_infoDot(INFO_DOT_CAN_TX,              "TX",  240U,  35U, 160U, INFO_REL_POS_ABOVE),
    // DECL_infoDot(INFO_DOT_CAN_RX,              "RX",  280U,  35U, 160U, INFO_REL_POS_ABOVE),
};

static InfoText_S commonInfoTexts[INFO_TEXT_COUNT] =
{
    DECL_infoText(INFO_TEXT_TIRE_CIRC, "Tires", 330U, 255U, INFO_REL_POS_ABOVE),
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

static void display_start(void)
{
    EVE_start_cmd_burst();
    EVE_cmd_dl_burst(CMD_DLSTART);             // tells EVE to start a new display-list
    EVE_cmd_dl_burst(DL_CLEAR_RGB | BLACK);    // sets the background color
    EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
}


/**
 * display_end
 *
 */
static void display_end(void)
{
    EVE_cmd_dl_burst(DL_DISPLAY);    // put in the display list to mark its end
    EVE_cmd_dl_burst(CMD_SWAP);      // tell EVE to use the new display list
    EVE_end_cmd_burst();
}


/**
 * common_display
 *
 */
static void common_display(void)
{
    update_infoDot(INFO_DOT_DRS, IO.dig.btn0);
    update_infoDot(INFO_DOT_LAUNCH_CTRL_STATE, IO.dig.btn1);
    update_infoDot(INFO_DOT_LAUNCH_CTRL_PAGE, IO.dig.switch0);
    update_infoDot(INFO_DOT_AUTOSHIFT_STATE, IO.dig.switch1);
    update_infoDot(INFO_DOT_TRACTION_CTRL_STATE, IO.dig.switch3);
    update_infoDot(INFO_DOT_RUN_STATUS, SCR.heartbeat);
    update_infoDot(INFO_DOT_ADC_CONV, IO.heartbeat);
    // update_infoDot(INFO_DOT_CAN_TX, IO.dig.btn0);
    // update_infoDot(INFO_DOT_CAN_RX, IO.dig.btn0);

    update_infoText(INFO_TEXT_TIRE_CIRC, IO.dig.switch4);

    render_InfoDots(commonInfoDots, INFO_DOT_COUNT);
    render_InfoTexts(commonInfoTexts, INFO_TEXT_COUNT);

    // if (veh.io.remote_start)
    // {
    // FTImpl.ColorRGB(255, 0, 0);
    // FTImpl.Cmd_Text(240, 208, 27, FT_OPT_CENTER, "IGN");
    // }
}
