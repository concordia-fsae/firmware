/**
 * @file HW_NX3LPW.c
 * @brief  Source code for  NX3L4051PW Driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_NX3L4051PW.h"
#include "HW_gpio.h"

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes NX3L chip
 *
 * @retval true = Success, false = Failure
 */
bool NX3L_init(void)
{
    return true;
}

/**
 * @brief Select a specific input from the NX3L chip
 *
 * @param chn Channel to select
 *
 * @retval true = Success, false = Failure
 */
bool NX3L_setMux(NX3L_MUXChannel_E chn)
{
    HW_GPIO_writePin(HW_GPIO_MUX_SEL1, (chn & 0x01) ? true : false);
    HW_GPIO_writePin(HW_GPIO_MUX_SEL2, (chn & (0x01 << 1)) ? true : false);
    HW_GPIO_writePin(HW_GPIO_MUX_SEL3, (chn & (0x01 << 2)) ? true : false);
    return true;
}

/**
 * @brief  Enables the multiplexer output to MCU
 *
 * @retval true = Success, false = Failure
 */
bool NX3L_enableMux(void)
{
    HW_GPIO_writePin(HW_GPIO_NX3_NEN, false);
    return true;
}

/**
 * @brief  Disables the multiplexer output to MCU
 *
 * @retval true = Success, false = Failure
 */
bool NX3L_disableMux(void)
{
    HW_GPIO_writePin(HW_GPIO_NX3_NEN, true);
    return true;
}
