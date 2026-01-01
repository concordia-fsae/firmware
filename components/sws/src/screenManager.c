/**
 * @file screenManager.c
 * @brief Source code to manage the screen displays
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "screenManager.h"
#include "app_faultManager.h"
#include "Utility.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "drv_timer.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define WARN_TIMER_CYCLE_MS 2000U

#define WARNING_INGRESS(warning, state) \
    if (state && !FLAG_get(sm.setWarnings, warning)) \
    { \
        FLAG_assign(sm.unseenWarnings, warning, state); \
    } \
    FLAG_assign(sm.setWarnings, warning, state);

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    ALERT_NONE = 0x00U,
    ALERT_REVERSE,
    ALERT_MC,
    ALERT_VCFRONT,
    ALERT_VCREAR,
    ALERT_VCPDU,
    ALERT_BMSB,
    ALERT_GLV,
    ALERT_EM,
    ALERT_COUNT,
} alerts_E;

typedef enum
{
    WARN_NONE = 0x00U,
    WARN_LOW_GLV,
    WARN_HOT_CELL,
    WARN_HOT_POWERTRAIN,
    WARN_LOW_CELL,
    WARN_CONTACTS_OPEN_IN_RUN,
    WARN_COUNT,
} warnings_E;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    alerts_E alert;
    warnings_E warning;

    drv_timer_S warningTimer;

    FLAG_create(setAlerts, ALERT_COUNT);
    FLAG_create(setWarnings, WARN_COUNT);
    FLAG_create(unseenWarnings, WARN_COUNT);
} sm;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static CAN_screenAlerts_E translateAlertToCAN(alerts_E alert)
{
    CAN_screenAlerts_E ret = CAN_SCREENALERTS_NONE;

    switch (alert)
    {
        case ALERT_REVERSE:
            ret = CAN_SCREENALERTS_REVERSE;
            break;
        case ALERT_MC:
            ret = CAN_SCREENALERTS_FAULT_MC;
            break;
        case ALERT_VCFRONT:
            ret = CAN_SCREENALERTS_FAULT_VCFRONT;
            break;
        case ALERT_VCPDU:
            ret = CAN_SCREENALERTS_FAULT_VCPDU;
            break;
        case ALERT_VCREAR:
            ret = CAN_SCREENALERTS_FAULT_VCREAR;
            break;
        case ALERT_BMSB:
            ret = CAN_SCREENALERTS_FAULT_BMSB;
            break;
        case ALERT_GLV:
            ret = CAN_SCREENALERTS_FAULT_GLV;
            break;
        case ALERT_EM:
            ret = CAN_SCREENALERTS_FAULT_EM;
            break;
        default:
            break;
    }

    return ret;
}

static CAN_screenWarnings_E translateWarningToCAN(warnings_E warning)
{
    CAN_screenWarnings_E ret = CAN_SCREENWARNINGS_NONE;

    switch (warning)
    {
        case WARN_LOW_GLV:
            ret = CAN_SCREENWARNINGS_LOW_GLV;
            break;
        case WARN_HOT_CELL:
            ret = CAN_SCREENWARNINGS_HOT_CELL;
            break;
        case WARN_HOT_POWERTRAIN:
            ret = CAN_SCREENWARNINGS_HOT_POWERTRAIN;
            break;
        case WARN_LOW_CELL:
            ret = CAN_SCREENWARNINGS_LOW_CELL;
            break;
        case WARN_CONTACTS_OPEN_IN_RUN:
            ret = CAN_SCREENWARNINGS_CONTACTS_OPEN_IN_RUN;
            break;
        default:
            break;
    }

    return ret;
}

static void getAlerts(void)
{
    CAN_gear_E gear = CAN_GEAR_SNA;
    CANRX_get_signal(VEH, VCFRONT_gear, &gear);

    FLAG_assign(sm.setAlerts, ALERT_REVERSE, gear == CAN_GEAR_REVERSE);
}

static void determineActiveAlert(void)
{
    if (FLAG_get(sm.setAlerts, ALERT_REVERSE))
    {
        sm.alert = ALERT_REVERSE;
    }
    else
    {
        sm.alert = ALERT_NONE;
    }
}

static void getWarnings(void)
{
    const bool lowGLV = app_faultManager_getNetworkedFault_state(VEH, VCPDU_faults, FM_FAULT_VCPDU_LOWVOLTAGE);
    const bool contactsOpeninRun = app_faultManager_getNetworkedFault_state(VEH, VCPDU_faults, FM_FAULT_VCPDU_CONTACTSOPENINRUN);

    WARNING_INGRESS(WARN_LOW_GLV, lowGLV);
    WARNING_INGRESS(WARN_CONTACTS_OPEN_IN_RUN, contactsOpeninRun);
}

static void determineActiveWarning(void)
{
    if (FLAG_any(sm.setWarnings, WARN_COUNT) || FLAG_any(sm.unseenWarnings, WARN_COUNT))
    {
        const drv_timer_state_E cycleState = drv_timer_getState(&sm.warningTimer);
        const uint16_t unseenWarning = FLAG_getFirst(sm.unseenWarnings, WARN_COUNT);
        uint16_t new_warning;

        if (cycleState == DRV_TIMER_EXPIRED)
        {
            drv_timer_start(&sm.warningTimer, WARN_TIMER_CYCLE_MS);

            if (unseenWarning == WARN_COUNT)
            {
                new_warning = FLAG_getNext(sm.setWarnings, WARN_COUNT, sm.warning + 1U);
                if (new_warning == WARN_COUNT)
                    new_warning = FLAG_getFirst(sm.setWarnings, WARN_COUNT);
            }
            else
            {
                new_warning = unseenWarning;
            }

            sm.warning = (warnings_E)new_warning;
        }
        else if (cycleState == DRV_TIMER_STOPPED)
        {
            sm.warning = FLAG_getFirst(sm.unseenWarnings, WARN_COUNT);
            drv_timer_start(&sm.warningTimer, WARN_TIMER_CYCLE_MS);
        }

        FLAG_clear(sm.unseenWarnings, sm.warning);
    }
    else
    {
        drv_timer_stop(&sm.warningTimer);
        sm.warning = WARN_NONE;
    }
}

static void screenManager_init(void)
{
    memset(&sm, 0x00U, sizeof(sm));
    drv_timer_init(&sm.warningTimer);
}

static void screenManager_10Hz(void)
{
    getAlerts();
    getWarnings();

    determineActiveAlert();
    determineActiveWarning();
}

const ModuleDesc_S screenManager_desc = {
    .moduleInit = &screenManager_init,
    .periodic10Hz_CLK = &screenManager_10Hz,
};

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

CAN_screenAlerts_E screenManager_getAlertCAN(void)
{
    return translateAlertToCAN(sm.alert);
}

CAN_screenWarnings_E screenManager_getWarningCAN(void)
{
    return translateWarningToCAN(sm.warning);
}
