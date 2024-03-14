/*
 * HW_CLK.h
 * This file describes low-level, mostly hardware-specific RCC/Clock peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define RCC_BASE                (0x40021000UL)

#define RCC_CR                  (RCC_BASE)
#define RCC_CFGR                (RCC_BASE + 0x04UL)
#define RCC_CIR                 (RCC_BASE + 0x08UL)
#define RCC_AHBENR              (RCC_BASE + 0x14UL)
#define RCC_APB2ENR             (RCC_BASE + 0x18UL)
#define RCC_APB1ENR             (RCC_BASE + 0x1CUL)

// peripheral clocks
#define RCC_APB1ENR_PWR_CLK     (0x10000000UL) // PWR clock enable
#define RCC_APB1ENR_BKP_CLK     (0x08000000UL) // Backup periheral clock enable
#define RCC_APB1ENR_TIM2_CLK    (0x00000001UL) // TIM2 peripheral clock enable
#define RCC_APB1ENR_CAN1_CLK    (0x02000000UL) // CAN peripheral clock enable
#define RCC_APB2ENR_AFIO_CLK    (0x00000001UL) // Alternate Function I/O clock enable
#define RCC_AHBENR_CRC_CLK      (0x00000040UL) // CRC peripheral clock enable


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/* todo: there must be some major misunderstanding in how we access
 * regs. The direct access approach (GET_REG) causes the usb init to
 * fail upon trying to activate RCC_APB1 |= 0x00800000. However, using
 * the struct approach from ST, it works fine...temporarily switching
 * to that approach */
typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_regMap;
#define pRCC    ((RCC_regMap *)RCC_BASE)


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void CLK_init(void);
