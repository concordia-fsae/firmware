/*
 * HW.c
 * Miscellaneous low-level content that doesn't warrant a dedicated file
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN.h"
#include "HW.h"
#include "Utilities.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void bkp10Write(uint16_t value)
{
    // Enable clocks for the backup domain registers
    pRCC->APB1ENR |= (RCC_APB1ENR_PWR_CLK | RCC_APB1ENR_BKP_CLK);

    // Disable backup register write protection
    pPWR->CR      |= PWR_CR_DBP;

    // store value in pBK DR10
    pBKP->DR10     = value;

    // Re-enable backup register write protection
    pPWR->CR      &= ~PWR_CR_DBP;
}

