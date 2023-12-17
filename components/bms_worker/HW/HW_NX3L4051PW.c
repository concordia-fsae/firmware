/**
 * @file HW_NX3LPW.c
 * @brief  Source code for  NX3L4051PW Driver
 */

#if defined(BMSW_BOARD_VA3)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_gpio.h"
#include "HW_NX3L4051PW.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

#if defined(BMSW_BOARD_VA3)
HW_GPIO_S S1 = {
    .port = MUX_SEL1_Port,
    .pin  = MUX_SEL1_Pin,
};
HW_GPIO_S S2 = {
    .port = MUX_SEL2_Port,
    .pin  = MUX_SEL2_Pin,
};
HW_GPIO_S S3 = {
    .port = MUX_SEL3_Port,
    .pin  = MUX_SEL3_Pin,
};
HW_GPIO_S MUX_NEn = {
    .port = NX3_NEN_Port,
    .pin  = NX3_NEN_Pin,
};
#endif // BMSW_BOARD_VA3


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
    HW_GPIO_writePin(&S1, (chn & 0x01) ? true : false);
    HW_GPIO_writePin(&S2, (chn & (0x01 << 1)) ? true : false);
    HW_GPIO_writePin(&S3, (chn & (0x01 << 2)) ? true : false);
    return true;
}

/**
 * @brief  Enables the multiplexer output to MCU
 *
 * @retval true = Success, false = Failure
 */
bool NX3L_enableMux(void)
{
    HW_GPIO_writePin(&MUX_NEn, false);
    return true;
}

/**
 * @brief  Disables the multiplexer output to MCU
 *
 * @retval true = Success, false = Failure
 */
bool NX3L_disableMux(void)
{
    HW_GPIO_writePin(&MUX_NEn, true);
    return true;
}

#endif // BMSW_BOARD_VA3
