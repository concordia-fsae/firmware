/*
 * HW_CLK.c
 * This file describes low-level, mostly hardware-specific RCC/Clock behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_CLK.h"

#include "HW.h"
#include "Utilities.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void CLK_init(void)
{
    unsigned int startupCounter = 0;

    // enable HSE
    SET_REG(RCC_CR, GET_REG(RCC_CR) | 0x00010001);
    // wait for it to come on
    while ((GET_REG(RCC_CR) & 0x00020000) == 0)
    { }

    // enable flash prefetch buffer
    SET_REG(FLASH_ACR, 0x00000012);

#if defined(XTAL12M)
    // 12 MHz crystal
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x00110400UL);    // pll=72Mhz(x6),APB1=36Mhz,AHB=72Mhz
#else
    // 8 MHz crystal, default
    // SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x001D0400UL);  // pll=72Mhz(x9),APB1=36Mhz,AHB=72Mhz
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x00190600UL);    // pll=64Mhz(x8),APB1=8Mhz,AHB=64Mhz
#endif

    // enable the pll
    SET_REG(RCC_CR, GET_REG(RCC_CR) | 0x01000000);

    // wait for it to come on
    while ((GET_REG(RCC_CR) & 0x03000000) == 0)
    {
        if (startupCounter >= HSE_STARTUP_TIMEOUT)
        {
            // HSE has not started. Try restarting the processor
            SYS_resetHard();
        }
        startupCounter++;
    }

    // Set SYSCLK as PLL
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) | 0x00000002);
    // wait for it to come on
    while ((GET_REG(RCC_CFGR) & 0x00000008) == 0)
    { }

    // pRCC->APB1ENR |= RCC_APB1ENR_USB_CLK; // enable USB clock

    // configure the HSI oscillator
    if ((pRCC->CR & 0x01) == 0x00)
    {
        uint32_t rwmVal = pRCC->CR;
        rwmVal  |= 0x01;
        pRCC->CR = rwmVal;
    }

    // wait for it to come on
    while ((pRCC->CR & 0x02) == 0x00)
    {}
}
