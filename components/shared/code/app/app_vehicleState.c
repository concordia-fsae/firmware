/**
 * @file app_vehicleState.c
 * @brief Source file for the vehicle state application
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_vehicleState.h"
#include "string.h"
#include "MessageUnpack_generated.h"
#include "drv_timer.h"
#include "drv_inputAD.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BOOT_TIME_MS 250U
#define VEHICLESTATE_TIMEOUT_MS 1000U
#define BOOT_SLEEP_DISABLE_MS 60000U

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

struct
{
    app_vehicleState_state_E state;
    drv_timer_S              bootTimer;

    drv_timer_S                  sleepTimeout;
    app_vehicleState_sleepable_E sleepState;
} vehicleState_data;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Translate the app_vehicleState_state_E to CAN_vehicleState_E
 * @param state the vehicle state
 * @return The equivalent CAN state
 */
static CAN_vehicleState_E translateToCANState(app_vehicleState_state_E state)
{
    CAN_vehicleState_E ret = CAN_VEHICLESTATE_INIT;

    switch (state)
    {
        case VEHICLESTATE_ON_GLV:
            ret = CAN_VEHICLESTATE_ON_GLV;
            break;
        case VEHICLESTATE_ON_HV:
            ret = CAN_VEHICLESTATE_ON_HV;
            break;
        case VEHICLESTATE_TS_RUN:
            ret = CAN_VEHICLESTATE_TS_RUN;
            break;
        case VEHICLESTATE_SLEEP:
            ret = CAN_VEHICLESTATE_SLEEP;
            break;
        default:
            break;
    }

    return ret;
}

static CAN_sleepFollowerState_E translateToCANSleepableState(app_vehicleState_sleepable_E state)
{
    CAN_sleepFollowerState_E ret = CAN_SLEEPFOLLOWERSTATE_SNA;

    switch (state)
    {
        case SLEEPABLE_OK:
            ret = CAN_SLEEPFOLLOWERSTATE_OK_TO_SLEEP;
            break;
        case SLEEPABLE_NOK:
            ret = CAN_SLEEPFOLLOWERSTATE_NOK_TO_SLEEP;
            break;
        case SLEEPABLE_ALARM:
            ret = CAN_SLEEPFOLLOWERSTATE_ALARM;
            break;
    }

    return ret;
}

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief Initialize the vehicle power state
 */
void app_vehicleState_init(void)
{
    memset(&vehicleState_data, 0x00U, sizeof(vehicleState_data));

    vehicleState_data.state = VEHICLESTATE_INIT;
    drv_timer_init(&vehicleState_data.bootTimer);
    drv_timer_init(&vehicleState_data.sleepTimeout);
    drv_timer_start(&vehicleState_data.sleepTimeout, BOOT_SLEEP_DISABLE_MS);
#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
    drv_timer_start(&vehicleState_data.bootTimer, BOOT_TIME_MS);
#endif
}

/**
 * @brief Run the periodic 100Hz task
 */
