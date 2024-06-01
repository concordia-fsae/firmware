/*
 * HW_GPIO.c
 * This file describes low-level, mostly hardware-specific GPIO behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "HW_GPIO.h"

#include "Utilities.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void GPIO_init(const GPIO_config_S mux[], uint8_t pinCount)
{
    pRCC->APB2ENR |= 0B111111100;    // Enable All GPIO clocks (A to G)

    for (uint8_t idx = 0; idx < pinCount; idx++)
    {
        const GPIO_config_S pin = mux[idx];
        GPIO_SETUP_PIN(pin);
        if ((pin.mode == GPIO_MODE_INPUT) && (pin.config == GPIO_CFG_INPUT_PULL))
        {
            SET_REG(GPIO_BSRR(pin.port), (1U << pin.pin) << (16U * (uint8_t)pin.pullDirection));
        }
    }
}


void GPIO_destroy(void)
{
    // disable all gpio clocks
    pRCC->APB2ENR |= 0B000000000;
}


void GPIO_assignPin(uint32_t bank, uint8_t pin, uint8_t val)
{
    val = !val;    // "set" bits are lower than "reset" bits
    SET_REG(GPIO_BSRR(bank), (1U << pin) << (16 * val));
}


void GPIO_strobePin(uint32_t bank, uint8_t pin, uint8_t count, uint32_t rate, uint8_t onState)
{
    GPIO_assignPin(bank, pin, 1 - onState);

    uint32_t c;
    while (count-- > 0)
    {
        for (c = rate; c > 0; c--)
        {
            asm volatile ("nop");
        }

        GPIO_assignPin(bank, pin, onState);

        for (c = rate; c > 0; c--)
        {
            asm volatile ("nop");
        }
        GPIO_assignPin(bank, pin, 1 - onState);
    }
}


bool GPIO_readPin(void)
{
    bool state = false;

    if (GET_REG(GPIO_IDR(BUTTON_PORT)) & (0x01 << BUTTON_PIN))
    {
        state = true;
    }

    if (BUTTON_PRESSED_STATE == 0)
    {
        state = !state;
    }

    return state;
}
