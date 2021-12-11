/*
 * Screen.c
 * This file defines the Screen Module, which drives the display on the steering wheel.
 */


//***************************************************************************//
//                             I N C L U D E S                               //
//***************************************************************************//

// Module header
#include "Screen.h"

// System includes
#include <string.h>

// other includes
#include "Displays/Displays.h"
#include "ModuleDesc.h"
#include "Types.h"


//***************************************************************************//
//                              D E F I N E S                                //
//***************************************************************************//

#define BRIGHTNESS_MAX 0x78
#define MAX_RETRIES    3

//***************************************************************************//
//                             T Y P E D E F S                               //
//***************************************************************************//

typedef struct
{
    uint16_t chipId;
    uint8_t  currentBrightness;
    uint8_t  retryCount;
    uint16_t errorCount;
} scr_S;

//***************************************************************************//
//                               M A C R O S                                 //
//***************************************************************************//

//***************************************************************************//
//          P R I V A T E  F U N C T I O N  P R O T O T Y P E S              //
//***************************************************************************//

ScrState_E process_unavailable(void);
ScrState_E process_running(void);
ScrState_E process_error(void);
ScrState_E process_retry(void);

//***************************************************************************//
//                         P R I V A T E  V A R S                            //
//***************************************************************************//

static scr_S scr;

ScrState_E (*stateFunctions[SCR_STATE_COUNT])(void) = {
    [SCR_STATE_UNAVAILABLE] = &process_unavailable,
    [SCR_STATE_RUNNING]     = &process_running,
    [SCR_STATE_RETRY]       = &process_retry,
    [SCR_STATE_INIT_ERROR]  = &process_error,
    [SCR_STATE_ERROR]       = &process_error,
};


//***************************************************************************//
//                           P U B L I C  V A R S                            //
//***************************************************************************//

extern SCR_S SCR;


//***************************************************************************//
//                     P R I V A T E  F U N C T I O N S                      //
//***************************************************************************//


ScrState_E process_running(void)
{
    static uint16_t timer;

    if (timer++ >= 5U)
    {
        timer = 0;

        display_start();     // start the display generation
        common_display();    // common display elements shared on all screens
        display_end();       // end the display generation and tell the screen to show it
    }

    return SCR_STATE_RUNNING;
}

ScrState_E process_unavailable(void)
{
    EVE_InitStatus_E initStatus;
    ScrState_E       nextState;
    // if init is NONE, we're booting up
    if (SCR.initStatus == EVE_INIT_NONE)
    {
        // initialize the chip and store the status and chipId
        initStatus = EVE_init(&scr.chipId);
    }
    // if init is not NONE then we're in an error state
    else
    {
        initStatus = EVE_INIT_NONE;
        scr.chipId = 0U;
    }

    // if global init status is NONE while local is SUCCESS, we just started up
    if ((SCR.initStatus == EVE_INIT_NONE) && (initStatus == EVE_INIT_SUCCESS))
    {
        SCR.brightness = 0x60;    // default value for brightness until a request comes in from elsewhere to change it
        EVE_memWrite8(REG_PWM_DUTY, SCR.brightness);
        nextState = SCR_STATE_RUNNING;
    }
    // startup failed
    else if ((initStatus != EVE_INIT_SUCCESS) && (initStatus != EVE_INIT_NONE))
    {
        nextState = SCR_STATE_INIT_ERROR;
    }
    else
    {
        // screen isn't running and this isn't startup
        // need to decide what to do in this case
        // for now, just stay in UNAVAILABLE
        nextState = SCR_STATE_UNAVAILABLE;
    }

    return nextState;
}

ScrState_E process_error(void)
{
    ScrState_E nextState;
    scr.errorCount++;

    if ((scr.retryCount < MAX_RETRIES) && (SCR.state != SCR_STATE_INIT_ERROR))
    {
        nextState = SCR_STATE_RETRY;
    }
    else
    {
        // should either do something else here or just get rid of this state
        // it isn't doing much right now, but I suspect we'll end up wanting it
        nextState = SCR_STATE_UNAVAILABLE;
    }
    return nextState;
}

