/**
 * stm32f1xx_hal_conf.h
 * HAL configuration file
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

// Enable HAL modules here

#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
// #define HAL_CRYP_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
// #define HAL_CAN_LEGACY_MODULE_ENABLED
// #define HAL_CEC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
// #define HAL_CRC_MODULE_ENABLED
// #define HAL_DAC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
// #define HAL_ETH_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
// #define HAL_I2C_MODULE_ENABLED
// #define HAL_I2S_MODULE_ENABLED
// #define HAL_IRDA_MODULE_ENABLED
// #define HAL_IWDG_MODULE_ENABLED
// #define HAL_NOR_MODULE_ENABLED
// #define HAL_NAND_MODULE_ENABLED
// #define HAL_PCCARD_MODULE_ENABLED
// #define HAL_PCD_MODULE_ENABLED
// #define HAL_HCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
// #define HAL_RTC_MODULE_ENABLED
// #define HAL_SD_MODULE_ENABLED
// #define HAL_MMC_MODULE_ENABLED
// #define HAL_SDRAM_MODULE_ENABLED
// #define HAL_SMARTCARD_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
// #define HAL_SRAM_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
// #define HAL_UART_MODULE_ENABLED
// #define HAL_USART_MODULE_ENABLED
// #define HAL_WWDG_MODULE_ENABLED

// Adjust Oscillator settings

#if !defined(HSE_VALUE)
# define HSE_VALUE    8000000U // Value of the External oscillator in Hz
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
# define HSE_STARTUP_TIMEOUT    100U // Time out for HSE start up, in ms
#endif

#if !defined(HSI_VALUE)
# define HSI_VALUE    8000000U // Value of the Internal oscillator in Hz
#endif

#if !defined(LSI_VALUE)
# define LSI_VALUE    40000U // LSI Typical Value in Hz
#endif

#if !defined(LSE_VALUE)
# define LSE_VALUE    32768U // Value of the External oscillator in Hz
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
# define LSE_STARTUP_TIMEOUT    5000U // Time out for LSE start up, in ms
#endif

// ########################### System Configuration #########################
/**
 * @brief This is the HAL system configuration section
 */
#define VDD_VALUE                               3300U // Value of VDD in mv
#define TICK_INT_PRIORITY                       0U    // tick interrupt priority (lowest by default)
#define USE_RTOS                                0U
#define PREFETCH_ENABLE                         1U

#define USE_HAL_ADC_REGISTER_CALLBACKS          0U // ADC register callback disabled
#define USE_HAL_CAN_REGISTER_CALLBACKS          1U // CAN register callback disabled
#define USE_HAL_CEC_REGISTER_CALLBACKS          0U // CEC register callback disabled
#define USE_HAL_DAC_REGISTER_CALLBACKS          0U // DAC register callback disabled
#define USE_HAL_ETH_REGISTER_CALLBACKS          0U // ETH register callback disabled
#define USE_HAL_HCD_REGISTER_CALLBACKS          0U // HCD register callback disabled
#define USE_HAL_I2C_REGISTER_CALLBACKS          0U // I2C register callback disabled
#define USE_HAL_I2S_REGISTER_CALLBACKS          0U // I2S register callback disabled
#define USE_HAL_MMC_REGISTER_CALLBACKS          0U // MMC register callback disabled
#define USE_HAL_NAND_REGISTER_CALLBACKS         0U // NAND register callback disabled
#define USE_HAL_NOR_REGISTER_CALLBACKS          0U // NOR register callback disabled
#define USE_HAL_PCCARD_REGISTER_CALLBACKS       0U // PCCARD register callback disabled
#define USE_HAL_PCD_REGISTER_CALLBACKS          0U // PCD register callback disabled
#define USE_HAL_RTC_REGISTER_CALLBACKS          0U // RTC register callback disabled
#define USE_HAL_SD_REGISTER_CALLBACKS           0U // SD register callback disabled
#define USE_HAL_SMARTCARD_REGISTER_CALLBACKS    0U // SMARTCARD register callback disabled
#define USE_HAL_IRDA_REGISTER_CALLBACKS         0U // IRDA register callback disabled
#define USE_HAL_SRAM_REGISTER_CALLBACKS         0U // SRAM register callback disabled
#define USE_HAL_SPI_REGISTER_CALLBACKS          0U // SPI register callback disabled
#define USE_HAL_TIM_REGISTER_CALLBACKS          0U // TIM register callback disabled
#define USE_HAL_UART_REGISTER_CALLBACKS         0U // UART register callback disabled
#define USE_HAL_USART_REGISTER_CALLBACKS        0U // USART register callback disabled
#define USE_HAL_WWDG_REGISTER_CALLBACKS         0U // WWDG register callback disabled


