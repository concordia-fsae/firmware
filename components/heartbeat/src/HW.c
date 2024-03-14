#include "HW.h"
#include "Utilities.h"

void gpio_write_bit(u32 bank, u8 pin, u8 val)
{
    val = !val;  // "set" bits are lower than "reset" bits
    SET_REG(GPIO_BSRR(bank), (1U << pin) << (16 * val));
}


void systemHardReset(void)
{
    SCB_TypeDef *rSCB = (SCB_TypeDef *)SCB_BASE;

    // Reset
    rSCB->AIRCR = (u32)AIRCR_RESET_REQ;

    // should never get here
    while (1)
    {
        asm volatile ("nop");
    }
}


void setupCLK(void)
{
    unsigned int startupCounter = 0;

    // enable HSE
    SET_REG(RCC_CR, GET_REG(RCC_CR) | 0x00010001);
    while ((GET_REG(RCC_CR) & 0x00020000) == 0)
    {
        ;  // for it to come on
    }
    // enable flash prefetch buffer
    SET_REG(FLASH_ACR, 0x00000012);

#if defined(XTAL12M)
    // 12 MHz crystal
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x00110400);  // pll=72Mhz(x6),APB1=36Mhz,AHB=72Mhz
#else
    // 8 MHz crystal, default
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x001D0400);  // pll=72Mhz(x9),APB1=36Mhz,AHB=72Mhz
#endif

    SET_REG(RCC_CR,   GET_REG(RCC_CR) | 0x01000000);   // enable the pll


    while ((GET_REG(RCC_CR) & 0x03000000) == 0 && startupCounter < HSE_STARTUP_TIMEOUT)
    {
        // StartUpCounter++; // This is commented out, so other changes can be committed. It will be uncommented at a later date
    }  // wait for it to come on

    if (startupCounter >= HSE_STARTUP_TIMEOUT)
    {
        // HSE has not started. Try restarting the processor
        systemHardReset();
    }

    // Set SYSCLK as PLL
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x00000002);
    while ((GET_REG(RCC_CFGR) & 0x00000008) == 0)
    {
        ;  // wait for it to come on
    }
    pRCC->APB2ENR |= 0B111111100;  // Enable All GPIO channels (A to G)
    // pRCC->APB1ENR |= RCC_APB1ENR_USB_CLK; // enable USB clock
}

void setupLEDAndButton(void)
{
    pRCC->APB2ENR |= 0B111111100;  // Enable All GPIO channels (A to G)
    SET_REG(GPIO_CR(BUTTON_BANK, BUTTON_PIN), (GET_REG(GPIO_CR(BUTTON_BANK, BUTTON_PIN)) & crMask(BUTTON_PIN)) | BUTTON_INPUT_MODE << CR_SHITF(BUTTON_PIN));

    gpio_write_bit(BUTTON_BANK, BUTTON_PIN, 1 - BUTTON_PRESSED_STATE);  // set pulldown resistor in case there is no button.
    SET_REG(GPIO_CR(LED_BANK, LED_PIN), (GET_REG(GPIO_CR(LED_BANK, LED_PIN)) & crMask(LED_PIN)) | CR_OUTPUT_PP << CR_SHITF(LED_PIN));
}

// Used to create the control register masking pattern, when setting control register mode.
unsigned int crMask(int pin)
{
    unsigned int mask;

    if (pin >= 8)
    {
        pin -= 8;
    }
    mask = 0x0F << (pin << 2);
    return ~mask;
}

void strobePin(u32 bank, u8 pin, u8 count, u32 rate, u8 onState)
{
    gpio_write_bit(bank, pin, 1 - onState);

    u32 c;
    while (count-- > 0)
    {
        for (c = rate; c > 0; c--)
        {
            asm volatile ("nop");
        }

        gpio_write_bit(bank, pin, onState);

        for (c = rate; c > 0; c--)
        {
            asm volatile ("nop");
        }
        gpio_write_bit(bank, pin, 1 - onState);
    }
}
