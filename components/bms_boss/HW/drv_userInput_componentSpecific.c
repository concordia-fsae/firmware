/**
 * @file drv_userInput.h
 * @brief Abstraction layer for various user input devices
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_userInput.h"

#define DEBOUNCE_MS 50

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

drv_userInput_configButton_S userInput_configButtons[USERINPUT_BUTTON_COUNT] = {
    [USERINPUT_SWITCH_TSMS] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_TSMS_CHG,
                .debounce_on_ms = DEBOUNCE_MS,
                .debounce_off_ms = DEBOUNCE_MS,
            },
        },
    },
};
