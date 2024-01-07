/**
 * HW_intc.h
 * Header file for the Interrupt Controller hardware implementation
 */

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

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
void TIM2_IRQHandler(void);
// 
// void CAN1_SCE_IRQHandler(void);
// void USB_HP_CAN1_TX_IRQHandler(void);
// void USB_LP_CAN1_RX0_IRQHandler(void);
// void CAN1_RX1_IRQHandler(void);

#ifdef __cplusplus
}
#endif
