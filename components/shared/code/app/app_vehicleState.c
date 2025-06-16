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

#define BOOT_TIME_MS 100U
#define VEHICLESTATE_TIMEOUT_MS 1000U

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

struct
{
    app_vehicleState_state_E state;
    drv_timer_S              timer;
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
        default:
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
    drv_timer_init(&vehicleState_data.timer);
#if FEATURE_VEHICLESTATE_MODE == FDEFS_MODE_LEADER
    drv_timer_start(&vehicleState_data.timer, BOOT_TIME_MS);
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
            if (drv_timer_getState(&vehicleState_data.timer) == DRV_TIMER_EXPIRED)
            {
                vehicleState_data.state = VEHICLESTATE_ON_GLV;
                drv_timer_stop(&vehicleState_data.timer);
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

                if (drv_inputAD_getDigitalActiveState(VEHICLESTATE_INPUTAD_TSMS) == DRV_IO_INACTIVE)
                {
                    vehicleState_data.state = VEHICLESTATE_ON_GLV;
                }
                else if ((VEHICLESTATE_CANRX_CONTACTORSTATE(&contacts) != CANRX_MESSAGE_VALID) ||
                         (contacts != CAN_PRECHARGECONTACTORSTATE_HVP_CLOSED))
                {
                    break;
                }
                else if (drv_inputAD_getDigitalActiveState(VEHICLESTATE_INPUTAD_RUN_BUTTON) == DRV_IO_ACTIVE)
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
    }
#else
    CAN_vehicleState_E state = translateToCANState(vehicleState_data.state);

    if (VEHICLESTATE_CANRX_SIGNAL(&state) == CANRX_MESSAGE_VALID)
    {
        drv_timer_stop(&vehicleState_data.timer);

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
        }
    }
    else if (drv_timer_getState(&vehicleState_data.timer) == DRV_TIMER_STOPPED)
    {
        drv_timer_start(&vehicleState_data.timer, VEHICLESTATE_TIMEOUT_MS);
    }
    else if (drv_timer_getState(&vehicleState_data.timer) == DRV_TIMER_EXPIRED)
    {
        vehicleState_data.state = VEHICLESTATE_ON_GLV;
        drv_timer_stop(&vehicleState_data.timer);
    }
#endif
}

/**
 * @brief Get current vehicle state
 * @return the current vehicle state
 */
app_vehicleState_state_E app_vehicleState_getState(void)
{
    return vehicleState_data.state;
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
