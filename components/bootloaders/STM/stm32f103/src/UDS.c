/*
 * UDS.c
 * UDS module implementation
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "UDS.h"

// other includes
#include "HW.h"
#include "HW_FLASH.h"
#include "Types.h"
#include "uds.h"
#include "Utilities.h"

#include <string.h>

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t appBootTimer;    // [ms] countdown timer until app will boot,
                              // unless inhibited by UDS
    struct
    {
        bool udsTimoutExpired:1;
    } bit;
} uds_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static uds_S uds;


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void checkTimeoutExpired_1kHz(void)
{
    if ((udsSrv_timeSinceLastTp() < 5U)
        || (udsSrv_getCurrentSession() != UDS_SESSION_TYPE_DEFAULT)
        )
    {
        uds.appBootTimer         = UDS_INACTIVITY_TIMER;
        uds.bit.udsTimoutExpired = false;
    }
    else if (uds.appBootTimer == 0U)
    {
        uds.bit.udsTimoutExpired = true;
    }
    else
    {
        uds.appBootTimer -= (uds.appBootTimer > 0U) ? 1U : 0U;
    }
}


static bool routine_0xf00f(udsRoutineControlType_E routineControlType, uint8_t *payload, uint8_t payloadLengthBytes)
{
    UNUSED(payloadLengthBytes);
    UNUSED(payload);

    bool result;

    switch(routineControlType)
    {
        case UDS_ROUTINE_CONTROL_START:
            result = FLASH_eraseApp();
            break;
        case UDS_ROUTINE_CONTROL_GET_RESULT:
            break;
        case UDS_ROUTINE_CONTROL_STOP:
        case UDS_ROUTINE_CONTROL_NONE:
        default:
            result = false;
            break;
    }

    return result;
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * UDS_shouldInhibitBoot
 * @brief whether the UDS module thinks boot should be inhibited or not
 *        (i.e. whether there is UDS active)
 */
bool UDS_shouldInhibitBoot(void)
{
    return !uds.bit.udsTimoutExpired;
}

/*
 * UDS_init
 * @brief initialize the UDS module
 */
void UDS_init(void)
{
    udsSrv_init();    // initialize the UDS library
    uds.appBootTimer = UDS_STARTUP_WAIT;
}

/*
 * UDS_periodic
 * @brief periodic UDS function. Needs to be called at 1kHz
 */
void UDS_periodic_1kHz(void)
{
    udsSrv_periodic();
    checkTimeoutExpired_1kHz();
}


/******************************************************************************
 *                U D S   L I B R A R Y   C A L L B A C K S
 ******************************************************************************/

/*
 * uds_cb_ecuReset
 * @brief callback for uds ecu reset service
 */
udsNegativeResponse_E uds_cb_ecuReset(udsResetType_E resetType)
{
    udsNegativeResponse_E resp = UDS_NRC_GENERAL_REJECT;

    switch (resetType)
    {
        case UDS_RESET_TYPE_HARD:
            // have to send positive response from here since this function
            // won't return in this case
            uds_sendPositiveResponse(UDS_SID_ECU_RESET, resetType, NULL, 0U);

            // one day we can have an actual check here to see if the ecu is ready to reset
            // i.e. check eeprom writes finished, etc.
            BLOCKING_DELAY(1000);
            SYS_resetHard();
            break;

        case UDS_RESET_TYPE_KEY:
        case UDS_RESET_TYPE_SOFT:
        case UDS_RESET_RAPID_SHUTDOWN_ENABLE:
        case UDS_RESET_RAPID_SHUTDOWN_DISABLE:
            // not implemented
            resp = UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
            break;

        default:
            resp = UDS_NRC_GENERAL_REJECT;
            break;
    }

    return resp;
}


/*
 * uds_cb_routineControl
 * @brief callback for uds routine control service
 */
udsNegativeResponse_E uds_cb_routineControl(udsRoutineControlType_E routineControlType, uint8_t *payload, uint8_t payloadLengthBytes)
{
    uint16_t routineId = (payload[0] << 8U) | payload[1];

    switch (routineId)
    {
        case 0xf00f:
            routine_0xf00f(routineControlType, payload, payloadLengthBytes);
            break;
        default:
            break;
    }


    return UDS_NRC_NONE;
}


/******************************************************************************
 *                U D S   L I B R A R Y   F U N C T I O N S
 ******************************************************************************/
// define functions required for isotp library

extern uint16_t isotp_user_send_can(const uint32_t id, const uint8_t data[], const uint8_t len);
uint16_t        isotp_user_send_can(const uint32_t id, const uint8_t data[], const uint8_t len)
{
    CAN_TxMessage_S msg = {
        .id          = id,
        .lengthBytes = len,
        .data        = { 0U },
    };

    memcpy(&msg.data, data, len);

    return CAN_sendMsg(msg);
}


extern void isotp_user_debug(const char* message, ...);
void        isotp_user_debug(const char* message, ...)
{
    UNUSED(message);
}


extern uint32_t isotp_user_get_ms(void);
extern uint32_t isotp_user_get_ms(void)
{
    return (uint32_t)TIM_getTimeMs();
}
