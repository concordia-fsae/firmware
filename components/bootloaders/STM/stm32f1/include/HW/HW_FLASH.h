/*
 * HW_FLASH.h
 * This file describes low-level, mostly hardware-specific Flash peripheral values
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"
#include "LIB_app.h"
#include "FeatureDefines_generated.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define FLASH_BASE         (0x40022000UL)        // base address of flash registers

#define FLASH_ACR          (FLASH_BASE + 0x00UL) // flash access control register
#define FLASH_KEYR         (FLASH_BASE + 0x04UL) // flash key register for unlocking flash
#define FLASH_OPTKEYR      (FLASH_BASE + 0x08UL) // flash key register for unlocking option bytes
#define FLASH_SR           (FLASH_BASE + 0x0CUL) // flash status register
#define FLASH_CR           (FLASH_BASE + 0x10UL) // flash control register
#define FLASH_AR           (FLASH_BASE + 0x14UL) // flash access register
#define FLASH_OBR          (FLASH_BASE + 0x1CUL) // flash option byte register
#define FLASH_WRPR         (FLASH_BASE + 0x20UL) // flash write protection register

#define FLASH_KEY1         (0x45670123UL)        // first key to be written to unlock flash
#define FLASH_KEY2         (0xCDEF89ABUL)        // second key to be written to unlock flash
#define FLASH_RDPRT        (0x000000A5UL)        // flash read protection key
#define FLASH_SR_BSY       (0x00000001UL)        // flash busy bit
#define FLASH_CR_PG        (0x00000001UL)        // flash program enable bit
#define FLASH_CR_PER       (0x00000002UL)        // flash page erase enable bit
#define FLASH_CR_START     (0x00000040UL)        // flash page erase start bit
#define FLASH_CR_LOCK      (0x00000080UL)        // flash page erase start bit

#define FLASH_PAGE_LAST    (0x08007C00UL)        // address of the last flash page
#define FLASH_SIZE_REG     (0x1FFFF7E0UL)        // register whose 4 least significant bits
                                                 // contains the size of the flash for this chip

#define FLASH_BUSY()       ((bool)(GET_REG(FLASH_SR) &FLASH_SR_BSY))  // returns true if flash is busy

// Address where the application code starts
// The app descriptor is expected at this address
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BL
#define APP_FLASH_START    ((const uint32_t)__APP_FLASH_ORIGIN)
#define APP_FLASH_END      ((const uint32_t)__FLASH_END)
#define APP_DESC_ADDR      ((lib_app_appDesc_S * const)APP_FLASH_START)
#elif APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
#define APP_FLASH_START    ((const uint32_t)__FLASH_ORIGIN)
#define APP_FLASH_END      ((const uint32_t)__BOOT_FLASH_END)
#define APP_DESC_ADDR      ((lib_app_appDesc_S * const)(APP_FLASH_END - sizeof(lib_app_crc_t) - sizeof(lib_app_appDesc_S)))
#endif

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

// defined in the linker script
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BL
extern const uint8_t __FLASH_END[];
extern const uint8_t __APP_FLASH_ORIGIN[];
#elif APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
extern const uint8_t __BOOT_FLASH_END[];
extern const uint8_t __FLASH_ORIGIN[];
#endif


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    bool     started   :1; // TRUE if app erase has started
    bool     completed :1; // TRUE if app erase has completed
    bool     status    :1; // TRUE if app has been erased successfully, FALSE if failed or unknown (i.e. in progress)
    uint16_t currPage;     // page to be erased
} FLASH_eraseState_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void               FLASH_init(void);

void               FLASH_lock(void);
void               FLASH_unlock(void);

bool               FLASH_updaterHasErasedFlash(void);
FLASH_eraseState_S FLASH_getEraseState(void);
bool               FLASH_erasePages(uint32_t pageAddr, uint16_t pages);
static inline bool FLASH_erasePage(uint32_t pageAddr) { return FLASH_erasePages(pageAddr, 1U); }
bool               FLASH_eraseAppStart(void);
bool               FLASH_eraseAppContinue(void);
bool               FLASH_eraseAppBlocking(void);
void               FLASH_eraseCompleteAck(void);

bool               FLASH_writeHalfwords(uint32_t addr, uint16_t *data, uint16_t dataLen);
bool               FLASH_writeWords(uint32_t addr, uint32_t *data, uint16_t dataLen);
static inline bool FLASH_writeWord(uint32_t addr, uint32_t data) { return FLASH_writeWords(addr, &data, 1U); };
