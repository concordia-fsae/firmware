/*
 * HW.h
 * Includes all the other hardware headers
 * Also adds some miscellaneous content that doesn't warrant a dedicated file
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// HW files
#include "HW_CAN.h"
#include "HW_CLK.h"
#include "HW_CRC.h"
#include "HW_FLASH.h"
#include "HW_GPIO.h"
#include "HW_NVIC.h"
#include "HW_specific.h"
#include "HW_SYS.h"
#include "HW_TIM.h"

#include "Types.h"

#include <stddef.h> // NULL

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// Peripheral register base addresses
#define AFIO_BASE          (0x40010000UL)
#define BKP_BASE           (0x40006C00UL)
#define PWR_BASE           (0x40007000UL)
#define SCS_BASE           (0xE000E000UL)
#define STK_BASE           (SCS_BASE + 0x10UL)
#define SCB_BASE           (SCS_BASE + 0x0D00UL)

// Register addresses
#define AFIO_MAPR          (AFIO_BASE + 0x04UL)
#define AIRCR_RESET        (0x05FA0000UL)
#define AIRCR_RESET_REQ    (AIRCR_RESET | 0x04UL)

// Bits
#define SCB_AIRCR          (SCB_BASE + 0x60UL)
#define SCB_VTOR           (SCB_BASE + 0x08UL)
#define STK_CTRL           (STK_BASE + 0x00UL)


// Value to place in RTC backup register 10 for persistent bootloader mode
#define RTC_BOOTLOADER_FLAG             0x424C
#define RTC_BOOTLOADER_JUST_UPLOADED    0x424D

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    volatile uint32_t const CPUID;
    volatile uint32_t       ICSR;
    volatile uint32_t       VTOR;
    volatile uint32_t       AIRCR;
    volatile uint32_t       SCR;
    volatile uint32_t       CCR;
    volatile uint32_t       SHPR[3];
    volatile uint32_t       SHCSR;
    volatile uint32_t       CFSR;
    volatile uint32_t       HFSR;
    volatile uint32_t       DFSR;
    volatile uint32_t       MMFAR;
    volatile uint32_t       BFAR;
    volatile uint32_t       AFSR;
} SCB_regMap;
#define pSCB    ((SCB_regMap*)SCB_BASE)


// Backup peripheral register map type.
typedef struct
{
    const uint32_t    RESERVED1; // Reserved
    volatile uint16_t DR1;       // Data register 1
    const uint16_t    RESERVED2;
    volatile uint16_t DR2;       // Data register 2
    const uint16_t    RESERVED3;
    volatile uint16_t DR3;       // Data register 3
    const uint16_t    RESERVED4;
    volatile uint16_t DR4;       // Data register 4
    const uint16_t    RESERVED5;
    volatile uint16_t DR5;       // Data register 5
    const uint16_t    RESERVED6;
    volatile uint16_t DR6;       // Data register 6
    const uint16_t    RESERVED7;
    volatile uint16_t DR7;       // Data register 7
    const uint16_t    RESERVED8;
    volatile uint16_t DR8;       // Data register 8
    const uint16_t    RESERVED9;
    volatile uint16_t DR9;       // Data register 9
    const uint16_t    RESERVED10;
    volatile uint16_t DR10;      // Data register 10
    const uint16_t    RESERVED11;
    volatile uint32_t RTCCR;     // RTC control register
    volatile uint32_t CR;        // Control register
    volatile uint32_t CSR;       // Control and status register
} BKP_regMap;
#define pBKP    ((BKP_regMap*)BKP_BASE)


// Power interface register map.
typedef struct
{
    volatile uint32_t CR;    // Control register
    volatile uint32_t CSR;   // Control and status register
} PWR_regMap;
#define pPWR          ((PWR_regMap*)PWR_BASE)

// Disable backup domain write protection bit
#define PWR_CR_DBP    (1U << 8UL)


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void bkp10Write(uint16_t value);
