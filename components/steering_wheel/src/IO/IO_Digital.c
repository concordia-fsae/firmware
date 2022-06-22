/**
 * IO_Digital.c
 * Digital IO Module source code
 * @note To transfer this module to another system, the initialized inputs
 *      of IO_DIGITAL must be modified to be congruent to the new system
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Module Header
#include "IO/IO_Digital.h"

// System includes
#include <string.h>

// Other includes
#include "ModuleDesc.h"
#include "Utility.h"
#include "SystemConfig.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

IO_Digital_S IO_DIGITAL = {
    .inputs[SW1] = { .gpiox = SW1_GPIO_Port, .pin = SW1_Pin },
    .inputs[SW2] = { .gpiox = SW2_GPIO_Port, .pin = SW2_Pin },
    .inputs[SW4] = { .gpiox = SW4_GPIO_Port, .pin = SW4_Pin },
    .inputs[SW5] = { .gpiox = SW5_GPIO_Port, .pin = SW5_Pin },
    .inputs[BTN_TOP_LEFT] = { .gpiox = BTN_TOP_LEFT_GPIO_Port, .pin = BTN_TOP_LEFT_Pin },
    .inputs[BTN_MIDDLE] = { .gpiox = BTN_MIDDLE_GPIO_Port, .pin = BTN_MIDDLE_Pin },
    .inputs[BTN_TOP_RIGHT] = { .gpiox = BTN_TOP_RIGHT_GPIO_Port, .pin = BTN_TOP_RIGHT_Pin }
}; /**< Must Initialize amount of IO_Digital_Input_S equal to NUM_INPUTS */


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes the Digital IO Init
 */
static void IO_Digital_init(void)
{
    memset(&IO_DIGITAL.inputState, 0x00, sizeof(IO_Digital_S) - sizeof(IO_Digital_Input_S)*NUM_INPUTS);
}

/**
 * @brief Periodic 10Hz function checking input states
 */
static void IO_Digital_10Hz_PRD(void)
{
    /**< Because of pull-up, signal is active low */
    for (digitalInput_E i = (digitalInput_E) 0U; i < NUM_INPUTS; i++)
    {
        if (HAL_GPIO_ReadPin(IO_DIGITAL.inputs[i].gpiox, IO_DIGITAL.inputs[i].pin) == GPIO_PIN_RESET)
        {
            IO_DIGITAL.inputState |= 0x01 << (unsigned int) i;
        }
        else
        {
            IO_DIGITAL.inputState &= ~(0x01 << (unsigned int) i);
        }
    }
}

const ModuleDesc_S IO_Digital_desc = {
    .moduleInit       = &IO_Digital_init,
    .periodic10Hz_CLK = &IO_Digital_10Hz_PRD,
};
