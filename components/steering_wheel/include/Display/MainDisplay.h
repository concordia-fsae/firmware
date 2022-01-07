/*
 * Display/MainDisplay.h
 * This file contains the code to display the main display
 * on the screen
 */

#pragma once

// Screen includes
#include "Colors.h"
#include "DisplayItems.h"
#include "DisplayTypes.h"
#include "EVE.h"
#include "EVE_commands.h"

// other includes
#include "IO.h"
#include "Screen.h"
#include "Types.h"
#include "Utility.h"
#include "printf.h"


static void main_display(void)
{
    // these will become CANRX macros eventually
    static uint8_t  currGear = 0;
    static uint16_t currRPM  = 0;

    static bool fontsLoaded = false;

    if (!fontsLoaded)
    {
        EVE_cmd_romfont_burst(10U, 33U);  // load a bigger font
        fontsLoaded = true;
    }

    char gear[7][2] = { "?", "N", "1", "2", "3", "4", "5" };
    EVE_cmd_text_burst(240U, (272U / 2U) + -50U, 10U, EVE_OPT_CENTER, gear[currGear]);
    currGear = (currGear == 6U) ? 0U : currGear + 1U;

    EVE_cmd_number_burst(240U, (272U / 2U) + 25U, 10U, EVE_OPT_CENTER, currRPM);
    currRPM = (currRPM > 12000U) ? 0U : currRPM + 100U;
}
