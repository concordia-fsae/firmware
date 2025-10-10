/**
 * @file HW.c
 * @brief  Source code for generic firmware functions
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// Header file
#include "HW.h"

// System Includes
#include "stdbool.h"
#include "string.h"

// Firmware Includes
#include "stm32f1xx.h"
#include "HW_tim.h"
#include "lib_nvm.h"

#include "FeatureDefines_generated.h"

#if (MCU_STM32_PN == FDEFS_STM32_PN_STM32F105) || \
    (MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB)
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
#define pSCB               ((SCB_regMap*)SCB_BASE)

#define AIRCR_RESET        (0x05FA0000UL)
#define AIRCR_RESET_REQ    (AIRCR_RESET | 0x04UL)
#else
#error "Chipset not supported"
#endif

typedef struct
{
    bool mcuShuttingDown;
} data_S;

static data_S data;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief  Initializes the generic low-level firmware
 *
 * @retval Always true 
 */
HW_StatusTypeDef_E HW_init(void) 
{
    memset(&data, 0x00, sizeof(data));
    return HAL_Init() == HAL_OK ? HW_OK : HW_ERROR;
}

/**
 * @brief  Get the number of ticks since clock start
 *
 * @retval Number of ticks
 */
uint32_t HW_getTick(void)
{
    return HAL_GetTick();
}

/**
 * @brief  Delay the execution in blocking mode for amount of ticks
 *
 * @param delay Number of ticks to delay in blocking mode
 */
void HW_delay(uint32_t delay)
{
    HAL_Delay(delay);
}

/**
 * @brief  This function is blocking and should be avoided
 *
 * @param us Microsecond blocking delay
 */
void HW_usDelay(uint8_t us)
{
    uint64_t us_start = HW_TIM_getBaseTick();

    while (HW_TIM_getBaseTick() < us_start + us);
}

void HW_systemHardReset(void)
{
    data.mcuShuttingDown = true;
#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
    lib_nvm_cleanUp();
#endif

#if (MCU_STM32_PN == FDEFS_STM32_PN_STM32F105) || \
    (MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB)
    pSCB->AIRCR = AIRCR_RESET_REQ;
#else
#error "Chipset not supported"
#endif
}

bool HW_mcuShuttingDown(void)
{
    return data.mcuShuttingDown;
}
