/**
 * HW_intc.c
 * Hardware Interrupt Controller Implementation
 * Cortex-M3 Processor Interruption and Exception Handlers
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "stm32f1xx.h"
#include "HW_intc.h"
#include "SystemConfig.h"


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern TIM_HandleTypeDef htim4;
extern CAN_HandleTypeDef hcan;
extern TIM_HandleTypeDef htim;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
    volatile uint8_t c = 0;

    while (c == 0)
    {
    }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void)
{
}

// Peripheral Interrupt Handlers

void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(OK_HS_Pin);
}

void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(TSMS_CHG_Pin);
}

void TIM1_UP_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim);
}
void TIM1_BRK_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim);
}
void TIM1_TRG_COM_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim);
}
void TIM1_CC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim);
}

/**
 * @brief This function handles TIM4 global interrupt.
 */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}

// CAN interrupts
void CAN1_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

/**
 * CAN1_TX_IRQHandler
 *
 */
void CAN1_TX_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

/**
 * CAN1_RX0_IRQHandler
 *
 */
void CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

/**
 * CAN1_RX1_IRQHandler
 *
 */
void CAN1_RX1_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}
