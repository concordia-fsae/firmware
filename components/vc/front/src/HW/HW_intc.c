/**
 * @file HW_intc.c
 * @brief  Source code for STM32F1xx Cortex M3 Interrupts
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_intc.h"
#include "NetworkDefines_generated.h"
#include "HW_tim.h"
#include "HW_uart.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;
extern TIM_HandleTypeDef htim[HW_TIM_PORT_COUNT];
extern TIM_HandleTypeDef htim_tick;
extern CAN_HandleTypeDef hcan[CAN_BUS_COUNT];
extern UART_HandleTypeDef huart[HW_UART_PORT_COUNT];


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

/**
 * @brief This function handles DMA1 channel1 global interrupt.
 */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
    //    HAL_ADC_IRQHandler(&hadc2);
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim_tick);
}

void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim[HW_TIM_PORT_WHEELSPEED]);
}

// CAN interrupts
void CAN1_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_VEH]);
}

/**
 * CAN1_TX_IRQHandler
 *
 */
void CAN1_TX_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_VEH]);
}

/**
 * CAN1_RX0_IRQHandler
 *
 */
void CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_VEH]);
}

/**
 * CAN1_RX1_IRQHandler
 *
 */
void CAN1_RX1_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_VEH]);
}

void CAN2_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_NOSE]);
}

void CAN2_TX_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_NOSE]);
}

void CAN2_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_NOSE]);
}

void CAN2_RX1_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan[CAN_BUS_NOSE]);
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart[HW_UART_PORT_GPS]);
}
