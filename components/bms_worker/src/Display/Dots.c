/**
 * @file Dots.c
 * @brief Info/Text Dots implementation
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Display/Dots.h"  // module header include

#include "Display/DisplayImports.h"

#include "printf.h"
#include "Utility.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void render_InfoDot(InfoDot_S dot)
{
    EVE_cmd_dl_burst(DL_COLOR_RGB | dot.dot.color);
    EVE_cmd_dl_burst(DL_BEGIN | EVE_POINTS);
    EVE_cmd_dl_burst(POINT_SIZE(dot.dot.size));
    EVE_cmd_dl_burst(VERTEX2F(dot.dot.coords.x * 16U, dot.dot.coords.y * 16U));
    EVE_cmd_dl_burst(DL_END);

    int8_t hOffset;
    int8_t vOffset;

    switch (dot.label.relPos)
    {
        case INFO_REL_POS_ABOVE:
            hOffset = 0;
            vOffset = -20;
            break;

        case INFO_REL_POS_BELOW:
            hOffset = 0;
            vOffset = 20;
            break;

        case INFO_REL_POS_LEFT:
            hOffset = -20;
            vOffset = 0;
            break;

        case INFO_REL_POS_RIGHT:
            hOffset = 20;
            vOffset = 0;
            break;

        default:
            hOffset = 0;
            vOffset = 0;
            break;
    }

    EVE_cmd_dl_burst(DL_COLOR_RGB | dot.label.color);
    EVE_cmd_text_burst(dot.dot.coords.x + hOffset,
                       dot.dot.coords.y + vOffset,
                       dot.label.fontSize,
                       EVE_OPT_CENTER,
                       dot.label.text);
}

/**
 * render_InfoDots
 * @param dots TODO
 * @param count TODO
 */
void render_InfoDots(InfoDot_S dots[], uint8_t count)
{
    for(uint8_t ui = 0U; ui < count; ui++)
    {
        render_InfoDot(dots[ui]);
    }
}

/**
 * render_InfoText
 * @param dot TODO
 */
void render_InfoText(InfoText_S dot)
{
    EVE_cmd_dl_burst(DL_COLOR_RGB | dot.status.color);
    EVE_cmd_text_burst(dot.status.coords.x,
                       dot.status.coords.y,
                       dot.status.fontSize,
                       EVE_OPT_CENTER,
                       dot.status.text);

    int8_t hOffset;
    int8_t vOffset;

    switch (dot.label.relPos)
    {
        case INFO_REL_POS_ABOVE:
            hOffset = 0;
            vOffset = -20;
            break;

        case INFO_REL_POS_BELOW:
            hOffset = 0;
            vOffset = 20;
            break;

        case INFO_REL_POS_LEFT:
            hOffset = -20;
            vOffset = 0;
            break;

        case INFO_REL_POS_RIGHT:
            hOffset = 20;
            vOffset = 0;
            break;

        default:
            hOffset = 0;
            vOffset = 0;
            break;
    }

    EVE_cmd_dl_burst(DL_COLOR_RGB | dot.label.color);
    EVE_cmd_text_burst(dot.status.coords.x + hOffset,
                       dot.status.coords.y + vOffset,
                       dot.label.fontSize,
                       EVE_OPT_CENTER,
                       dot.label.text);
}

/**
 * render_InfoTexts
 * @param dots TODO
 * @param count TODO
 */
void render_InfoTexts(InfoText_S dots[], uint8_t count)
{
    for(uint8_t ui = 0U; ui < count; ui++)
    {
        render_InfoText(dots[ui]);
    }
}
