/*
 * main.c
 * Contains program entrypoint and main loop
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN.h"
#include "HW.h"
#include "lib_uds.h"
#include "UDS.h"
#include "Utilities.h"

#include <string.h>    // memset
#include "FeatureDefines_generated.h"
#include "LIB_app.h"
#include "HW_FLASH.h"


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/*
 * 1kHz periodic function
 */
static void periodic_1kHz(void)
{
#if FEATURE_IS_ENABLED(FEATURE_CAN_DEBUG)
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x200;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 0U;
    CAN_sendMsg(msg);
#endif // FEATURE_CAN_DEBUG
    UDS_periodic_1kHz();
}


/*
 * 100Hz periodic function
 */
static void periodic_100Hz(void)
{
#if FEATURE_IS_ENABLED(FEATURE_CAN_DEBUG)
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x201;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 1U;
    CAN_sendMsg(msg);
#endif // FEATURE_CAN_DEBUG
}


/*
 * 10Hz periodic function
 */
static void periodic_10Hz(void)
{
    extern lib_app_appDesc_S hwDesc;
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x299;
    msg.lengthBytes = 8U;
    msg.data.u16[0] = hwDesc.appComponentId;
    msg.data.u16[1] = hwDesc.appPcbaId;
    msg.data.u8[4]  = APP_FUNCTION_ID;
    msg.data.u8[5]  = SYS_checkAppValid(APP_DESC_ADDR);
#if FEATURE_IS_ENABLED(APP_NODE_ID)
    msg.data.u8[6]  = hwDesc.appNodeId;
#endif // APP_NODE_ID
    msg.mailbox     = 2U;
    CAN_sendMsg(msg);

#if FEATURE_IS_ENABLED(FEATURE_CAN_DEBUG)
    msg.id          = 0x202;
    msg.lengthBytes = 8U;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 2U;
    CAN_sendMsg(msg);
#endif // FEATURE_CAN_DEBUG
}


/*
 * 1Hz periodic function
 */
static void periodic_1Hz(void)
{
#if FEATURE_IS_ENABLED(FEATURE_CAN_DEBUG)
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x203;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 2U;
    CAN_sendMsg(msg);
#endif // FEATURE_CAN_DEBUG

    static bool led = true;
    GPIO_assignPin(LED_PORT, LED_PIN, led);
    led = !led;
}


#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BL
/*
 * tryBoot
 * @brief try to boot the app, return to periodic loop if it fails
 *        to boot or is otherwise inhibited
 */
static void tryBoot(void)
{
    bool doBoot = true;

    switch (SYS_getResetType())
    {
        case RESET_TYPE_PERSISTENT_BOOTLOADER:
            doBoot = false;
            break;

        case RESET_TYPE_JUST_UPDATED:
        case RESET_TYPE_NONE:
        default:
            break;
    }

    if (doBoot)
    {
        bool appValid = SYS_checkAppValid(APP_DESC_ADDR);
        if (appValid)
        {
            SYS_bootApp(APP_DESC_ADDR->appStart);
        }
        else
        {
#if FEATURE_IS_ENABLED(FEATURE_CAN_DEBUG)
            CAN_TxMessage_S msg = { 0U };
            msg.id          = 0x401;
            msg.lengthBytes = 2;
            msg.mailbox     = 2;
            msg.data.u64    = 0ULL;

            msg.data.u8[0] = 0xFF;
            msg.data.u8[1] |= appValid ? 1U : 0U;

            CAN_sendMsg(msg);
#endif // FEATURE_CAN_DEBUG
        }
    }
}
#endif // FUNCTION_ID_BL


int main(void)
{
    SYS_init();

    for (;;)
    {
        static uint16_t timerMs  = 1U;
        static uint32_t lastTick = 0U;
        uint32_t        tick     = TIM_getTimeMs();

        if (lastTick != tick)
        {
            lastTick = tick;

            periodic_1kHz();

            if (timerMs % 10U == 0U)
            {
                periodic_100Hz();
            }

            if (timerMs % 100U == 0U)
            {
                periodic_10Hz();
            }

            if (timerMs % 1000U == 0U)
            {
                periodic_1Hz();
            }

            if (timerMs++ == 1000U)
            {
                timerMs = 1U;
            }
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BL
            // try to boot if UDS is not inhibiting it
            if (!UDS_shouldInhibitBoot())
            {
                tryBoot();
            }
#endif // FUNCTION_ID_BL
        }

        // handle continuing the flash erase here for now since there's no rtos
        // normally we'd want there to be a dedicated task for flash and eeprom tasks like this
        FLASH_eraseState_S eraseState = FLASH_getEraseState();
        if (eraseState.started && !eraseState.completed)
        {
            FLASH_eraseAppContinue();
        }
    }

    return 0;
}
