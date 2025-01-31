/*
 * HW_GPIO.c
 * This file describes low-level, mostly hardware-specific GPIO peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define GPIOA                 (0x40010800UL)
#define GPIOB                 (0x40010C00UL)
#define GPIOC                 (0x40011000UL)
#define GPIOD                 (0x40011400UL)
#define GPIOE                 (0x40011800UL)
#define GPIOF                 (0x40011C00UL)
#define GPIOG                 (0x40012000UL)

#define GPIO_CR(port, pin)    (port + (0x04UL * (uint32_t)(pin > 7U)))
#define GPIO_IDR(port)        (port + 0x08UL)
#define GPIO_ODR(port)        (port + 0x0CUL)
#define GPIO_BSRR(port)       (port + 0x10UL)

// produces the CNF + MODE bits (4 bits) from the gpio config
// format: | AF on/off (1bit) | CNF (1 bit) | MODE (2 bits) | << (pin shift)
#define GPIO_MODE_BITS(cfg)    (((cfg.alternate_function ? 0b1000 : 0b0000)            \
                                 | ((cfg.config & 0b11U) << 2U) | ((cfg.mode & 0b11U)) \
                                 ) << CR_SHIFT(cfg.pin))

#define GPIO_CR_CFG(cfg)       GPIO_CR(cfg.port, cfg.pin)
#define GPIO_SETUP_PIN(cfg)    SET_REG(GPIO_CR_CFG(cfg),                             \
                                       (GET_REG(GPIO_CR_CFG(cfg)) &CR_MASK(cfg.pin)) \
                                       | GPIO_MODE_BITS(cfg))


// more bit twiddling to set Control register bits
#define CR_SHIFT(pin)    ((uint32_t)(pin - (8U * (uint8_t)(pin > 7U))) << 2U)
#define CR_MASK(pin)     ((uint32_t) ~(0x0F << ((pin - (8U * (uint8_t)(pin >= 8U))) << 2U)))
// FIXME: this can be deleted once its uses are cleaned up
#define CR_INPUT         (0x04UL)

// test macros using arbitrary pin (12)
_Static_assert(CR_MASK(12U) == (uint32_t)(~(0b1111 << 16U)), "CR MASK macro produced incorrect mask");
_Static_assert(CR_SHIFT(12U) == 16U,                         "CR SHIFT macro produced incorrect shift");

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    // INPUT MODE CONFIG BITS
    GPIO_CFG_INPUT_ANALOG      = 0b00,
    GPIO_CFG_INPUT_FLOATING    = 0b01,
    GPIO_CFG_INPUT_PULL        = 0b10,

    // OUTPUT MODE CONFIG BITS
    GPIO_CFG_OUTPUT_PUSH_PULL  = 0b00,
    GPIO_CFG_OUTPUT_OPEN_DRAIN = 0b01,
} GPIO_CNF_E;

typedef enum
{
    GPIO_MODE_INPUT               = 0b00,
    GPIO_MODE_OUTPUT_MEDIUM_SPEED = 0b01,
    GPIO_MODE_OUTPUT_LOW_SPEED    = 0b10,
    GPIO_MODE_OUTPUT_HIGH_SPEED   = 0b11,
} GPIO_MODE_E;

typedef enum
{
    GPIO_PULL_DOWN = 0U,
    GPIO_PULL_UP,
} GPIO_pullDir_E;

typedef struct
{
    uint32_t       port;
    uint8_t        pin;
    bool           alternate_function;
    GPIO_MODE_E    mode;
    GPIO_CNF_E     config;
    GPIO_pullDir_E pullDirection;
} GPIO_config_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void GPIO_init(const GPIO_config_S pinmux[], uint8_t pinCount);
void GPIO_destroy(void);
void GPIO_assignPin(uint32_t bank, uint8_t pin, uint8_t val);
void GPIO_strobePin(uint32_t bank, uint8_t pin, uint8_t count, uint32_t rate, uint8_t onState);
bool GPIO_readPin(void);
