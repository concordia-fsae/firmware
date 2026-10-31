#pragma once
#define FEATURE_IS_ENABLED(feature)     (feature)
#define FEATURE_IS_DISABLED(feature)    (!(feature))
#define NVM_LIB_ENABLED                 1
#define NVM_FLASH_BACKED                1
#define NVM_TASK                        1
#define NVM_SWI                         0
#define NVM_BLOCK_SIZE                  512U
#define FDEFS_STM32_PN_STM32F103XB      1
#define FDEFS_STM32_PN_STM32F105        2
#define MCU_STM32_PN                    FDEFS_STM32_PN_STM32F103XB
#define MCU_STM32_USE_HAL               0
