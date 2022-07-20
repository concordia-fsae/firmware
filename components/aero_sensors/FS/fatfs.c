/**
 * @file fatfs.h
 * @brief  Source code for the FatFS interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-16
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "fatfs.h"
#include "ff_gen_drv.h"
#include "integer.h"

#include "HW_spi.h"
#include <stdint.h>
#include <wchar.h>
//#include <stdint.h>


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define SD_SPI_INTERFACE ((SPI_TypeDef*)SPI2) /**< SD Interface on SPI2 */
#define SD_INIT_TIMEOUT  1000

#define FCLK_SLOW()                                                            \
 {                                                                             \
  MODIFY_REG(spi2->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_128); \
 } /* Set SCLK = slow, 250 KBits/s*/
#define FCLK_FAST()                                                          \
 {                                                                           \
  MODIFY_REG(spi2->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_8); \
 } /* Set SCLK = fast, 4 MBits/s */

/* MMC/SD command */
#define CMD0   (0)         /* GO_IDLE_STATE */
#define CMD1   (1)         /* SEND_OP_COND (MMC) */
#define ACMD41 (0x80 + 41) /* SEND_OP_COND (SDC) */
#define CMD8   (8)         /* SEND_IF_COND */
#define CMD9   (9)         /* SEND_CSD */
#define CMD10  (10)        /* SEND_CID */
#define CMD12  (12)        /* STOP_TRANSMISSION */
#define ACMD13 (0x80 + 13) /* SD_STATUS (SDC) */
#define CMD16  (16)        /* SET_BLOCKLEN */
#define CMD17  (17)        /* READ_SINGLE_BLOCK */
#define CMD18  (18)        /* READ_MULTIPLE_BLOCK */
#define CMD23  (23)        /* SET_BLOCK_COUNT (MMC) */
#define ACMD23 (0x80 + 23) /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24  (24)        /* WRITE_BLOCK */
#define CMD25  (25)        /* WRITE_MULTIPLE_BLOCK */
#define CMD32  (32)        /* ERASE_ER_BLK_START */
#define CMD33  (33)        /* ERASE_ER_BLK_END */
#define CMD38  (38)        /* ERASE */
#define CMD55  (55)        /* APP_CMD */
#define CMD58  (58)        /* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC   0x01              /* MMC ver 3 */
#define CT_SD1   0x02              /* SD ver 1 */
#define CT_SD2   0x04              /* SD ver 2 */
#define CT_SDC   (CT_SD1 | CT_SD2) /* SD */
#define CT_BLOCK 0x08              /* Block addressing */


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/**
 * Start of Internal Functions
 */
static uint8_t card_select(void);
static void    card_deselect(void);
static uint8_t xchg_sd(uint8_t data);
static void    start_timer(uint32_t wait);
static uint8_t timer_status(void);
static BYTE    send_cmd(BYTE cmd, DWORD arg);
static BYTE    xmit_datablock(const BYTE* buff, BYTE token);
static BYTE    rcvr_datablock(BYTE* buff, uint32_t btr);
/**
 * End of Internal Functions
 */

/**
 * Start FatFS Generic Interface
 */
#include "assert.h"
static_assert(sizeof(DWORD) == 4 && sizeof(SHORT) == 2 && sizeof(BYTE) == 1, "WRONG FatFS TYPE SIZE");
static_assert(sizeof(UINT) == 4, "WRONG FatFS UINT TYPE SIZE");
DSTATUS sd_init(uint8_t drv);
DSTATUS sd_status(uint8_t drv);
DRESULT sd_read(uint8_t byte, uint8_t* buff, DWORD sector, UINT count);
DRESULT sd_write(uint8_t drv, const uint8_t* buff, uint32_t sector, UINT count);
/**
 * End FatFS Generic Interface
 */

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static const Diskio_drvTypeDef disk_driver = {
    .disk_initialize = &sd_init,
    .disk_status     = &sd_status,
    .disk_read       = &sd_read,
    .disk_write      = &sd_write,
};

static uint8_t CardStatus;
static uint8_t CardType;

uint32_t sdTimerTickStart;
uint32_t sdTimerTickDelay;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Link the driver to FatFs
 */
