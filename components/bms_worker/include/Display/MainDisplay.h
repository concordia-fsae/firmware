/**
 * Display/MainDisplay.h
 * This file contains the code to display the main display on the screen
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Screen includes
#include "DisplayImports.h"
#include "Pills.h"

// other includes
#include "Utility.h"
#include "printf.h"

// includes for data acccess
#include "IO.h"

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

#define update_valuePill(pill, val)    if (valuePills[pill].update != NULL) { valuePills[pill].update(&valuePills[pill], val); }
#define MM_TO_PX(mm)                   (uint16_t)(mm * 5U)


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    VALUE_PILL_FUEL_PRESSURE = 0U,
    VALUE_PILL_OIL_PRESSURE,
    VALUE_PILL_BATT_V,
    VALUE_PILL_CPU_TEMP,
    VALUE_PILL_COUNT,
} valuePills_E;


/******************************************************************************
 *                       U P D A T E  F U N C T I O N S
 ******************************************************************************/

DECL_valuePillUpdate(VALUE_PILL_FUEL_PRESSURE)
{
    pill->value      = val;
    pill->bgColor    = BLUE;
    pill->fgColor    = WHITE;
    pill->labelColor = WHITE;
}

DECL_valuePillUpdate(VALUE_PILL_OIL_PRESSURE)
{
    pill->value      = val;
    pill->bgColor    = BLUE;
    pill->fgColor    = WHITE;
    pill->labelColor = WHITE;
}

DECL_valuePillUpdate(VALUE_PILL_BATT_V)
{
    pill->value      = val;
    pill->bgColor    = BLUE;
    pill->fgColor    = WHITE;
    pill->labelColor = WHITE;
}

DECL_valuePillUpdate(VALUE_PILL_CPU_TEMP)
{
    pill->value      = val;
    pill->bgColor    = BLUE;
    pill->fgColor    = WHITE;
    pill->labelColor = WHITE;
}


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static ValuePill_S valuePills[VALUE_PILL_COUNT] =
{
    DECL_valuePill(VALUE_PILL_FUEL_PRESSURE, "FuelP", 60U,  90U, 80U, 30U, 0U, 1U, "kPa"),
    DECL_valuePill(VALUE_PILL_OIL_PRESSURE,  "OilP",  60U, 140U, 80U, 30U, 0U, 1U, "kPa"),
    DECL_valuePill(VALUE_PILL_BATT_V,        "BattV", 60U, 190U, 80U, 30U, 0U, 2U, "V"),
    DECL_valuePill(VALUE_PILL_CPU_TEMP,      "CPUT",  60U, 240U, 80U, 30U, 0U, 2U, "C"),
};


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * main_display
 *
 */
static void main_display(void)
{
    // these will become CANRX macros eventually
    static uint8_t  currGear    = 0;
    static uint16_t currRPM     = 0;

    static bool     fontsLoaded = false;

    if (!fontsLoaded)
    {
        EVE_cmd_romfont_burst(10U, 33U);  // load a bigger font
        fontsLoaded = true;
    }

    char gear[7][2] = { "?", "N", "1", "2", "3", "4", "5" };

    EVE_cmd_dl_burst(DL_COLOR_RGB | WHITE);
    EVE_cmd_text_burst(240U, (272U / 2U) + -50U, 10U, EVE_OPT_CENTER, gear[currGear]);
    currGear = (currGear == 6U) ? 0U : currGear + 1U;

    EVE_cmd_number_burst(240U, (272U / 2U) + 25U, 10U, EVE_OPT_CENTER, currRPM);
    currRPM  = (currRPM > 12000U) ? 0U : currRPM + 100U;

    update_valuePill(VALUE_PILL_FUEL_PRESSURE, 101.3f);
    update_valuePill(VALUE_PILL_OIL_PRESSURE, 10.01f);
    update_valuePill(VALUE_PILL_BATT_V, 13.42f);
    update_valuePill(VALUE_PILL_CPU_TEMP, IO.temp.mcu);

    render_ValuePills(valuePills, VALUE_PILL_COUNT);
}
