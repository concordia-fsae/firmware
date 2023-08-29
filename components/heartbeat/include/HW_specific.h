#pragma once

#define HSE_STARTUP_TIMEOUT    ((unsigned int)0x0500)  // !< Time out for HSE start up

#define LED_BANK        GPIOC
#define LED_PIN         13
#define LED_ON_STATE    0

// Use Boot1 PB2 as the button, as hardly anyone uses this pin as GPIO
// Need to set the button input mode to just CR_INPUT and not CR_INPUT_PU_PD because the external pullup on the jumplink is very weak
#define BUTTON_INPUT_MODE       CR_INPUT
#define BUTTON_BANK             GPIOB
#define BUTTON_PIN              2
#define BUTTON_PRESSED_STATE    1


// Speed controls for strobing the LED pin
#define BLINK_FAST    0x50000
#define BLINK_SLOW    0x100000


// startup defs
#define STARTUP_BLINKS       5
#ifndef BOOTLOADER_WAIT
# ifdef BUTTON_BANK
#  define BOOTLOADER_WAIT    6
# else
#  define BOOTLOADER_WAIT    30
# endif
#endif
