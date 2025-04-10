/**
 * @file HW_intc.c
 * @brief  Source code for STM32F1xx Cortex M3 Interrupts
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Firmware Includes
#include "HW_intc.h"
#include "stm32f1xx.h"
#include "FeatureDefines_generated.h"
#include "HW_tim.h"

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;
extern CAN_HandleTypeDef hcan;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void)
{
    while (1)
    {}
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
    volatile uint8_t c = 0;

    while (c == 0)
    {}
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void)
{
    while (1)
    {}
}

/**
 * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void)
{
    while (1)
    {}
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void)
{
    while (1)
    {}
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
    // HAL_ADC_IRQHandler(&hadc2);
}
/**
 * @brief This function handles TIM4 global interrupt.
 */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim[HW_TIM_PORT_PWM]);
}

void TIM3_IRQHandler(void)
{
#if FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
    HAL_TIM_IRQHandler(&htim[HW_TIM_PORT_HS_INTERRUPT]);
#endif // FEATURE_HIGH_FREQUENCY_CELL_MEASUREMENT_TASK == FEATURE_DISABLED
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim_tick);
}

void TIM1_TRG_COM_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim[HW_TIM_PORT_TACH]);
}

void TIM1_CC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim[HW_TIM_PORT_TACH]);
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
