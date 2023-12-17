/**
 * @file HW_intc.h
 * @brief  Header file for STM32F1xx Cortex M3 Interrupts
 */

#pragma once

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);

void DMA1_Channel1_IRQHandler(void);
void ADC1_2_IRQHandler(void);
void TIM4_IRQHandler(void);
void TIM1_TRG_COM_IRQHandler(void);
void TIM2_IRQHandler(void);
void TIM1_CC_IRQHandler(void);

// TODO: Need to implement in communications updates
// void CAN1_SCE_IRQHandler(void);
// void USB_HP_CAN1_TX_IRQHandler(void);
// void USB_LP_CAN1_RX0_IRQHandler(void);
// void CAN1_RX1_IRQHandler(void);