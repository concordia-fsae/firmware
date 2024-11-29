/*
 * UDS.c
 * UDS module implementation
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "FeatureDefines_generated.h"
#if FEATURE_IS_ENABLED(APP_UDS)

#ifndef ISO_TP_USER_DEBUG_ENABLED
#define ISO_TP_USER_DEBUG_ENABLED 0U
#endif

// module include
#include "UDS.h"

// other includes
#include "CAN/CanTypes.h"
#include "FreeRTOS.h"
#include "HW.h"
#include "HW_can.h"
#include "ModuleDesc.h"
#include "task.h"
#include "uds.h"
#include "Utility.h"
#include "LIB_app.h"

// system includes
#include <string.h>


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern uint16_t isotp_user_send_can(const uint32_t id, const uint8_t data[], const uint8_t len);
extern uint32_t isotp_user_get_ms(void);
#if ISO_TP_USER_DEBUG_ENABLED
extern void     isotp_user_debug(const char* message, ...);
#endif


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

// typedef struct
// {
// } uds_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

// static uds_S uds;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * UDS_init
 * @brief initialize the UDS module
 */
static void UDS_init(void)
{
    udsSrv_init();    // initialize the UDS library
}

/*
 * UDS_periodic
 * @brief periodic UDS function. Needs to be called at 1kHz
 */
static void UDS_periodic_1kHz(void)
{
    udsSrv_periodic();
}


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

// module description
// TODO: right now there are no UDS operations that will really block for anything.
// If ever that changes, UDS should probably get its own dedicated task
const ModuleDesc_S UDS_desc = {
    .moduleInit       = &UDS_init,
    .periodic1kHz_CLK = &UDS_periodic_1kHz,
};


/******************************************************************************
 *                U D S   L I B R A R Y   C A L L B A C K S
 ******************************************************************************/

/*
 * uds_cb_ecuReset
 * @brief callback for uds ecu reset service
 */
void uds_cb_ecuReset(udsResetType_E resetType)
{
    switch (resetType)
    {
        case UDS_RESET_TYPE_HARD:
            // have to send positive response from here since this function
            // won't return in this case
            uds_sendPositiveResponse(UDS_SID_ECU_RESET, resetType, NULL, 0U);

            // one day we can have an actual check here to see if the ecu is ready to reset
            // i.e. check eeprom writes finished, etc.
            vTaskDelay(pdMS_TO_TICKS(10U));

            // Reset
            // FIXME: move this to a function somewhere
            {
                HW_systemHardReset();

                // loop while we wait for the reset to happen
                while (1)
                {
                    __asm__ volatile ("nop");
                }
            }
            break;

        case UDS_RESET_TYPE_KEY:
        case UDS_RESET_TYPE_SOFT:
        case UDS_RESET_RAPID_SHUTDOWN_ENABLE:
        case UDS_RESET_RAPID_SHUTDOWN_DISABLE:
            // not implemented
            uds_sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
            break;

        default:
            uds_sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_GENERAL_REJECT);
            break;
    }
}


/*
 * uds_cb_routineControl
 * @brief callback for uds routine control service
 */
void uds_cb_routineControl(udsRoutineControlType_E routineControlType, uint8_t *payload, uint8_t payloadLengthBytes)
{
    UNUSED(routineControlType);
    if (payloadLengthBytes < 2U)
    {
        uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_INVALID_LEN_FORMAT);
        return;
    }

    union
    {
        uint8_t  u8[2U];
        uint16_t u16;
    } routineId;

    memcpy(routineId.u8, payload, 2);

    switch (routineId.u16)
    {
        default:
            uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_SERVICE_NOT_SUPPORTED);
            break;
    }
}


/*
 * uds_cb_DIDRead
 * @brief callback for a data ID read
 */
void uds_cb_DIDRead(uint8_t *payload, uint8_t payloadLengthBytes)
{
    if (payloadLengthBytes != 2U)
    {
        uds_sendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_INVALID_LEN_FORMAT);
        return;
    }

    union
    {
        uint8_t  u8[2U];
        uint16_t u16;
    } did;

    memcpy(did.u8, payload, 2);

    switch (did.u16)
    {
        case 0x00:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&appDesc.appStart, sizeof(appDesc.appStart));
            break;
        }
        case 0x01:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&appDesc.appEnd, sizeof(appDesc.appEnd));
            break;
        }
        case 0x02:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&appDesc.appCrcLocation, sizeof(appDesc.appCrcLocation));
            break;
        }
        case 0x03:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&(*((uint32_t*)appDesc.appCrcLocation)), sizeof(*((uint32_t*)appDesc.appCrcLocation)));
            break;
        }
        case 0x04:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&appDesc.appComponentId, sizeof(appDesc.appComponentId));
            break;
        }
        case 0x05:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&appDesc.appPcbaId, sizeof(appDesc.appPcbaId));
            break;
        }
        case 0x06:
        {
            extern const lib_app_appDesc_S appDesc;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, (uint8_t*)&appDesc.appNodeId, sizeof(appDesc.appNodeId));
            break;
        }
        case 0x101:
        {
            // always respond with 0x01 since we're in the app
            uint8_t data = 0x01;
            uds_sendPositiveResponse(UDS_SID_READ_DID, UDS_NRC_NONE, &data, 1);
            break;
        }

        default:
            uds_sendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_GENERAL_REJECT);
            break;
    }
}


/******************************************************************************
 *                U D S   L I B R A R Y   F U N C T I O N S
 ******************************************************************************/
// define functions required for isotp library

uint16_t isotp_user_send_can(const uint32_t id, const uint8_t data[], const uint8_t len)
{
    CAN_data_T d;
    bool sent = false;

    memcpy(&d, data, len);

    for (CAN_TxMailbox_E mailbox = 0U; mailbox < CAN_TX_MAILBOX_COUNT; mailbox++)
    {
        if (CAN_sendMsg(CAN_BUS_VEH, mailbox, d, (uint16_t)id, len))
        {
            sent = true;
        }
    }
    return sent;
}


uint32_t isotp_user_get_ms(void)
{
    return xTaskGetTickCount();
}


#if ISO_TP_USER_DEBUG_ENABLED
extern void isotp_user_debug(const char* message, ...);
void        isotp_user_debug(const char* message, ...)
{
    UNUSED(message);
}
#endif
#endif // fEATURE_UDS
