/**
 * @file brakeLight.c
 * @brief Module source that manages the brake light
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "brakeLight.h"
#include "LIB_Types.h"
#include "Module.h"
#include "ModuleDesc.h"
#include "string.h"
#include "MessageUnpack_generated.h"
#include "drv_outputAD.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    brakeLight_state_E state;
} brake_data;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool brakeLight_isOn(void)
{
    return brake_data.state != BRAKELIGHT_OFF;
}

brakeLight_state_E brakeLight_getState(void)
{
    return brake_data.state;
}

CAN_brakeLightState_E brakeLight_getStateCAN(void)
{
    CAN_brakeLightState_E ret = CAN_BRAKELIGHTSTATE_SNA;

    switch (brake_data.state)
    {
        case BRAKELIGHT_ON:
            ret = CAN_BRAKELIGHTSTATE_ON;
            break;
        case BRAKELIGHT_OFF:
            ret = CAN_BRAKELIGHTSTATE_OFF;
            break;
        case BRAKELIGHT_FAULT:
            ret = CAN_BRAKELIGHTSTATE_FAULT;
            break;
        default:
            break;
    }

    return ret;
}

static void brakeLight_init(void)
{
    memset(&brake_data, 0x00U, sizeof(brake_data));

    brake_data.state = BRAKELIGHT_OFF;
}

static void brakeLight_periodic_10Hz(void)
{
    float32_t percentage = 0U; // Can percentage 0.0f - 100.0f

    if (CANRX_get_signal(VEH, VCFRONT_brakePosition, &percentage) == CANRX_MESSAGE_VALID)
    {
        if (percentage > 10.0f)
        {
            brake_data.state = BRAKELIGHT_ON;
        }
        else
        {
            brake_data.state = BRAKELIGHT_OFF;
        }
    }
    else
    {
        brake_data.state = BRAKELIGHT_FAULT;
    }

    switch (brake_data.state)
    {
        case BRAKELIGHT_ON:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BR_LIGHT_EN, DRV_IO_ACTIVE);
            break;
        case BRAKELIGHT_OFF:
            drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_BR_LIGHT_EN, DRV_IO_INACTIVE);
            break;
        case BRAKELIGHT_FAULT:
        default:
            drv_outputAD_toggleDigitalState(DRV_OUTPUTAD_DIGITAL_BR_LIGHT_EN);
            break;
    }
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S brakeLight_desc = {
    .moduleInit = &brakeLight_init,
    .periodic10Hz_CLK = &brakeLight_periodic_10Hz,
};
