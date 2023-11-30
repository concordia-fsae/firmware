/**
 * @file fatfs.h
 * @brief  Source code for the FatFS interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-16
 *
 * @note
 * Portions copyright (C) 2014, ChaN, all rights reserved.
 * Portions copyright (C) 2017, kiwih, all rights reserved.
 *
 * This software is a free software and there is NO WARRANTY.
 * No restriction on use. You can use, modify and redistribute it for
 * personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
 * Redistributions of source code must retain the above copyright notice.
 *
 ******************************************************************************
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "fatfs.h"
#include "ff_gen_drv.h"
#include "integer.h"

#include "HW_spi.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define SD_SPI_INTERFACE ((SPI_TypeDef*)SPI2) /**< SD Interface on SPI2 */
#define SD_INIT_TIMEOUT  100
#define SD_WAIT_TIMEOUT  500
#define SD_RCVR_TIMEOUT  200

#define FCLK_SLOW() { MODIFY_REG(spi2->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_128); } /* Set SCLK = slow, 250 KBits/s*/
#define FCLK_FAST() { MODIFY_REG(spi2->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_8); } /* Set SCLK = fast, 4 MBits/s */

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
static uint8_t wait_ready(uint32_t wait);
static uint8_t card_select(void);
static void    card_deselect(void);
static uint8_t xchg_sd(BYTE data);
static void    rcvr_spi_multi(BYTE* buff, UINT btr);
static void    xmit_spi_multi(const BYTE* buff, UINT btx);
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
static_assert(sizeof(UINT) == 4 || sizeof(UINT) == 2, "WRONG FatFS UINT TYPE SIZE");
DSTATUS sd_init(BYTE drv);
DSTATUS sd_status(BYTE drv);
DRESULT sd_read(BYTE byte, BYTE* buff, DWORD sector, UINT count);
DRESULT sd_write(BYTE drv, const BYTE* buff, DWORD sector, UINT count);
DRESULT sd_ioctl(BYTE drv,BYTE cmd,void *buff);
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
    .disk_ioctl      = &sd_ioctl,
};

static uint8_t CardStatus;
static uint8_t CardType;

uint32_t sdTimerTickStart;
uint32_t sdTimerTickDelay;

char path[4];

uint8_t semaphore = 0;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Link the driver to FatFs
 */
