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
#include "SystemConfig.h"
#include "Utility.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

const HW_GPIO_Input_S inputs[NUM_INPUTS] = {
    [SW1]           = { .port = SW1_GPIO_Port, .pin = SW1_Pin },
    [SW2]           = { .port = SW2_GPIO_Port, .pin = SW2_Pin },
    [SW4]           = { .port = SW4_GPIO_Port, .pin = SW4_Pin },
    [SW5]           = { .port = SW5_GPIO_Port, .pin = SW5_Pin },
    [BTN_TOP_LEFT]  = { .port = BTN_TOP_LEFT_GPIO_Port, .pin = BTN_TOP_LEFT_Pin },
    [BTN_MIDDLE]    = { .port = BTN_MIDDLE_GPIO_Port, .pin = BTN_MIDDLE_Pin },
    [BTN_TOP_RIGHT] = { .port = BTN_TOP_RIGHT_GPIO_Port, .pin = BTN_TOP_RIGHT_Pin }
};


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief Initializes the Digital IO Init
 */
static void IO_Digital_init(void)
{
    memset(&IO_DIGITAL, 0x00, sizeof(IO_Digital_S));
}

/**
 * @brief Periodic 10Hz function checking input states
 */
static void IO_Digital_10Hz_PRD(void)
{
    /**< Because of pull-up, signal is active low */
    for (digitalInput_E i = (digitalInput_E)0U; i < NUM_INPUTS; i++)
    {
        if (HW_GPIO_ReadPin(&inputs[i]) == HW_PIN_RESET)
        {
            IO_DIGITAL.inputState |= 0x01 << (unsigned int)i;
        }
        else
        {
            IO_DIGITAL.inputState &= ~(0x01 << (unsigned int)i);
        }
    }
}

const ModuleDesc_S IO_Digital_desc = {
    .moduleInit       = &IO_Digital_init,
    .periodic10Hz_CLK = &IO_Digital_10Hz_PRD,
};
