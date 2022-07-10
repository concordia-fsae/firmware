/**
 * Screen.c
 * This file defines the Screen Module, which drives the display on the steering wheel.
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Module header
#include "Screen.h"

// System includes
#include <string.h>

// other includes
#include "ModuleDesc.h"
#include "Types.h"

// display includes
#include "Display/CommonDisplay.h"
#include "Display/MainDisplay.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BRIGHTNESS_MAX    0x78
#define MAX_RETRIES       3


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t   chipId;
    uint8_t    currentBrightness;
    uint8_t    retryCount;
    uint16_t   errorCount;

    ScrPages_E page;
} scr_S;

typedef ScrState_E (*stateFn_t)(void);


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static ScrState_E process_unavailable(void);
static ScrState_E process_running(void);
static ScrState_E process_error(void);
static ScrState_E process_retry(void);


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

SCR_S SCR;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static scr_S scr;


static stateFn_t stateFunctions [SCR_STATE_COUNT] = {
    [SCR_STATE_UNAVAILABLE] = &process_unavailable,
    [SCR_STATE_RUNNING]     = &process_running,
    [SCR_STATE_RETRY]       = &process_retry,
    [SCR_STATE_INIT_ERROR]  = &process_error,
    [SCR_STATE_ERROR]       = &process_error,
};


static void (*pageFunctions[SCR_PAGE_COUNT])(void) = {
    [SCR_PAGE_MAIN]           = &main_display,
    [SCR_PAGE_LAUNCH_CONTROL] = NULL,
    [SCR_PAGE_DIAG]           = NULL,
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * process_running
 * @return TODO
 */
static ScrState_E process_running(void)
{
    static uint16_t timer;

    if (timer++ >= 5U)
    {
        timer = 0;

        display_start();     // start the display generation
        common_display();    // common display elements shared on all screens
        // display the current page
        if (pageFunctions[scr.page] != NULL)
        {
            pageFunctions[scr.page]();
        }
        display_end();    // end the display generation and tell the screen to show it
    }

    return SCR_STATE_RUNNING;
}

/**
 * process_unavailable
 * @return TODO
 */
static ScrState_E process_unavailable(void)
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
        SCR.initStatus = EVE_INIT_SUCCESS;
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

/**
 * process_error
 * @return TODO
 */
static ScrState_E process_error(void)
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

/**
 * process_retry
 * @return TODO
 */
static ScrState_E process_retry(void)
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


/**
 * updateBrightness_10Hz
 *
 */
static void updateBrightness_10Hz()
{
    if ((SCR.brightness != scr.currentBrightness)    //
        && (SCR.brightness >= 0x00)                  //
        && (SCR.brightness <= BRIGHTNESS_MAX))
    {
        EVE_memWrite8(REG_PWM_DUTY, SCR.brightness);    // setup backlight, range is from 0 = off to 0x80 = max
        scr.currentBrightness = SCR.brightness;
    }
}


/**
 * Screen_init
 *
 */
static void Screen_init(void)
{
    // initialize structs
    memset(&scr, 0x00, sizeof(scr));
    memset(&SCR, 0x00, sizeof(SCR));
}


/**
 * Screen10Hz_PRD
 *
 */
static void Screen10Hz_PRD(void)
{
    if (SCR.initStatus == EVE_INIT_SUCCESS)
    {
        updateBrightness_10Hz();
    }

    SCR.heartbeat = !SCR.heartbeat;
}


/**
 * Screen100Hz_PRD
 *
 */
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

