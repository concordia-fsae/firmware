/*
 * main.c
 * Contains program entrypoint and main loop
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN.h"
#include "HW.h"
#include "uds.h"
#include "UDS.h"
#include "Utilities.h"

#include <string.h>    // memset


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

appDesc_S *appDesc;


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/*
 * 1kHz periodic function
 */
static void periodic_1kHz(void)
{
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x200;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 0U;
    CAN_sendMsg(msg);

    UDS_periodic_1kHz();
}


/*
 * 100Hz periodic function
 */
static void periodic_100Hz(void)
{
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x201;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 1U;
    CAN_sendMsg(msg);
}


/*
 * 10Hz periodic function
 */
static void periodic_10Hz(void)
{
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x202;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 2U;
    CAN_sendMsg(msg);
}


/*
 * 1Hz periodic function
 */
static void periodic_1Hz(void)
{
    CAN_TxMessage_S msg = { 0U };
    msg.id          = 0x203;
    msg.lengthBytes = 8;
    msg.data.u64    = 0x8000000000000001ULL;
    msg.mailbox     = 2U;
    CAN_sendMsg(msg);

    static bool led = true;
    GPIO_assignPin(LED_PORT, LED_PIN, led);
    led = !led;
}


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
        case RESET_TYPE_JUST_UPDATED:
            // try to boot fast if we just updated the app
            break;

        case RESET_TYPE_PERSISTENT_BOOTLOADER:
            doBoot = false;
            break;

        case RESET_TYPE_NONE:
        default:
            if (!SYS_checkAppValid(appDesc) || readButtonState())
            {
                // TODO: throw an error on CAN here
                CAN_TxMessage_S msg = { 0U };
                msg.id          = 0x401;
                msg.lengthBytes = 3;
                msg.mailbox     = 2;
                msg.data.u64    = 0ULL;

                msg.data.u8[0] = 0xFF;
                msg.data.u8[1] = (uint8_t)SYS_checkAppValid(appDesc);
                msg.data.u8[2] = (uint8_t)readButtonState();

                CAN_sendMsg(msg);

                doBoot = false;
            }
            break;
    }


    if (doBoot)
    {
        {
            if (SYS_checkAppValid(appDesc))
            {
                SYS_bootApp(appDesc->appStart);
            }
            else
            {
                // NO valid app to execute
                // TODO: throw error on CAN here with boot failure reason
                GPIO_strobePin(LED_PORT, LED_PIN, 5, BLINK_SLOW, LED_ON_STATE);
                SYS_resetHard();
            }
        }
    }
    else
    {
        // TODO: throw error on CAN here with boot failure reason
        // SYS_resetHard();
    }
}


int main(void)
{
    SYS_init();

    appDesc = (appDesc_S*)(APP_FLASH_START);

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

            // try to boot if UDS is not inhibiting it
            if (!UDS_shouldInhibitBoot())
            {
                tryBoot();
            }
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
