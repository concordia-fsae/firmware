#pragma once

#include "HW_specific.h"
#include "Types.h"

#define AFIO_BASE              ((u32)0x40010000)
#define FLASH_BASE             ((u32)0x40022000)
#define NVIC_BASE              (SCS_BASE + 0x0100)
#define RCC_BASE               ((u32)0x40021000)
#define SCS_BASE               ((u32)0xE000E000)
#define SCB_BASE               (SCS_BASE + 0x0D00)
#define STK_BASE               (SCS_BASE + 0x10)


#define AFIO_MAPR              (AFIO_BASE + 0x04)
#define AIRCR_RESET            (0x05FA0000)
#define AIRCR_RESET_REQ        (AIRCR_RESET | (u32)0x04);
#define FLASH_ACR              (FLASH_BASE + 0x00)
#define SCB_VTOR               (SCB_BASE + 0x08)
#define STK_CTRL               (STK_BASE + 0x00)


#define RCC_CR                 (RCC_BASE)
#define RCC_CFGR               (RCC_BASE + 0x04)
#define RCC_CIR                (RCC_BASE + 0x08)
#define RCC_AHBENR             (RCC_BASE + 0x14)
#define RCC_APB2ENR            (RCC_BASE + 0x18)
#define RCC_APB1ENR            (RCC_BASE + 0x1C)

// Clocks for the backup domain registers
#define RCC_APB1ENR_PWR_CLK    0x10000000
#define RCC_APB1ENR_BKP_CLK    0x08000000


#define GPIOA                  ((u32)0x40010800)
#define GPIOB                  ((u32)0x40010C00)
#define GPIOC                  ((u32)0x40011000)
#define GPIOD                  ((u32)0x40011400)
#define GPIOE                  ((u32)0x40011800)
#define GPIOF                  ((u32)0x40011C00)
#define GPIOG                  ((u32)0x40012000)

#define GPIO_CRL(port)         (port)
#define GPIO_CRH(port)         (port + 0x04)
#define GPIO_IDR(port)         (port + 0x08)
#define GPIO_ODR(port)         (port + 0x0c)
#define GPIO_BSRR(port)        (port + 0x10)
#define GPIO_CR(port, pin)     (port + (0x04 * (pin > 7)))


// more bit twiddling to set Control register bits
#define CR_SHITF(pin)    ((pin - 8 * (pin > 7)) << 2)
#define CR_OUTPUT_OD     0x05
#define CR_OUTPUT_PP     0x01
#define CR_INPUT         0x04


typedef struct
{
    vuc32 CPUID;
    vu32  ICSR;
    vu32  VTOR;
    vu32  AIRCR;
    vu32  SCR;
    vu32  CCR;
    vu32  SHPR[3];
    vu32  SHCSR;
    vu32  CFSR;
    vu32  HFSR;
    vu32  DFSR;
    vu32  MMFAR;
    vu32  BFAR;
    vu32  AFSR;
} SCB_TypeDef;


/* todo: there must be some major misunderstanding in how we access
 * regs. The direct access approach (GET_REG) causes the usb init to
 * fail upon trying to activate RCC_APB1 |= 0x00800000. However, using
 * the struct approach from ST, it works fine...temporarily switching
 * to that approach */
typedef struct
{
    vu32 CR;
    vu32 CFGR;
    vu32 CIR;
    vu32 APB2RSTR;
    vu32 APB1RSTR;
    vu32 AHBENR;
    vu32 APB2ENR;
    vu32 APB1ENR;
    vu32 BDCR;
    vu32 CSR;
} RCC_RegStruct;
#define pRCC    ((RCC_RegStruct *)RCC_BASE)


// Disable backup domain write protection bit
#define PWR_CR_DBP    (1 << 8)


// Value to place in RTC backup register 10 for persistent bootloader mode
#define RTC_BOOTLOADER_FLAG             0x424C
#define RTC_BOOTLOADER_JUST_UPLOADED    0x424D


// prototypes
void systemHardReset(void);
void setupCLK(void);
void setupLEDAndButton(void);

void gpio_write_bit(u32 bank, u8 pin, u8 val);
void strobePin(u32 bank, u8 pin, u8 count, u32 rate, u8 onState);

unsigned int crMask(int pin);
