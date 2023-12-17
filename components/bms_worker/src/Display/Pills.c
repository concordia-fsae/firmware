/**
 * @file InfoPill.c
 * @brief Implementation of an InfoPill display item
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Display/Pills.h"  // module header include

#include "Display/DisplayImports.h"

#include "printf.h"
#include "Utility.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * render_InfoPill
 * @param pill TODO
 */
void render_ValuePill(ValuePill_S pill)
{
    // TODO: Math
    uint16_t offset   = (pill.width - pill.height) / 2U;
    uint16_t p1x      = pill.coords.x - offset;
    uint16_t p2x      = pill.coords.x + offset;

    char     text[20] = { 0U };

    snprintf(text, 20, "% 2.*f %s", pill.precision, pill.value, pill.unit);

    // make pill
    EVE_cmd_dl_burst(DL_COLOR_RGB | pill.bgColor);
    EVE_cmd_dl_burst(LINE_WIDTH(pill.height * 16U / 2U));    // Radius, in 1/16th of a pixel
    EVE_cmd_dl_burst(DL_BEGIN | EVE_LINES);
    EVE_cmd_dl_burst(VERTEX2F(p1x * 16U, pill.coords.y * 16U));
    EVE_cmd_dl_burst(VERTEX2F(p2x * 16U, pill.coords.y * 16U));
    EVE_cmd_dl_burst(DL_END);

    // write text on pill
    EVE_cmd_dl_burst(DL_COLOR_RGB | pill.fgColor);
    EVE_cmd_text_burst(pill.coords.x,
                       pill.coords.y,
                       21U,
                       EVE_OPT_CENTER,
                       text);

    // write label above pill
    EVE_cmd_dl_burst(DL_COLOR_RGB | pill.labelColor);
    EVE_cmd_text_burst(pill.coords.x,
                       pill.coords.y - (pill.height / 2U) - 8U,
                       21U,
                       EVE_OPT_CENTER,
                       pill.label);
}

/**
 * render_InfoPills
 * @param pills TODO
 * @param len TODO
 */
void render_ValuePills(ValuePill_S pills[], uint8_t len)
{
    for (uint8_t ui = 0U; ui < len; ui++)
    {
        render_ValuePill(pills[ui]);
    }
}
