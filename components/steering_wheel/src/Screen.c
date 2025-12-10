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
#include "EVE.h"
#include "EVE_commands.h"
#include "IO.h"
#include "microui.h"
#include "ModuleDesc.h"
#include "Utility.h"


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
    mu_Context ctx;
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

static scr_S     scr;


static stateFn_t stateFunctions[SCR_STATE_COUNT] = {
    [SCR_STATE_UNAVAILABLE] = &process_unavailable,
    [SCR_STATE_RUNNING]     = &process_running,
    [SCR_STATE_RETRY]       = &process_retry,
    [SCR_STATE_INIT_ERROR]  = &process_error,
    [SCR_STATE_ERROR]       = &process_error,
};


static void      (*pageFunctions[SCR_PAGE_COUNT])(void) = {
    // define pages here
};

// defined statically in microui.c which is annoying
static mu_Rect   unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };

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

        // display the current page
        // if (pageFunctions[scr.page] != NULL)
        // {
            mu_begin(&scr.ctx);

            if (mu_begin_window(&scr.ctx, "main", mu_rect(0, 0, 480, 270)))
            {
                mu_layout_row(&scr.ctx, 2, (int[]) { 60, -1 }, 0);

                mu_label(&scr.ctx, "First:");
                if (mu_button(&scr.ctx, "Button1"))
                {
                    // printf("Button1 pressed\n");
                }

                mu_label(&scr.ctx, "Second:");
                if (mu_button(&scr.ctx, "Button2"))
                {
                    mu_open_popup(&scr.ctx, "My Popup");
                }

                if (mu_begin_popup(&scr.ctx, "My Popup"))
                {
                    mu_label(&scr.ctx, "Hello world!");
                    mu_end_popup(&scr.ctx);
                }

                mu_end_window(&scr.ctx);

                // pageFunctions[scr.page]();
            }
            mu_end(&scr.ctx);

            mu_Command *cmd = NULL;
            while (mu_next_command(&scr.ctx, &cmd))
            {
                switch (cmd->type)
                {
                    case MU_COMMAND_TEXT:
                        // render_text(cmd->text.font, cmd->text.str, cmd->text.pos.x, cmd->text.pos.y, cmd->text.color);
                        EVE_cmd_dl_burst(DL_COLOR_RGB |
                                         (uint32_t)(
                                             ((uint8_t)cmd->text.color.r < 4)
                                             & ((uint8_t)cmd->text.color.g < 2)
                                             & ((uint8_t)cmd->text.color.b))
                                         );
                        EVE_cmd_dl_burst(0x10000000 | cmd->text.color.a);
                        EVE_cmd_text_burst(cmd->text.pos.x,
                                           cmd->text.pos.y,
                                           21U,
                                           EVE_OPT_CENTER,
                                           cmd->text.str);
                        break;

                    case MU_COMMAND_RECT:
                        // render_rect(cmd->rect.rect, cmd->rect.color);
                        EVE_cmd_dl_burst(DL_COLOR_RGB |
                                         (uint32_t)(
                                             ((uint8_t)cmd->rect.color.r < 4)
                                             & ((uint8_t)cmd->rect.color.g < 2)
                                             & ((uint8_t)cmd->rect.color.b))
                                         );
                        EVE_cmd_dl_burst(0x10000000 | cmd->rect.color.a);
                        EVE_cmd_dl_burst(DL_BEGIN | EVE_RECTS);
                        EVE_cmd_dl_burst(VERTEX2F(cmd->rect.rect.x, cmd->rect.rect.y));
                        EVE_cmd_dl_burst(VERTEX2F(cmd->rect.rect.x + cmd->rect.rect.w, cmd->rect.rect.y + cmd->rect.rect.h));
                        EVE_cmd_dl_burst(DL_END);
                        break;

                    case MU_COMMAND_ICON:
                        // render_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
                        break;

                    case MU_COMMAND_CLIP:
                        // set_clip_rect(cmd->clip.rect);
                        if (memcmp(&cmd->clip.rect, &unclipped_rect, sizeof(mu_Rect)))
                        {
                            EVE_cmd_dl_burst(SCISSOR_SIZE(2048, 2048));
                            EVE_cmd_dl_burst(SCISSOR_XY(0, 0));
                        }
                        else
                        {
                            EVE_cmd_dl_burst(SCISSOR_SIZE(cmd->clip.rect.w, cmd->clip.rect.h));
                            EVE_cmd_dl_burst(SCISSOR_XY(cmd->clip.rect.x, cmd->clip.rect.y));
                        }
                        break;
                }
            }
        // }
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
        nextState      = SCR_STATE_RUNNING;
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
    if ((SCR.brightness != scr.currentBrightness)
        && (SCR.brightness >= 0x00)
        && (SCR.brightness <= BRIGHTNESS_MAX)
        )
    {
        EVE_memWrite8(REG_PWM_DUTY, SCR.brightness);    // setup backlight, range is from 0 = off to 0x80 = max
        scr.currentBrightness = SCR.brightness;
    }
}


static int text_width(mu_Font font, const char *str, int len)
{
    UNUSED(font);
    UNUSED(str);
    return len * 20;
}

static int text_height(mu_Font font)
{
    UNUSED(font);
    return 30;
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

    mu_init(&scr.ctx);

    scr.ctx.text_height = text_height;
    scr.ctx.text_width  = text_width;
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

    if (IO.dig.switch0)
    {
        scr.page = SCR_PAGE_LAUNCH_CONTROL;
    }
    else if (IO.dig.switch1)
    {
        scr.page = SCR_PAGE_DIAG;
    }
    else
    {
        scr.page = SCR_PAGE_MAIN;
    }
}


// module description
const ModuleDesc_S Screen_desc = {
    .moduleInit        = &Screen_init,
    .periodic10Hz_CLK  = &Screen10Hz_PRD,
    .periodic100Hz_CLK = &Screen100Hz_PRD,
};
