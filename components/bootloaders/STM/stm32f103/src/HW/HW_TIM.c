/*
 * HW_TIM.c
 * This file describes low-level, mostly hardware-specific Timer behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_TIM.h"
#include "Types.h"
#include "Utilities.h"

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static Time_t timeMs;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

// defined in c_only_startup.S
// part of the interrupt vector table
extern void TIM2_IRQHandler(void);


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

void TIM_init(void)
{
    // enable TIM2 clock
    SET_BIT(pRCC->APB1ENR, RCC_APB1ENR_TIM2_CLK);

    // put the timer in down-counting mode
    SET_REG(TIM2_CR1, GET_REG(TIM2_CR1) | TIM2_CR1_DIR);

    // enable the update interrupt
    SET_REG(TIM2_DIER, GET_REG(TIM2_DIER) | TIM2_DIER_UIE);

    // TIM2 is clocked by APB1x2 which is running at 8MHz x 2 = 16MHz
    // we want an interrupt every 1ms, so set the counter start value to 16000
    // (16MHz / 16000 = 1000 interrupts/s or 1 interrupt/ms)
    SET_REG(TIM2_ARR, 16000U);

    // enable the timer
    SET_REG(TIM2_CR1, GET_REG(TIM2_CR1) | TIM2_CR1_CE);

    // set interrupt priority
    NVIC_SetPriority(TIM2_IRQn, 4U, 0U);

    // enable the interrupt
    NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM_destroy(void)
{
    // enable the update interrupt
    SET_REG(RCC_APB1RSTR, GET_REG(RCC_APB1RSTR) | 1U);
    BLOCKING_DELAY(1000);
    SET_REG(RCC_APB1RSTR, GET_REG(RCC_APB1RSTR) & 0xFEUL);
    BLOCKING_DELAY(1000);
}

Time_t TIM_getTimeMs(void)
{
    return timeMs;
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/*
 * TIM2_IRQHandler
 * @brief interrupt handler for TIM2. Increment the global timer var and clear
 *        the interrupt flag
 */
void TIM2_IRQHandler(void)
{
    timeMs++;
    SET_REG(TIM2_SR, GET_REG(TIM2_SR) & ~TIM2_SR_UIF);
}
