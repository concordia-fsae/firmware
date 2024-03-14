/*
 * HW_NVIC.c
 * This file describes low-level, mostly hardware-specific NVIC peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW.h"
#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define NVIC_BASE         (SCS_BASE + 0x0100UL)
#define NVIC_PRIO_BITS    4U    // STM32 uses 4 Bits for the Priority Levels
#define NVIC_PRIGROUP     (((*(uint32_t*)SCB_AIRCR) & 0x700UL) >> 8U)


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    volatile uint32_t ISER[2];
    uint32_t          RESERVED0[30];
    volatile uint32_t ICER[2];
    uint32_t          RSERVED1[30];
    volatile uint32_t ISPR[2];
    uint32_t          RESERVED2[30];
    volatile uint32_t ICPR[2];
    uint32_t          RESERVED3[30];
    volatile uint32_t IABR[2];
    uint32_t          RESERVED4[62];
    volatile uint32_t IPR[15];
} NVIC_regMap;
#define pNVIC    ((NVIC_regMap*)NVIC_BASE)

typedef struct
{
    uint8_t NVIC_IRQChannel;
    uint8_t NVIC_IRQChannelPreemptionPriority;
    uint8_t NVIC_IRQChannelSubPriority;
    bool    NVIC_IRQChannelCmd; // TRUE for enable
} NVIC_config_S;


typedef enum
{
// *****  Cortex-M3 Processor Exceptions Numbers **************************************************
    NonMaskableInt_IRQn   = -14,    // 2 Non Maskable Interrupt
    HardFault_IRQn        = -13,    // 3 Cortex-M3 Hard Fault Interrupt
    MemoryManagement_IRQn = -12,    // 4 Cortex-M3 Memory Management Interrupt
    BusFault_IRQn         = -11,    // 5 Cortex-M3 Bus Fault Interrupt
    UsageFault_IRQn       = -10,    // 6 Cortex-M3 Usage Fault Interrupt
    SVCall_IRQn           = -5,     // 11 Cortex-M3 SV Call Interrupt
    DebugMonitor_IRQn     = -4,     // 12 Cortex-M3 Debug Monitor Interrupt
    PendSV_IRQn           = -2,     // 14 Cortex-M3 Pend SV Interrupt
    SysTick_IRQn          = -1,     // 15 Cortex-M3 System Tick Interrupt

// *****  STM32 specific Interrupt Numbers ********************************************************
    WWDG_IRQn             = 0,                    // Window WatchDog Interrupt
    PVD_IRQn              = 1,                    // PVD through EXTI Line detection Interrupt
    TAMPER_IRQn           = 2,                    // Tamper Interrupt
    RTC_IRQn              = 3,                    // RTC global Interrupt
    FLASH_IRQn            = 4,                    // FLASH global Interrupt
    RCC_IRQn              = 5,                    // RCC global Interrupt
    EXTI0_IRQn            = 6,                    // EXTI Line0 Interrupt
    EXTI1_IRQn            = 7,                    // EXTI Line1 Interrupt
    EXTI2_IRQn            = 8,                    // EXTI Line2 Interrupt
    EXTI3_IRQn            = 9,                    // EXTI Line3 Interrupt
    EXTI4_IRQn            = 10,                   // EXTI Line4 Interrupt
    DMA1_Channel1_IRQn    = 11,                   // DMA1 Channel 1 global Interrupt
    DMA1_Channel2_IRQn    = 12,                   // DMA1 Channel 2 global Interrupt
    DMA1_Channel3_IRQn    = 13,                   // DMA1 Channel 3 global Interrupt
    DMA1_Channel4_IRQn    = 14,                   // DMA1 Channel 4 global Interrupt
    DMA1_Channel5_IRQn    = 15,                   // DMA1 Channel 5 global Interrupt
    DMA1_Channel6_IRQn    = 16,                   // DMA1 Channel 6 global Interrupt
    DMA1_Channel7_IRQn    = 17,                   // DMA1 Channel 7 global Interrupt
    ADC1_2_IRQn           = 18,                   // ADC1 and ADC2 global Interrupt
    USB_HP_CAN1_TX_IRQn   = 19,                   // USB Device High Priority (shared with CAN1_TX_IRQn)
    CAN1_TX_IRQn          = USB_HP_CAN1_TX_IRQn,  // CAN1 TX Interrupt (shared with USB_HP_CAN1_TX_IRQn)
    USB_LP_CAN1_RX0_IRQn  = 20,                   // USB Device Low Priority (shared with CAN1_RX0_IRQn)
    CAN1_RX0_IRQn         = USB_LP_CAN1_RX0_IRQn, // CAN1 RX0 Interrupt (shared with USB_LP_CAN1_RX0_IRQn)
    CAN1_RX1_IRQn         = 21,                   // CAN1 RX1 Interrupt
    CAN1_SCE_IRQn         = 22,                   // CAN1 SCE Interrupt
    EXTI9_5_IRQn          = 23,                   // External Line[9:5] Interrupts
    TIM1_BRK_IRQn         = 24,                   // TIM1 Break Interrupt
    TIM1_UP_IRQn          = 25,                   // TIM1 Update Interrupt
    TIM1_TRG_COM_IRQn     = 26,                   // TIM1 Trigger and Commutation Interrupt
    TIM1_CC_IRQn          = 27,                   // TIM1 Capture Compare Interrupt
    TIM2_IRQn             = 28,                   // TIM2 global Interrupt
    TIM3_IRQn             = 29,                   // TIM3 global Interrupt
    TIM4_IRQn             = 30,                   // TIM4 global Interrupt
    I2C1_EV_IRQn          = 31,                   // I2C1 Event Interrupt
    I2C1_ER_IRQn          = 32,                   // I2C1 Error Interrupt
    I2C2_EV_IRQn          = 33,                   // I2C2 Event Interrupt
    I2C2_ER_IRQn          = 34,                   // I2C2 Error Interrupt
    SPI1_IRQn             = 35,                   // SPI1 global Interrupt
    SPI2_IRQn             = 36,                   // SPI2 global Interrupt
    USART1_IRQn           = 37,                   // USART1 global Interrupt
    USART2_IRQn           = 38,                   // USART2 global Interrupt
    USART3_IRQn           = 39,                   // USART3 global Interrupt
    EXTI15_10_IRQn        = 40,                   // External Line[15:10] Interrupts
    RTC_Alarm_IRQn        = 41,                   // RTC Alarm through EXTI Line Interrupt
    USBWakeUp_IRQn        = 42,                   // USB Device WakeUp from suspend through EXTI Line Interrupt
} IRQ_map_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void NVIC_SetPriority(IRQ_map_E IRQn, uint32_t preemptPriority, uint32_t subPriority);
void NVIC_EnableIRQ(IRQ_map_E IRQn);
void NVIC_disableInterrupts(void);
