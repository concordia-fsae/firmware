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
#include "Colors.h"
#include "DisplayItems.h"
#include "DisplayTypes.h"
#include "EVE.h"
#include "EVE_commands.h"

// other includes
#include "IO.h"
#include "Types.h"
#include "Utility.h"
#include "printf.h"


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

static void display_end(void)
{
    EVE_cmd_dl_burst(DL_DISPLAY);    // put in the display list to mark its end
    EVE_cmd_dl_burst(CMD_SWAP);      // tell EVE to use the new display list
    EVE_end_cmd_burst();
}

static void common_display(void)
{
    InfoDotLabeled_S* infoDot;
    for (dispCommonInfoDots_E i = (dispCommonInfoDots_E)0U; i < INFO_DOT_COUNT; i++)
    {
        infoDot = &dispCommonInfoDots[i];

        EVE_cmd_dl_burst(DL_COLOR_RGB | infoDot->dot.stateColors[infoDot->dot.state]);
        EVE_cmd_dl_burst(DL_BEGIN | EVE_POINTS);
        EVE_cmd_dl_burst(POINT_SIZE(infoDot->dot.size));
        EVE_cmd_dl_burst(VERTEX2II(infoDot->dot.coords.x,
                                   infoDot->dot.coords.y, 1, 0));
        EVE_cmd_dl_burst(DL_END);

        EVE_cmd_dl_burst(DL_COLOR_RGB | infoDot->label.color);
        EVE_cmd_text_burst(infoDot->label.coords.x,
                           infoDot->label.coords.y,
                           infoDot->label.fontSize,
                           EVE_OPT_CENTER,
                           infoDot->label.text);
    }

    InfoTextLabeled_S* infoText;
    for (dispCommonInfoTexts_E i = (dispCommonInfoTexts_E)0U; i < INFO_TEXT_COUNT; i++)
    {
        infoText = &dispCommonInfoTexts[i];

        EVE_cmd_dl_burst(DL_COLOR_RGB | infoText->statusText.stateColors[infoText->statusText.state]);
        EVE_cmd_text_burst(infoText->statusText.coords.x,
                           infoText->statusText.coords.y,
                           infoText->statusText.fontSize,
                           EVE_OPT_CENTER,
                           infoText->statusText.stateTexts[infoText->statusText.state]);

        EVE_cmd_dl_burst(DL_COLOR_RGB | infoText->labelText.color);
        EVE_cmd_text_burst(infoText->labelText.coords.x,
                           infoText->labelText.coords.y,
                           infoText->labelText.fontSize,
                           EVE_OPT_CENTER,
                           infoText->labelText.text);
    }


    char tempMCU[10] = { 0U };
    snprintf(tempMCU, 10, "% 2.*f", 2, IO.temp.mcu);
    EVE_cmd_text_burst(240U, 20U, 21U, EVE_OPT_CENTER, tempMCU);


    // if (veh.io.remote_start)
    // {
    //     FTImpl.ColorRGB(255, 0, 0);
    //     FTImpl.Cmd_Text(240, 208, 27, FT_OPT_CENTER, "IGN");
    // }
}