void FatFS_Init(void)
{
    FATFS_LinkDriver(&disk_driver, "/");
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * Start of Specific Internal Functions
 */

/**
 * @brief  Select the CS of the SD
 *
 * @retval   1: OK, 0: Timout
 */
static inline uint8_t card_select(void)
{
    return 1;
}
/**
 * @brief  Deslect SD CS
 */
static inline void card_deselect(void)
{
    /**< Current implementation has no CS and no other SPI peripheral */
}

/**
 * @brief  Transmit and receive 8 bit
 *
 * @param data Data to be sent to the SD
 *
 * @retval   8 bits received from SD
 */
static inline uint8_t xchg_sd(uint8_t data)
{
    return HW_SPI_TransmitReceive8(SD_SPI_INTERFACE, data);
}

/**
 * @brief  Starts the timer
 *
 * @param wait Timout(ms)
 */
static void start_timer(uint32_t wait)
{
    sdTimerTickStart = HAL_GetTick();
    sdTimerTickDelay = wait;
}

/**
 * @brief  Returns the status of the timer
 *
 * @retval   0: Valid, 1: Invalid
 */
static uint8_t timer_status(void)
{
    return ((HAL_GetTick() - sdTimerTickStart) < sdTimerTickDelay);
}

/**
 * @brief  Send's a command to the SD
 *
 * @param cmd Command code
 * @param arg Argument
 *
 * @retval   R1 resp (bit7==1:Failed)
 */
static BYTE send_cmd(BYTE cmd, DWORD arg)
{
    BYTE n, res;

    if (cmd & 0x80) /**< Send CMD55 prior to ACMD */
    {
        cmd &= 0x7f;
        res = send_cmd(CMD55, 0);
        if (res > 1)
            return res;
    }

    if (cmd != CMD12) /**< Select card and wait for ready */
    {
        card_deselect();
        if (!card_select())
            return 0xff;
    }

    /**< Send command packet */
    xchg_sd(0x40 | cmd); /**< Start + Command */
    xchg_sd((BYTE)arg >> 24);
    xchg_sd((BYTE)arg >> 16);
    xchg_sd((BYTE)arg >> 8);
    xchg_sd((BYTE)arg);

    n = 0x01; /**< Dummy CRC + Stop */
    if (cmd == CMD0)
        n = 0x95; /**< Valid CRC for CMD0 */
    if (cmd == CMD8)
        n = 0x87; /**< Valid CRC for CMD8 */
    xchg_sd(n);

    if (cmd == CMD12)
        xchg_sd(0xff); /**< Discard following byte when CMD12 */
    n = 10;            /**< Wait for response, 10 bytes max */

    do {
        res = xchg_sd(0xff);
    } while ((res & 0x80) && --n);

    return res;
}

/**
 * @brief Transmit a block of data to the SD card
 *
 * @param buff Buffer of data to send
 * @param token Transfer token to send
 *
 * @retval Integer value (1: OK, 0: Failed)
 */
static BYTE xmit_datablock(const BYTE* buff, BYTE token)
{
    BYTE resp;

    // if
}

/**
 * @brief Receive a block of data from the SD card
 *
 * @param buff Buffer to write too
 * @param btr Size in bytes
 *
 * @retval Integer value (1: OK, 0: Failed)
 */
static BYTE rcvr_datablock(BYTE* buff, uint32_t btr)
{
}

/**
 * End of Specific Internal Functions
 */

/**
 * Start of FatFS Generic Interface
 */

/**
 * @brief  Initializes SD
 *
 * @param drv Drive number
 *
 * @retval   Status
 */
DSTATUS sd_init(BYTE drv)
{
    uint8_t n, type, ocr[4], cmd;

    if (!drv)
        return STA_NOINIT; /**< Support only drive 0 */

    FCLK_SLOW();
    for (n = 10; n; n--) xchg_sd(0xff);

    type = 0;

    if (send_cmd(CMD0, 0) == 1) /**< Set card to idle state */
    {
        start_timer(SD_INIT_TIMEOUT);
        if (send_cmd(CMD8, 0x1aa) == 1) /**< Is card SDv2? */
        {
            for (n = 0; n < 4; n++) ocr[n] = xchg_sd(0xff); /**< Get 32 bit R7 response */
            if (ocr[2] == 0x01 && ocr[3] == 0xaa)           /**< Does the card support 2.7-3.7v? */
            {
                while (timer_status() && send_cmd(ACMD41, 0x01 << 30))
                    ;                                          /**< Wait for end of initialization with ACMD41 */
                if (timer_status() && send_cmd(CMD58, 0) == 0) /**< Check CCS bit in OCR */
                {
                    for (n = 0; n < 4; n++) ocr[n] = xchg_sd(0xff);
                    type = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2; /**< Card ID SDv2 */
                }
            }
        }
        else
        {
            if (send_cmd(ACMD41, 0) <= 1) /**< SDv1 or MMC? */
            {
                type = CT_SD1;
                cmd  = ACMD41; /**< SDv1 and ACMD41*/
            }
            else
            {
                type = CT_MMC;
                cmd  = CMD1; /** MMCv3 and CMD1 */
            }
            while (timer_status() && send_cmd(cmd, 0))
                ;                                             /**< Wait for end of initialization */
            if (!timer_status() || send_cmd(CMD16, 512) != 0) /**< Set blocksize = 512 */
                type = 0;
        }
    }

    CardType = type;

    card_deselect();

    if (type)
    {
        FCLK_FAST();
        CardStatus &= ~STA_NOINIT;
    }
    else
    {
        CardStatus = STA_NOINIT;
    }

    return CardStatus;
}

/**
 * @brief  Get status of the SD
 *
 * @param drv Drive number
 *
 * @retval   Status
 */
DSTATUS sd_status(BYTE drv)
{
    if (drv)
        return STA_NOINIT;

    return CardStatus;
}

/**
 * @brief  Read from the SD
 *
 * @param drv Drive number
 * @param buff Buffer to write to
 * @param sector Sector to read from
 * @param count Count of sectors to read
 *
 * @retval   Result of the read
 */
DRESULT sd_read(BYTE drv, BYTE* buff, DWORD sector, UINT count)
{
    if (drv || !count)
        return RES_PARERR; /**< Check params */
    if (CardStatus & STA_NOINIT)
        return RES_NOTRDY; /**< Check drive is ready */

    if (!(CardType & CT_BLOCK))
        sector *= 512; /**< Is card byte addressable? */

    if (count == 1)
    {
        if ((send_cmd(CMD17, sector) == 0) && rcvr_datablock(buff, 512)) /**< Read single block */
            count = 0;
    }
    else
    {
        if (send_cmd(CMD18, sector) == 0)
        {
            do {
                if (!rcvr_datablock(buff, 512))
                    break;
                buff += 512;
            } while (--count);
            send_cmd(CMD12, 0); /**< Stop transmission */
        }
    }

    card_deselect();

    return count ? RES_ERROR : RES_OK;
}

/**
 * @brief  Write to the SD
 *
 * @param drv Drive number
 * @param buff Buffer to write from
 * @param sector Sector to start from
 * @param count Count of sectors to write too
 *
 * @retval   Result of SD write
 */
DRESULT sd_write(BYTE drv, const BYTE* buff, DWORD sector, UINT count)
{
    if (drv || !count)
        return RES_PARERR; /* Check parameter */
    if (CardStatus & STA_NOINIT)
        return RES_NOTRDY; /* Check drive status */
    if (CardStatus & STA_PROTECT)
        return RES_WRPRT; /* Check write protect */

    if (!(CardType & CT_BLOCK))
        sector *= 512; /* LBA ==> BA conversion (byte addressing cards) */

    if (count == 1)
    {                                      /* Single sector write */
        if ((send_cmd(CMD24, sector) == 0) /* WRITE_BLOCK */
            && xmit_datablock(buff, 0xFE))
        {
            count = 0;
        }
    }
    else
    { /* Multiple sector write */
        if (CardType & CT_SDC)
            send_cmd(ACMD23, count); /* Predefine number of sectors */
        if (send_cmd(CMD25, sector) == 0)
        { /* WRITE_MULTIPLE_BLOCK */
            do {
                if (!xmit_datablock(buff, 0xFC))
                    break;
                buff += 512;
            } while (--count);
            if (!xmit_datablock(0, 0xFD))
                count = 1; /* STOP_TRAN token */
        }
    }
    card_deselect();

    return count ? RES_ERROR : RES_OK; /* Return result */
}

/**
 * End of FatFS Generic Interface
 */
