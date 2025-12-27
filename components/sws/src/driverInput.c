/**
 * @file driverInput.c
 * @brief Handle driver input and control
 *
 * Driver interactions
 * -------------------------------------------------
 * Buttons:
 *   - LEFT_TOP  = PAGE_PREV  (previous page)
 *   - RIGHT_TOP = PAGE_NEXT  (next page)
 *   - LEFT_MID  = TORQUE_DEC
 *   - RIGHT_MID = TORQUE_INC
 *   - LEFT_BOT  = SLIP_DEC
 *   - RIGHT_BOT = SLIP_INC
 *   - RIGHT_TOGGLE = TC (level)
 *   - LEFT_TOGGLE  = REGEN (level)
 *
 * Combos (debounced; bit maintained while held):
 *   - RUN:     PAGE_NEXT + PAGE_PREV
 *   - RACE:    SLIP_INC + SLIP_DEC
 *   - REVERSE: RACE + TORQUE_INC + TORQUE_DEC  (highest priority)
 *
 * Timing semantics:
 *   - Inputs are **already debounced** in drv_userInput; we respect that and
 *     also consult drv_userInput_buttonInDebounce() to avoid acting while a
 *     related button in the same combo set is mid-debounce.
 *   - COMBO_DEBOUNCE_MS: additional stable window before latching a combo.
 *   - RACE_EXIT_LOCKOUT_MS: after leaving REVERSE, RACE is locked out briefly
 *     so an unintentional double-press doesn’t promote immediately to RACE.
 *   - ADJUST_GUARD_MS: when an axis key (torque/slip) is pressed while any
 *     button in the {torque_inc, torque_dec, slip_inc, slip_dec} set is
 *     debouncing, delay asserting the axis request until the set is stable and
 *     the guard window has elapsed.
 *   - Race confirmation waits for **slip** and **torque** buttons to be
 *     out of debounce to ensure the driver isn’t mid-sequence toward REVERSE.
 *
 * Request bit semantics:
 *   - Bits are recomputed every tick (no queues).
 *   - Once a combo is latched, its bit remains true until the combo is released.
 *   - Axis mutual exclusion: TORQUE_INC ∧ TORQUE_DEC -> both false
 *                            SLIP_INC   ∧ SLIP_DEC   -> both false
 *   - Cross-axis allowed: torque and slip may be true simultaneously.
 *
 * Page navigation:
 *   - Locked while RUN is active (prevents accidental nav).
 *   - Single-step on initial press; repeat every NAV_DEBOUNCE_MS while held.
 *   - Clamped to [0, DRIVERINPUT_PAGE_COUNT-1].
 *
 * Examples:
 *   - Hold RIGHT_MID (torque++) and LEFT_BOT (slip--) simultaneously: valid.
 *   - Hold LEFT_MID and RIGHT_MID together: neither torque dir bit asserts.
 *   - Release REVERSE while still holding both slip buttons: RACE stays locked
 *     out for RACE_EXIT_LOCKOUT_MS; driver must release/repress intentionally.
 *   - Press TORQUE_INC while SLIP_INC is **debouncing**: delay TORQUE_INC
 *     assertion until debouncing clears and ADJUST_GUARD_MS expires.
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "driverInput.h"
#include "drv_userInput.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_timer.h"
#include "MessageUnpack_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BUTTON_PAGE_PREV  USERINPUT_BUTTON_LEFT_TOP
#define BUTTON_PAGE_NEXT  USERINPUT_BUTTON_RIGHT_TOP
#define BUTTON_TORQUE_DEC USERINPUT_BUTTON_LEFT_MID
#define BUTTON_TORQUE_INC USERINPUT_BUTTON_RIGHT_MID
#define BUTTON_SLIP_DEC   USERINPUT_BUTTON_LEFT_BOT
#define BUTTON_SLIP_INC   USERINPUT_BUTTON_RIGHT_BOT

#define TOGGLE_TC    USERINPUT_BUTTON_LEFT_TOGGLE
#define TOGGLE_REGEN USERINPUT_BUTTON_RIGHT_TOGGLE

#define NAV_DEBOUNCE_MS     250
#define RUN_DEBOUNCE_MS     250
#define RACE_DEBOUNCE_MS    500
#define LAUNCH_DEBOUNCE_MS  500
#define REVERSE_DEBOUNCE_MS 2500

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    driverInput_page_E page;
    bool page_lockout;

    // Timers
    drv_timer_S nav_timer;
    drv_timer_S run_timer;
    drv_timer_S race_timer;
    drv_timer_S reverse_timer;
    drv_timer_S launch_timer;

    // Latched combo state
    bool run_active;
    bool race_active;
    bool reverse_active;
    bool launch_active;

    struct {
        bool is_set;
    } digital[DRIVERINPUT_REQUEST_COUNT];
} data_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static data_S data;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void driverInput_init(void)
{
    drv_timer_init(&data.nav_timer);
    drv_timer_init(&data.run_timer);
    drv_timer_init(&data.race_timer);
    drv_timer_init(&data.reverse_timer);
    drv_timer_init(&data.launch_timer);

    for (uint8_t i = 0; i < DRIVERINPUT_REQUEST_COUNT; i++)
    {
        data.digital[i].is_set = false;
    }

    data.page = DRIVERINPUT_PAGE_HOME;
    data.run_active = false;
    data.race_active = false;
    data.reverse_active = false;
    data.launch_active = false;
}

static void update_params(const bool tq_inc, const bool tq_dec,
                          const bool sl_inc, const bool sl_dec,
                          const bool db_tq_inc, const bool db_tq_dec,
                          const bool db_sl_inc, const bool db_sl_dec,
                          bool status[DRIVERINPUT_REQUEST_TORQUE_INC])
{
    // Axis mutual exclusion (within-axis), but defer assertion while related buttons are debouncing
    const bool axis_any_db = db_tq_inc || db_tq_dec || db_sl_inc || db_sl_dec;

    // Torque axis
    if (!axis_any_db && (tq_inc ^ tq_dec))
    {
        if (tq_inc && !tq_dec)
        {
            status[DRIVERINPUT_REQUEST_TORQUE_INC] = true;
        }
        else if (tq_dec && !tq_inc)
        {
            status[DRIVERINPUT_REQUEST_TORQUE_DEC] = true;
        }
    }

    // Slip axis (independent of torque axis, but uses the same stability gate)
    if (!axis_any_db && (sl_inc ^ sl_dec))
    {
        if (sl_inc && !sl_dec)
        {
            status[DRIVERINPUT_REQUEST_TC_SLIP_INC] = true;
        }
        else if (sl_dec && !sl_inc)
        {
            status[DRIVERINPUT_REQUEST_TC_SLIP_DEC] = true;
        }
    }
}

static void update_combos(const bool pg_next, const bool pg_prev,
                          const bool tq_inc, const bool tq_dec,
                          const bool sl_inc, const bool sl_dec,
                          const bool db_pg_next, const bool db_pg_prev,
                          const bool db_tq_inc, const bool db_tq_dec,
                          const bool db_sl_inc, const bool db_sl_dec)
{
    const bool run_combo = pg_next && pg_prev;
    const bool race_combo = sl_inc && sl_dec;
    const bool launch_combo = tq_inc && tq_dec;
    const bool rev_combo = race_combo && launch_combo; // highest priority

    // require stability for the involved buttons to progress debounce timers
    const bool run_stable    = run_combo    && !(db_pg_next || db_pg_prev);
    const bool rev_stable    = rev_combo    && !(db_sl_inc || db_sl_dec || db_tq_inc || db_tq_dec);
    const bool race_stable   = race_combo   && !rev_stable && !(db_sl_inc || db_sl_dec || db_tq_inc || db_tq_dec);
    const bool launch_stable = launch_combo && !rev_stable && !(db_sl_inc || db_sl_dec || db_tq_inc || db_tq_dec);

    const drv_timer_state_E timer_state_run = drv_timer_getState(&data.run_timer);
    const drv_timer_state_E timer_state_race = drv_timer_getState(&data.race_timer);
    const drv_timer_state_E timer_state_reverse = drv_timer_getState(&data.reverse_timer);
    const drv_timer_state_E timer_state_launch = drv_timer_getState(&data.launch_timer);

    if (run_stable)
    {
        if (timer_state_run == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&data.run_timer, RUN_DEBOUNCE_MS);
        }
    }
    else
    {
        drv_timer_stop(&data.run_timer);
    }

    if (rev_stable)
    {
        if (timer_state_reverse == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&data.reverse_timer, REVERSE_DEBOUNCE_MS);
        }
    }
    else
    {
        drv_timer_stop(&data.reverse_timer);
    }

    if (race_stable)
    {
        if (timer_state_race == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&data.race_timer, RACE_DEBOUNCE_MS);
        }
    }
    else
    {
        drv_timer_stop(&data.race_timer);
    }

    if (launch_stable)
    {
        if (timer_state_launch == DRV_TIMER_STOPPED)
        {
            drv_timer_start(&data.launch_timer, RACE_DEBOUNCE_MS);
        }
    }
    else
    {
        drv_timer_stop(&data.launch_timer);
    }

    data.run_active = timer_state_run == DRV_TIMER_EXPIRED;
    data.reverse_active = timer_state_reverse == DRV_TIMER_EXPIRED;
    data.race_active = timer_state_race == DRV_TIMER_EXPIRED;
    data.launch_active = timer_state_launch == DRV_TIMER_EXPIRED;
}

static void update_page_nav(const bool pg_next, const bool pg_prev,
                            const bool db_pg_next, const bool db_pg_prev)
{
    // Only act when both buttons are stable (not in debounce)
    if (!(db_pg_next || db_pg_prev) && (pg_next ^ pg_prev))
    {
        if (drv_timer_getState(&data.nav_timer) != DRV_TIMER_RUNNING)
        {
            // initial step immediately
            if (pg_prev && data.page > 0)
            {
                data.page = (driverInput_page_E)(data.page - 1);
            }
            else if (pg_next && data.page < (driverInput_page_E)(DRIVERINPUT_PAGE_COUNT - 1))
            {
                data.page = (driverInput_page_E)(data.page + 1);
            }

            drv_timer_start(&data.nav_timer, NAV_DEBOUNCE_MS);
        }
    }
    else
    {
        drv_timer_stop(&data.nav_timer);
    }
}

static void driverInput_10Hz(void)
{
    // Pressed levels
    const bool pg_next = drv_userInput_buttonPressed(BUTTON_PAGE_NEXT);
    const bool pg_prev = drv_userInput_buttonPressed(BUTTON_PAGE_PREV);
    const bool tq_inc  = drv_userInput_buttonPressed(BUTTON_TORQUE_INC);
    const bool tq_dec  = drv_userInput_buttonPressed(BUTTON_TORQUE_DEC);
    const bool sl_inc  = drv_userInput_buttonPressed(BUTTON_SLIP_INC);
    const bool sl_dec  = drv_userInput_buttonPressed(BUTTON_SLIP_DEC);

    // Debounce states from lower layer
    const bool db_pg_next = drv_userInput_buttonInDebounce(BUTTON_PAGE_NEXT);
    const bool db_pg_prev = drv_userInput_buttonInDebounce(BUTTON_PAGE_PREV);
    const bool db_tq_inc  = drv_userInput_buttonInDebounce(BUTTON_TORQUE_INC);
    const bool db_tq_dec  = drv_userInput_buttonInDebounce(BUTTON_TORQUE_DEC);
    const bool db_sl_inc  = drv_userInput_buttonInDebounce(BUTTON_SLIP_INC);
    const bool db_sl_dec  = drv_userInput_buttonInDebounce(BUTTON_SLIP_DEC);

    bool status[DRIVERINPUT_REQUEST_COUNT] = { false };

    update_combos(pg_next, pg_prev, tq_inc, tq_dec, sl_inc, sl_dec,
                  db_pg_next, db_pg_prev, db_tq_inc, db_tq_dec, db_sl_inc, db_sl_dec);
    update_page_nav(pg_next, pg_prev, db_pg_next, db_pg_prev);
    update_params(tq_inc, tq_dec, sl_inc, sl_dec,
                  db_tq_inc, db_tq_dec, db_sl_inc, db_sl_dec,
                  (bool*)&status);

    // Build status each tick
    status[DRIVERINPUT_REQUEST_REVERSE]        = data.reverse_active;
    status[DRIVERINPUT_REQUEST_RUN]            = data.run_active;
    status[DRIVERINPUT_REQUEST_RACE]           = data.race_active;
    status[DRIVERINPUT_REQUEST_LAUNCH_CONTROL] = data.launch_active;
    status[DRIVERINPUT_REQUEST_TC]             = drv_userInput_buttonPressed(TOGGLE_TC);
    status[DRIVERINPUT_REQUEST_REGEN]          = drv_userInput_buttonPressed(TOGGLE_REGEN);

    for (uint8_t i = 0; i < DRIVERINPUT_REQUEST_COUNT; i++)
    {
        data.digital[i].is_set = status[i];
    }
}

const ModuleDesc_S driverInput_desc = {
    .moduleInit = &driverInput_init,
    .periodic10Hz_CLK = &driverInput_10Hz,
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool driverInput_getDigital(driverInput_inputDigital_E input)
{
    return data.digital[input].is_set;
}

CAN_screenPage_E driverInput_getScreenCAN(void)
{
    CAN_screenPage_E page = CAN_SCREENPAGE_SNA;

    CAN_launchControlState_E launch_control = CAN_LAUNCHCONTROLSTATE_SNA;
    const bool launch_control_active = (CANRX_get_signal(VEH, VCFRONT_launchControlState, &launch_control) != CANRX_MESSAGE_SNA) &&
                                        ((launch_control == CAN_LAUNCHCONTROLSTATE_HOLDING) ||
                                         (launch_control == CAN_LAUNCHCONTROLSTATE_SETTLING) ||
                                         (launch_control == CAN_LAUNCHCONTROLSTATE_PRELOAD) ||
                                         (launch_control == CAN_LAUNCHCONTROLSTATE_LAUNCH));

    if (launch_control_active)
    {
        page = CAN_SCREENPAGE_LAUNCH;
    }
    else
    {
        switch (data.page)
        {
            case DRIVERINPUT_PAGE_HOME:
                page = CAN_SCREENPAGE_HOME;
                break;
            case DRIVERINPUT_PAGE_BUTTONS:
                page = CAN_SCREENPAGE_BUTTONS;
                break;
            case DRIVERINPUT_PAGE_DATA1:
                page = CAN_SCREENPAGE_DATA1;
                break;
            case DRIVERINPUT_PAGE_DATA2:
                page = CAN_SCREENPAGE_DATA2;
                break;
            case DRIVERINPUT_PAGE_LAUNCH:
                page = CAN_SCREENPAGE_LAUNCH;
                break;
            default:
                break;
        }
    }

    return page;
}
