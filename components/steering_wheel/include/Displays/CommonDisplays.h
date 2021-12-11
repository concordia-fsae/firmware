/*
 * Displays/CommonDisplays.h
 * This file contains common functions for display purposes
 */

#pragma once

//***************************************************************************//
//                             I N C L U D E S                               //
//***************************************************************************//

#include "DisplayItems.h"
#include "EVE.h"
#include "Screen.h"
#include "Types.h"

extern void display_start(void);
extern void display_end(void);
extern void common_display(void);

//***************************************************************************//
//                       P U B L I C  F U N C T I O N S                      //
//***************************************************************************//

void display_start(void)
{
    EVE_start_cmd_burst();
    EVE_cmd_dl_burst(CMD_DLSTART);             // tells EVE to start a new display-list
    EVE_cmd_dl_burst(DL_CLEAR_RGB | BLACK);    // sets the background color
    EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
}

void display_end(void)
{
    EVE_cmd_dl_burst(DL_DISPLAY);    // put in the display list to mark its end
    EVE_cmd_dl_burst(CMD_SWAP);      // tell EVE to use the new display list
    EVE_end_cmd_burst();
}

void common_display(void)
{
    for (uint8_t i = 0; i < INFO_DOT_COUNT; i++)
    {
        EVE_cmd_dl_burst(DL_COLOR_RGB | dispCommonInfoDots[i].dot.color);
        EVE_cmd_dl_burst(DL_BEGIN | EVE_POINTS);
        EVE_cmd_dl_burst(POINT_SIZE(dispCommonInfoDots[i].dot.size));
        EVE_cmd_dl_burst(VERTEX2II(dispCommonInfoDots[i].dot.coords.x,
                                   dispCommonInfoDots[i].dot.coords.y, 1, 0));
        EVE_cmd_dl_burst(DL_END);

        EVE_cmd_text_burst(dispCommonInfoDots[i].label.coords.x,
                           dispCommonInfoDots[i].label.coords.y,
                           dispCommonInfoDots[i].label.fontSize,
                           EVE_OPT_CENTER,
                           dispCommonInfoDots[i].label.text);
    }



    // const char* d_w = !veh.io.sw4 ? "D" : "W";
    // FTImpl.Cmd_Text(330, 255, 28, FT_OPT_CENTER, d_w);


    // if (veh.io.remote_start)
    // {
    //     FTImpl.ColorRGB(255, 0, 0);
    //     FTImpl.Cmd_Text(240, 208, 27, FT_OPT_CENTER, "IGN");
    // }
}
