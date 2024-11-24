/*
 * HW_SYS.c
 * This file describes low-level, mostly hardware-specific System behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "HW_SYS.h"

#include "CAN.h"
#include "HW.h"
#include "HW_CRC.h"
#include "HW_FLASH.h"
#include "HW_pinmux.h"
#include "Types.h"
#include "UDS.h"
#include "Utilities.h"

#include "BuildDefines.h"

// defined by linker
extern const uint32_t __app_start_addr;
extern const uint32_t __app_end_addr;

__attribute__((section(".appDescriptor")))
const appDesc_S hwDesc = {
    .appStart = (const uint32_t)&__app_start_addr,
    .appEnd = (const uint32_t)&__app_end_addr,
    .appCrcLocation = (const uint32_t)&__app_end_addr,
    .appComponentId = APP_COMPONENT_ID,
    .appPcbaId = APP_PCBA_ID,
#if FEATURE_IS_ENABLED(APP_NODE_ID)
    .appNodeId = APP_NODE_ID,
#endif // APP_NODE_ID
};

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/*
 * bootApp
 * @brief Dedicated function with no call to any function (apart from the last call)
 *        This way, there is no manipulation of the stack here, ensuring that GCC
 *        didn't insert any pop from the SP after having set the MSP.
 *        Can't be static for some reason
 */
void bootApp(uint32_t appAddr);
void bootApp(uint32_t appAddr)
{
    typedef void (*funcPtr)(void);

    // address of the app's reset handler function
    uint32_t appResetHandlerAddr = *(volatile uint32_t *)(appAddr + 0x04);

    // function pointer to the app's reset handler
    funcPtr  appResetHandler     = (funcPtr)appResetHandlerAddr;

    // tell the chip where the app's vector table is
    SET_REG(SCB_VTOR, (volatile uint32_t)appAddr);

    asm volatile ("msr msp, %0" : : "g" (*(volatile uint32_t *)appAddr));

    appResetHandler();    // call the app's reset handler (effectively, actually boot the app)
}


/*
 * checkAppDescValid
 * @brief check if the app descriptor at the start of the app's flash
 *        contains valid records
 */
static bool checkAppDescValid(appDesc_S *appDesc)
{
    // TODO: clean this up
    uint8_t valid = 0b11111;

    if (appDesc->appStart < APP_FLASH_START)
    {
        valid &= 0b11110;
    }

    if (appDesc->appEnd >= APP_FLASH_END)
    {
        valid &= 0b11101;
    }

    if ((appDesc->appCrcLocation & 0x0FF00000) != 0x08000000)
    {
        valid &= 0b11011;
    }

    CAN_TxMessage_S msg = {
        .id          = 0x400,
        .lengthBytes =    1U,
        .mailbox     =    2U,
        .data.u8     = {
            [0] = valid,
        },
    };
    CAN_sendMsg(msg);

    return(valid == 0b111);
}


static bool checkAppCrc(appDesc_S *appDesc)
{
    const uint16_t appLength        = ((appDesc->appEnd - appDesc->appStart) / 4U);
    const uint32_t calculatedAppCrc = CRC_mpeg2Calculate((uint32_t*)appDesc->appStart, appLength);
    const uint32_t appDescCrc       = *(uint32_t*)appDesc->appCrcLocation;


    // send result on CAN
    CAN_TxMessage_S msg = {
        .id          = 0x402,
        .lengthBytes =    8U,
        .mailbox     =    2U,
        .data.u32    = {
            [0] = appDescCrc,
            [1] = calculatedAppCrc,
        }
    };

    CAN_sendMsg(msg);

    return (appDescCrc != 0xFFFFFFFF)
           && (appDescCrc != 0x00000000)
           && (calculatedAppCrc != 0xFFFFFFFF)
           && (calculatedAppCrc != 0x00000000)
           && (calculatedAppCrc == appDescCrc);
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * SYS_init
 * @brief initializes all the relevant system-level modules
 */
void SYS_init(void)
{
    SYS_resetSoft();
    CLK_init();
    TIM_init();
    GPIO_init(pinmux, COUNTOF(pinmux));
    FLASH_init();
    CRC_init();
    CAN_init();
    UDS_init();
}


/*
 * SYS_bootApp
 * @brief de-init most of the chip and boot the application
 */
void SYS_bootApp(uint32_t appAddr)
{
    // tear down all the setup we did so the application
    // doesn't get an unexpected initial configuration
    FLASH_lock();
    GPIO_destroy();
    NVIC_disableInterrupts();
    CAN_destroy();
    TIM_destroy();
    // TODO: tear down everything else we configured too

    SYS_resetSoft();

    bootApp(appAddr);
}

/*
 * SYS_resetSoft
 * @brief performs a soft reset. Resets all (most?) of the peripherals, but doesn't actually
 *        reset the chip
 */
void SYS_resetSoft(void)
{
    SET_REG(RCC_CR,   GET_REG(RCC_CR) | 0x00000001);
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) & 0xF8FF0000);
    SET_REG(RCC_CR,   GET_REG(RCC_CR) & 0xFEF6FFFF);
    SET_REG(RCC_CR,   GET_REG(RCC_CR) & 0xFFFBFFFF);
    SET_REG(RCC_CFGR, GET_REG(RCC_CFGR) & 0xFF80FFFF);

    SET_REG(RCC_CIR,  0x00000000); // disable all RCC interrupts
}


/*
 * SYS_resetHard
 * @brief performs a hard reset. Totally resets the chip. Only the backup peripheral persists
 */
void SYS_resetHard(void)
{
    // Reset
    pSCB->AIRCR = AIRCR_RESET_REQ;

    // should never get here
    while (1)
    {
        asm volatile ("nop");
    }
}


/*
 * SYS_getResetType
 * @brief checks to see if we were reset for a known reason
 */
resetType_E SYS_getResetType(void)
{
    resetType_E resetType = RESET_TYPE_NONE;

    // Enable clocks for the backup domain registers
    pRCC->APB1ENR |= (RCC_APB1ENR_PWR_CLK | RCC_APB1ENR_BKP_CLK);

    // Get flag value
    switch (pBKP->DR10)
    {
        case RTC_BOOTLOADER_FLAG:
            resetType = RESET_TYPE_PERSISTENT_BOOTLOADER;
            break;

        case RTC_BOOTLOADER_JUST_UPLOADED:
            resetType = RESET_TYPE_JUST_UPDATED;
            break;
    }

    if (resetType != RESET_TYPE_NONE)
    {
        bkp10Write(0x0000U);    // Clear the flag
    }

    // Disable clocks
    pRCC->APB1ENR &= ~(RCC_APB1ENR_PWR_CLK | RCC_APB1ENR_BKP_CLK);

    return resetType;
}


/*
 * SYS_checkAppValid
 * @brief checks to see if it looks like there's a valid app at
 *        the relevant start address
 */
bool SYS_checkAppValid(appDesc_S *appDesc)
{
    if (!checkAppDescValid(appDesc))
    {
        return false;
    }

    bool     appCrcValid = checkAppCrc(appDesc);
    uint32_t appStackPtr = *(volatile uint32_t *)(appDesc->appStart);

    // check that app stack pointer points somewhere in sram
    return(appCrcValid && ((appStackPtr & 0x2FFE0000) == 0x20000000));
}