// Uncomment the line below to expanse the "assert_param" macro in the
// HAL drivers code
// #define USE_FULL_ASSERT
#ifdef USE_FULL_ASSERT
/**
 * @brief  The assert_param macro is used for function's parameters check.
 * @param  expr If expr is false, it calls assert_failed function
 *         which reports the name of the source file and the source
 *         line number of the call that failed.
 *         If expr is true, it returns no value.
 * @retval None
 */
# define assert_param(expr)    ((expr) ? (void)0U : assert_failed((uint8_t*)__FILE__, __LINE__))

void assert_failed(uint8_t* file, uint32_t line);
#else
# define assert_param(expr)    ((void)0U)
#endif // ifdef USE_FULL_ASSERT


// CRC FEATURE: Use to activate CRC feature inside HAL SPI Driver
// Activated: CRC code is present inside driver
// Deactivated: CRC code cleaned from driver
#define USE_SPI_CRC    0U

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Include headers for enabled HAL modules

#ifdef HAL_RCC_MODULE_ENABLED
# include "stm32f1xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
# include "stm32f1xx_hal_gpio.h"
#endif

#ifdef HAL_EXTI_MODULE_ENABLED
# include "stm32f1xx_hal_exti.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
# include "stm32f1xx_hal_dma.h"
#endif

#ifdef HAL_CAN_MODULE_ENABLED
# include "stm32f1xx_hal_can.h"
#endif

#ifdef HAL_CEC_MODULE_ENABLED
# include "stm32f1xx_hal_cec.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
# include "stm32f1xx_hal_cortex.h"
#endif

#ifdef HAL_ADC_MODULE_ENABLED
# include "stm32f1xx_hal_adc.h"
#endif

#ifdef HAL_CRC_MODULE_ENABLED
# include "stm32f1xx_hal_crc.h"
#endif

#ifdef HAL_DAC_MODULE_ENABLED
# include "stm32f1xx_hal_dac.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
# include "stm32f1xx_hal_flash.h"
#endif

#ifdef HAL_SRAM_MODULE_ENABLED
# include "stm32f1xx_hal_sram.h"
#endif

#ifdef HAL_NOR_MODULE_ENABLED
# include "stm32f1xx_hal_nor.h"
#endif

#ifdef HAL_I2C_MODULE_ENABLED
# include "stm32f1xx_hal_i2c.h"
#endif

#ifdef HAL_IWDG_MODULE_ENABLED
# include "stm32f1xx_hal_iwdg.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
# include "stm32f1xx_hal_pwr.h"
#endif

#ifdef HAL_RTC_MODULE_ENABLED
# include "stm32f1xx_hal_rtc.h"
#endif

#ifdef HAL_PCCARD_MODULE_ENABLED
# include "stm32f1xx_hal_pccard.h"
#endif

#ifdef HAL_SD_MODULE_ENABLED
# include "stm32f1xx_hal_sd.h"
#endif

#ifdef HAL_NAND_MODULE_ENABLED
# include "stm32f1xx_hal_nand.h"
#endif

#ifdef HAL_SPI_MODULE_ENABLED
# include "stm32f1xx_hal_spi.h"
#endif

#ifdef HAL_TIM_MODULE_ENABLED
# include "stm32f1xx_hal_tim.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
# include "stm32f1xx_hal_uart.h"
#endif

#ifdef HAL_USART_MODULE_ENABLED
# include "stm32f1xx_hal_usart.h"
#endif

#ifdef HAL_IRDA_MODULE_ENABLED
# include "stm32f1xx_hal_irda.h"
#endif

#ifdef HAL_SMARTCARD_MODULE_ENABLED
# include "stm32f1xx_hal_smartcard.h"
#endif

#ifdef HAL_WWDG_MODULE_ENABLED
# include "stm32f1xx_hal_wwdg.h"
#endif

#ifdef HAL_PCD_MODULE_ENABLED
# include "stm32f1xx_hal_pcd.h"
#endif

#ifdef HAL_HCD_MODULE_ENABLED
# include "stm32f1xx_hal_hcd.h"
#endif

#ifdef HAL_MMC_MODULE_ENABLED
# include "stm32f1xx_hal_mmc.h"
#endif

#ifdef __cplusplus
}
#endif
