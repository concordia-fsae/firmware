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
#include "CAN/CanTypes.h"
#include "FreeRTOS.h"
#include "HW_can.h"
#include "ModuleDesc.h"
#include "task.h"
#include "Types.h"
#include "uds.h"
#include "Utility.h"

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

// FIXME: move this somewhere else
typedef struct
{
    volatile uint32_t const CPUID;
    volatile uint32_t       ICSR;
    volatile uint32_t       VTOR;
    volatile uint32_t       AIRCR;
    volatile uint32_t       SCR;
    volatile uint32_t       CCR;
    volatile uint32_t       SHPR[3];
    volatile uint32_t       SHCSR;
    volatile uint32_t       CFSR;
    volatile uint32_t       HFSR;
    volatile uint32_t       DFSR;
    volatile uint32_t       MMFAR;
    volatile uint32_t       BFAR;
    volatile uint32_t       AFSR;
} SCB_regMap;
#define pSCB               ((SCB_regMap*)SCB_BASE)

#define AIRCR_RESET        (0x05FA0000UL)
#define AIRCR_RESET_REQ    (AIRCR_RESET | 0x04UL)

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
            vTaskDelay(pdMS_TO_TICKS(1));

            // Reset
            // FIXME: move this to a function somewhere
            {
                pSCB->AIRCR = AIRCR_RESET_REQ;

                // loop while we wait for the reset to happen
                while (1)
                {
                    asm volatile ("nop");
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
        case 0x101:
        {
            // always respond with 0x01 since we're in the app
            uint8_t data = 0x01;
            uds_sendPositiveResponse(UDS_SID_READ_DID, 0x00, &data, 1);
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

    memcpy(&d, data, len);

    return CAN_sendMsgBus0(0, d, id, len);
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
