/**
 * @file drv_userInput.h
 * @brief Abstraction layer for various user input devices
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_userInput.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

drv_userInput_configButton_S userInput_configButtons[USERINPUT_BUTTON_COUNT] = {
    [USERINPUT_BUTTON_LEFT_TOP] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN1,
                .debounce_on_ms = 250,
                .debounce_off_ms = 100,
            },
        },
    },
    [USERINPUT_BUTTON_LEFT_MID] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN2,
                .debounce_on_ms = 250,
                .debounce_off_ms = 100,
            },
        },
    },
    [USERINPUT_BUTTON_LEFT_BOT] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN3,
                .debounce_on_ms = 250,
                .debounce_off_ms = 100,
            },
        },
    },
    [USERINPUT_BUTTON_RIGHT_TOP] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN4,
                .debounce_on_ms = 250,
                .debounce_off_ms = 100,
            },
        },
    },
    [USERINPUT_BUTTON_RIGHT_MID] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN5,
                .debounce_on_ms = 250,
                .debounce_off_ms = 100,
            },
        },
    },
    [USERINPUT_BUTTON_RIGHT_BOT] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN6,
                .debounce_on_ms = 250,
                .debounce_off_ms = 100,
            },
        },
    },
    [USERINPUT_BUTTON_LEFT_TOGGLE] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN7,
                .debounce_on_ms = 50,
                .debounce_off_ms = 50,
            },
        },
    },
    [USERINPUT_BUTTON_RIGHT_TOGGLE] = {
        .type = USERINPUT_TYPE_GPIO,
        .config = {
            .gpio = {
                .pin = DRV_INPUTAD_DIGITAL_CHANNEL_DIN8,
                .debounce_on_ms = 50,
                .debounce_off_ms = 50,
            },
        },
    },
};