void FatFS_Init(void)
{
    FATFS_LinkDriver(&disk_driver, (char*)&path);
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * Start of Specific Internal Functions
 */

/**
 * @brief  Wait until SD is ready
 * FIXME: Currently in blocking mode
 *
 * @param wait Time to wait
 *
 * @retval   Result (1: Success, 0: Timeout)
 */
static uint8_t wait_ready(uint32_t wait)
{
    BYTE res;

    uint32_t timStart = HAL_GetTick();

    do {
        res = xchg_sd(0xff);
    } while (res != 0xff && (HAL_GetTick() - timStart < wait));

    return (res == 0xff) ? 1 : 0;
}

/**
 * @brief  Probe the sd card and wait for response
 *
 * @retval   1: OK, 0: Timout
 */
static inline uint8_t card_select(void)
{
	xchg_sd(0xFF);	/** Dummy clock (force DO enabled) */
	if (wait_ready(500)) return 1;	/** Wait for card ready */

	card_deselect();
	return 0;	/** Timeout */
}
/**
 * @brief  Give time for sd card before delecting it
 */
static inline void card_deselect(void)
{
    /**< Current implementation has no CS and no other SPI peripheral */
	xchg_sd(0xFF);	/** Dummy clock (force DO hi-z for multiple slave SPI) */
}

/**
 * @brief  Transmit and receive 8 bit
 *
 * @param data Data to be sent to the SD
 *
 * @retval   8 bits received from SD
 */
static inline uint8_t xchg_sd(BYTE data)
{
    return HW_SPI_TransmitReceive8(SD_SPI_INTERFACE, data);
}

/**
 * @brief Receive multiple bytes from spi
 *
 * @param buff Buffer to write to
 * @param btr Number of bytes to receive
 */
static void rcvr_spi_multi(BYTE* buff, UINT btr)
{
    for (UINT i = 0; i < btr; i++)
    {
        buff[i] = xchg_sd(0xff);
    }
}

/**
 * @brief  Transmit multiple bytes over spi
 *
 * @param buff Source buffer
 * @param btx Number of bytes to send
 */
static void xmit_spi_multi(const BYTE* buff, UINT btx)
{
    for (UINT i = 0; i < btx; i++)
    {
        xchg_sd(buff[i]);
    }
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
 * @retval   1: Valid, 0: Invalid
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

    if (cmd != CMD12 && cmd != CMD0) /**< Select card and wait for ready */
    {
        card_deselect();
        if (!card_select())
            return 0xff;
    }

    /**< Send command packet */
    xchg_sd(0x40 | cmd); /**< Start + Command */
    xchg_sd((BYTE)(arg >> 24));
    xchg_sd((BYTE)(arg >> 16));
    xchg_sd((BYTE)(arg >> 8)); 
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

     /**< Current implementation has no CS and no other SPI peripheral */
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

    if (!wait_ready(SD_WAIT_TIMEOUT))
        return 0; /**< Wait for ready */

    xchg_sd(token); /**< Send token */

    if (token != 0xfd) /**< Send data if token isnt StopTrans */
    {
        xmit_spi_multi(buff, 512); /**< Send sector */
        xchg_sd(0xff);
        xchg_sd(0xff); /**< Dummy CRC */

        resp = xchg_sd(0xff); /**< Receive data response */
        if ((resp & 0x1f) != 0x05)
            return 0;
    }
    return 1;
}

/**
 * @brief Receive a block of data from the SD card
 * FIXME: Currently in blocking mode
 *
 * @param buff Buffer to write too
 * @param btr Size in bytes
 *
 * @retval Integer value (1: OK, 0: Failed)
 */
static BYTE rcvr_datablock(BYTE* buff, uint32_t btr)
{
    BYTE token;

    start_timer(SD_RCVR_TIMEOUT);

    do {
        token = xchg_sd(0xff);
    } while ((token == 0xff) && timer_status());

    rcvr_spi_multi(buff, btr);
    xchg_sd(0xff);
    xchg_sd(0xff); /**< Dummy CRC */

    return 1;
}

/**
 * End of Specific Internal Functions
 */

/**
 * Start of FatFS Generic Interface
 */

/**
 * @brief  Initializes SD
 * FIXME: Currently in blocking mode
 *
 * @param drv Drive number
 *
 * @retval   Status
 */
DSTATUS sd_init(BYTE drv)
{
    uint8_t n, type, ocr[4], cmd;

    if (drv)
        return STA_NOINIT; /**< Support only drive 0 */

    FCLK_SLOW();
    for (n = 10; n; n--) xchg_sd(0xff);

    type = 0;

    if (send_cmd(CMD0, 0) == 1) /**< Set card to idle state */
    {
        start_timer(SD_INIT_TIMEOUT);
        if (send_cmd(CMD8, 0x1AA) == 1) /**< Is card SDv2? */
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
            while (timer_status() && send_cmd(cmd, 0)); /**< Wait for end of initialization */
            if (!timer_status() || send_cmd(CMD16, 512) != 0) /**< Set blocksize = 512 */
            {
                type = 0;
            }
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
 * @brief  Control device specific features + miscellaneous functions other than Read/Write
 * @param drv Drive number
 * @param cmd Control command code
 * @param buff Buffer to write from
 * @retval   Result of SD ioctl
 */
#if _USE_IOCTL
inline DRESULT sd_ioctl (
	BYTE drv,		/* Physical drive number (0) */
	BYTE cmd,		/* Control command code */
	void *buff		/* Pointer to the conrtol data */
)
{
	DRESULT res;
	BYTE n, csd[16];
	DWORD *dp, st, ed, csize;


	if (drv) return RES_PARERR;	/* Check parameter */


	res = RES_ERROR;

	switch (cmd) {
	case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
		if (card_select()) res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	            /* Get drive capacity in unit of sector (DWORD) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = csize << 10;
			} else {					/* SDC ver 1.XX or MMC ver 3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :	                /* Get erase block size in unit of sector (DWORD) */
		if (CardType & CT_SD2) {	        /* SDC ver 2.00 */
			if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				xchg_sd(0xFF);
				if (rcvr_datablock(csd, 16)) {				    /* Read partial block */
					for (n = 64 - 16; n; n--) xchg_sd(0xFF);	/* Purge trailing data */
					*(DWORD*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					        /* SDC ver 1.XX or MMC */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
				if (CardType & CT_SD1) {	/* SDC ver 1.XX */
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMC */
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case CTRL_TRIM :	                                /* Erase a block of sectors (used when _USE_ERASE == 1) */
		if (!(CardType & CT_SDC)) break;				/* Check if the card is SDC */
		if (sd_ioctl(drv, MMC_GET_CSD, csd)) break;	    /* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
		dp = buff; st = dp[0]; ed = dp[1];				/* Load sector block */
		if (!(CardType & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000)) {	/* Erase sector block */
			res = RES_OK;	/* FatFs does not check result of this command */
		}
		break;

	default:
		res = RES_PARERR;
	}

	card_deselect();

	return res;
}
#endif


/**
 * End of FatFS Generic Interface
 */

/*------------------------------------------------------------------------*/
/* Create a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount() function to create a new
/  synchronization object, such as semaphore and mutex. When a 0 is returned,
/  the f_mount() function fails with FR_INT_ERR.
*/

int ff_cre_syncobj(              /* 1:Function succeeded, 0:Could not create the sync object */
                   BYTE     vol, /* Corresponding volume (logical drive number) */
                   _SYNC_t* sobj /* Pointer to return the created sync object */
)
{
    UNUSED(vol);
    UNUSED(sobj);
    
    return 1;
}


/*------------------------------------------------------------------------*/
/* Delete a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount() function to delete a synchronization
/  object that created with ff_cre_syncobj() function. When a 0 is returned,
/  the f_mount() function fails with FR_INT_ERR.
*/

int ff_del_syncobj(             /* 1:Function succeeded, 0:Could not delete due to any error */
                   _SYNC_t sobj /* Sync object tied to the logical drive to be deleted */
)
{
    UNUSED(sobj);
    return 1;
}


/*------------------------------------------------------------------------*/
/* Request Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on entering file functions to lock the volume.
/  When a 0 is returned, the file function fails with FR_TIMEOUT.
*/

int ff_req_grant(             /* 1:Got a grant to access the volume, 0:Could not get a grant */
                 _SYNC_t sobj /* Sync object to wait */
)
{
    UNUSED(sobj);
    if (semaphore)
    {
        return 0;
    }
    semaphore = ~0x00;
    return 1;
}


/*------------------------------------------------------------------------*/
/* Release Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on leaving file functions to unlock the volume.
 */

void ff_rel_grant(
    _SYNC_t sobj /* Sync object to be signaled */
)
{
    UNUSED(sobj);
    semaphore = 0;
}
