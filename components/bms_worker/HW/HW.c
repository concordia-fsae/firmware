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

// Firmware Includes
#include "include/HW_tim.h"
#include "stm32f1xx.h"
#include "LIB_nvm.h"

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

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * @brief  Initializes the generic low-level firmware
 *
 * @retval HW_OK  
 */
HW_StatusTypeDef_E HW_init(void) 
{
    HAL_Init();
    return HW_OK;
}

/**
 * @brief Deinitializes the generic low-level firmware
 *
 * @retval HW_OK
 */
HW_StatusTypeDef_E HW_deInit(void)
{
    HAL_DeInit();
    return HW_OK;
}

void HW_systemHardReset(void)
{
#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
    lib_nvm_cleanUp();
#endif
    pSCB->AIRCR = AIRCR_RESET_REQ;
}