void app_vehicleState_run100Hz(void)
{
#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
    switch (vehicleState_data.state)
    {
        case VEHICLESTATE_INIT:
            if (drv_timer_getState(&vehicleState_data.bootTimer) == DRV_TIMER_EXPIRED)
            {
                vehicleState_data.state = VEHICLESTATE_ON_GLV;
                drv_timer_stop(&vehicleState_data.bootTimer);
            }
            break;
        case VEHICLESTATE_ON_GLV:
            if (drv_inputAD_getDigitalActiveState(VEHICLESTATE_INPUTAD_TSMS) == DRV_IO_ACTIVE)
            {
                vehicleState_data.state = VEHICLESTATE_ON_HV;
            }
            break;
        case VEHICLESTATE_ON_HV:
            {
                CAN_prechargeContactorState_E contacts = CAN_PRECHARGECONTACTORSTATE_OPEN;
                float32_t percentage = 0.0f;

                if (drv_inputAD_getDigitalActiveState(VEHICLESTATE_INPUTAD_TSMS) == DRV_IO_INACTIVE)
                {
                    vehicleState_data.state = VEHICLESTATE_ON_GLV;
                }
                else if ((VEHICLESTATE_CANRX_CONTACTORSTATE(&contacts) != CANRX_MESSAGE_VALID) ||
                         (contacts != CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED))
                {
                    break;
                }
                else if (drv_inputAD_getDigitalActiveState(VEHICLESTATE_INPUTAD_RUN_BUTTON) == DRV_IO_ACTIVE && ((VEHICLESTATE_CANRX_BRAKEPOSITION(&percentage) == CANRX_MESSAGE_VALID) && (percentage > 10.0f)))
                {
                    vehicleState_data.state = VEHICLESTATE_TS_RUN;
                }
            }
            break;
        case VEHICLESTATE_TS_RUN:
            if (drv_inputAD_getDigitalActiveState(VEHICLESTATE_INPUTAD_TSMS) == DRV_IO_INACTIVE)
            {
                vehicleState_data.state = VEHICLESTATE_ON_GLV;
            }
            break;
        case VEHICLESTATE_SLEEP:
            // Explicitly do nothing and handle later
            break;
    }

    if ((vehicleState_data.state != VEHICLESTATE_ON_GLV) &&
        (vehicleState_data.state != VEHICLESTATE_INIT) &&
        (vehicleState_data.state != VEHICLESTATE_SLEEP))
    {
        app_vehicleState_delaySleep(BOOT_SLEEP_DISABLE_MS);
    }
#else // FDEFS_MODE_LEADER
    CAN_vehicleState_E state = translateToCANState(vehicleState_data.state);

    if (VEHICLESTATE_CANRX_SIGNAL(&state) == CANRX_MESSAGE_VALID)
    {
        drv_timer_stop(&vehicleState_data.bootTimer);

        switch (state)
        {
            case CAN_VEHICLESTATE_INIT:
                vehicleState_data.state = VEHICLESTATE_INIT;
                break;
            case CAN_VEHICLESTATE_ON_GLV:
                vehicleState_data.state = VEHICLESTATE_ON_GLV;
                break;
            case CAN_VEHICLESTATE_ON_HV:
                vehicleState_data.state = VEHICLESTATE_ON_HV;
                break;
            case CAN_VEHICLESTATE_TS_RUN:
                vehicleState_data.state = VEHICLESTATE_TS_RUN;
                break;
            case CAN_VEHICLESTATE_SLEEP:
                vehicleState_data.state = VEHICLESTATE_SLEEP;
                break;
        }
    }
    else if (drv_timer_getState(&vehicleState_data.bootTimer) == DRV_TIMER_STOPPED)
    {
        drv_timer_start(&vehicleState_data.bootTimer, VEHICLESTATE_TIMEOUT_MS);
    }
    else if (drv_timer_getState(&vehicleState_data.bootTimer) == DRV_TIMER_EXPIRED)
    {
        vehicleState_data.state = VEHICLESTATE_ON_GLV;
        drv_timer_stop(&vehicleState_data.bootTimer);
    }
#endif // !FDEFS_MODE_LEADER

    const bool sleepExpired = drv_timer_getState(&vehicleState_data.sleepTimeout) == DRV_TIMER_EXPIRED;
    vehicleState_data.sleepState = sleepExpired ? SLEEPABLE_OK : SLEEPABLE_NOK;

#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
    if (sleepExpired)
    {
        CAN_sleepFollowerState_E swsSleepable = CAN_SLEEPFOLLOWERSTATE_SNA;
        CAN_sleepFollowerState_E vcfrontSleepable = CAN_SLEEPFOLLOWERSTATE_SNA;
        const bool canSleepSWS = (CANRX_get_signal(VEH, SWS_sleepable, &swsSleepable) == CANRX_MESSAGE_VALID) &&
                                 (swsSleepable == CAN_SLEEPFOLLOWERSTATE_OK_TO_SLEEP);
        const bool canSleepVCFRONT = (CANRX_get_signal(VEH, VCFRONT_sleepable, &vcfrontSleepable) == CANRX_MESSAGE_VALID) &&
                                     (vcfrontSleepable == CAN_SLEEPFOLLOWERSTATE_OK_TO_SLEEP);
        bool canSleepUDS = true;

        CANRX_MESSAGE_health_E (*uds_clients[])(void) = {
            CANRX_validate_func(VEH, UDSCLIENT_bmsbUdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsbUdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsw0UdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsw1UdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsw2UdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsw3UdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsw4UdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_bmsw5UdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_vcfrontUdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_vcrearUdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_swsUdsRequest),
            CANRX_validate_func(VEH, UDSCLIENT_vcpduUdsRequest),
        };

        for (uint8_t i = 0; (i < COUNTOF(uds_clients)) && canSleepUDS; i++)
        {
            if (uds_clients[i]() == CANRX_MESSAGE_VALID)
            {
                canSleepUDS = false;
            }
        }

        if (canSleepSWS && canSleepVCFRONT && canSleepUDS)
        {
            vehicleState_data.state = VEHICLESTATE_SLEEP;
        }
    }
    else if (app_vehicleState_sleeping())
    {
        app_vehicleState_init();
    }
#endif // FDEFS_MODE_LEADER
}

/**
 * @brief Get current vehicle state
 * @return the current vehicle state
 */
app_vehicleState_state_E app_vehicleState_getState(void)
{
    return vehicleState_data.state;
}

/**
 * @brief Delay this controller from allowing sleep for atleast the next amount of time
 * @note If the timer is sleeping for more time than this already, do nothing
 * @param ms The amount of ms to sleep for
 */
void app_vehicleState_delaySleep(uint32_t ms)
{
    const uint32_t timerEnd = drv_timer_getEndTimeMS(&vehicleState_data.sleepTimeout);
    const uint32_t now = HW_TIM_getTimeMS();

    if (timerEnd < (now + ms))
    {
        drv_timer_start(&vehicleState_data.sleepTimeout, ms);
    }
}

bool app_vehicleState_sleeping(void)
{
    return app_vehicleState_getState() == VEHICLESTATE_SLEEP;
}

/**
 * @brief Get current sleepable state of followers state
 * @return the current sleepable state
 */
CAN_sleepFollowerState_E app_vehicleState_getSleepableStateCAN(void)
{
    return translateToCANSleepableState(vehicleState_data.sleepState);
}

#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
/**
 * @brief Get current vehicle state
 * @note Only the leader should be able to transmit messages over CAN
 * @return the current vehicle state
 */
CAN_vehicleState_E app_vehicleState_getStateCAN(void)
{
    return translateToCANState(vehicleState_data.state);
}
#endif

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S app_vehicleState_desc = {
    .moduleInit = &app_vehicleState_init,
    .periodic100Hz_CLK = &app_vehicleState_run100Hz,
};