ScrState_E process_retry(void)
{
    ScrState_E nextState;
    while (scr.retryCount < MAX_RETRIES)
    {
        if (EVE_init(&scr.chipId) == EVE_INIT_SUCCESS)
        {
            SCR.initStatus = EVE_INIT_SUCCESS;
            nextState      = SCR_STATE_RUNNING;
            scr.retryCount = 0;
            break;
        }
    }

    // if retries fail to get the screen working,
    // go to unavailable
    if (SCR.initStatus != EVE_INIT_SUCCESS)
    {
        nextState = SCR_STATE_UNAVAILABLE;
    }
    return nextState;
}

// void initStaticBackground(void)
// {
//     EVE_memWrite8(REG_PWM_DUTY, 0x30); /* setup backlight, range is from 0 = off to 0x80 = max */
//     EVE_cmd_dl(CMD_DLSTART);           /* Start the display list */

//     EVE_cmd_dl(TAG(0)); /* do not use the following objects for touch-detection */

//     EVE_cmd_bgcolor(0x00c0c0c0); /* light grey */

//     // EVE_cmd_dl(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */

//     // /* draw a rectangle on top */
//     // EVE_cmd_dl(DL_BEGIN | EVE_RECTS);
//     // EVE_cmd_dl(LINE_WIDTH(1 * 16)); /* size is in 1/16 pixel */

//     // EVE_cmd_dl(DL_COLOR_RGB | BLUE_1);
//     // EVE_cmd_dl(VERTEX2F(0, 0));
//     // EVE_cmd_dl(VERTEX2F(EVE_HSIZE, LAYOUT_Y1 - 2));
//     // EVE_cmd_dl(DL_END);

//     // /* draw a black line to separate things */
//     // EVE_cmd_dl(DL_COLOR_RGB | BLACK);
//     // EVE_cmd_dl(DL_BEGIN | EVE_LINES);
//     // EVE_cmd_dl(VERTEX2F(0, LAYOUT_Y1 - 2));
//     // EVE_cmd_dl(VERTEX2F(EVE_HSIZE, LAYOUT_Y1 - 2));
//     // EVE_cmd_dl(DL_END);

//     EVE_cmd_text(EVE_HSIZE / 2, 15, 29, EVE_OPT_CENTERX, "EVE Demo");

//     // EVE_cmd_text(10, EVE_VSIZE - 50, 26, 0, "DL-size:");
//     // EVE_cmd_text(10, EVE_VSIZE - 35, 26, 0, "Time1:");
//     // EVE_cmd_text(10, EVE_VSIZE - 20, 26, 0, "Time2:");

//     // EVE_cmd_text(125, EVE_VSIZE - 35, 26, 0, "us");
//     // EVE_cmd_text(125, EVE_VSIZE - 20, 26, 0, "us");

//     EVE_cmd_dl(DL_DISPLAY); /* instruct the graphics processor to show the list */
//     EVE_cmd_dl(CMD_SWAP);   /* make this list active */

//     while (EVE_busy())
//         ;
// }
//
//

static void updateBrightness_10Hz()
{
    if ((SCR.brightness != scr.currentBrightness)    //
        && (SCR.brightness >= 0x00)                  //
        && (SCR.brightness <= BRIGHTNESS_MAX))
    {
        EVE_memWrite8(REG_PWM_DUTY, SCR.brightness); /* setup backlight, range is from 0 = off to 0x80 = max */
        scr.currentBrightness = SCR.brightness;
    }
}


static void Screen_init(void)
{
    // initialize structs
    memset(&scr, 0x00, sizeof(scr));
    memset(&SCR, 0x00, sizeof(SCR));
}


static void Screen10Hz_PRD(void)
{
    if (SCR.initStatus == EVE_INIT_SUCCESS)
    {
        updateBrightness_10Hz();
    }
}


static void Screen100Hz_PRD(void)
{
    SCR.state = stateFunctions[SCR.state]();
}


// module description
const ModuleDesc_S Screen_desc = {
    .moduleInit        = &Screen_init,
    .periodic10Hz_CLK  = &Screen10Hz_PRD,
    .periodic100Hz_CLK = &Screen100Hz_PRD,
};
