/*
 * UDS.c
 * UDS module implementation
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "UDS.h"

// other includes
#include "HW.h"
#include "HW_FLASH.h"
#include "libcrc.h"
#include "Types.h"
#include "uds.h"
#include "Utilities.h"

// system includes
#include <string.h>

#ifndef ISO_TP_USER_DEBUG_ENABLED
#define ISO_TP_USER_DEBUG_ENABLED 0U
#endif

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern uint16_t isotp_user_send_can(const uint32_t id, const uint8_t data[], const uint8_t len);
extern uint32_t isotp_user_get_ms(void);
#if ISO_TP_USER_DEBUG_ENABLED
extern void     isotp_user_debug(const char* message, ...);
#endif


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#if UDS_DOWNLOAD_USE_CRC
# if UDS_DOWNLOAD_CRC_SIZE % 8U
#  error "UDS_DOWNLOAD_CRC_SIZE must be a multiple of 8"
# endif
# define UDS_DOWNLOAD_BLOCK_SIZE_OFFSET    (1U + (UDS_DOWNLOAD_CRC_SIZE / 8U))
#else
# define UDS_DOWNLOAD_BLOCK_SIZE_OFFSET    1U
#endif

#define UDS_DOWNLOAD_BUFFER_SIZE           4U


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint16_t          appBootTimer;     // [ms] countdown timer until app will boot, unless inhibited by UDS

    udsDownloadDesc_S downloadDesc;     // descriptor of the current download, if one is in progress
    uint8_t           downloadCounter;  // sequence counter for the current download
    uint32_t          downloadAddrCurr; // current address for writing the downloaded payload
    uint32_t          downloadedBytes;  // how many words have been downloaded so far


    uint8_t           downloadBufferSize; // current end pos in the download buffer
    union
    {
        uint8_t  u8[UDS_DOWNLOAD_BUFFER_SIZE];
        uint32_t u32[UDS_DOWNLOAD_BUFFER_SIZE / 4U];
    } downloadBuffer;

    struct
    {
        bool udsTimoutExpired:1;
        bool downloadStarted :1;
    } bit;
} uds_S;

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static uds_S uds;


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void checkTimeoutExpired_1kHz(void)
{
    if ((udsSrv_timeSinceLastTp() < 5U)
        || (udsSrv_getCurrentSession() != UDS_SESSION_TYPE_DEFAULT)
        )
    {
        uds.appBootTimer         = UDS_INACTIVITY_TIMER;
        uds.bit.udsTimoutExpired = false;
    }
    else if (uds.appBootTimer == 0U)
    {
        uds.bit.udsTimoutExpired = true;
    }
    else
    {
        uds.appBootTimer -= (uds.appBootTimer > 0U) ? 1U : 0U;
    }
}

/*
 * routine_0xf00f
 * @brief called when UDS routine 0xf00f is requested. Erases the app
 * @param routineControlType whether to start, stop, or get results of routine
 * @param payload payload data array
 * @param payloadLengthBytes payload data array length
 */
static void routine_0xf00f(udsRoutineControlType_E routineControlType, uint8_t *payload, uint8_t payloadLengthBytes)
{
    UNUSED(payloadLengthBytes);
    UNUSED(payload);

    switch (routineControlType)
    {
        case UDS_ROUTINE_CONTROL_START:
            if (FLASH_eraseAppStart())
            {
                uds_sendPositiveResponse(UDS_SID_ROUTINE_CONTROL, UDS_ROUTINE_CONTROL_START, payload, 0x02);
            }
            else
            {
                uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_PROGRAMMING_FAILED);
            }
            break;

        case UDS_ROUTINE_CONTROL_GET_RESULT:
        {
            // FIXME: this should probably be an enum with the different possible outcomes
            uint8_t            resp;
            FLASH_eraseState_S state = FLASH_getEraseState();

            // erase was never started
            if (!state.started)
            {
                uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_GENERAL_REJECT);
            }
            // failed
            else if (state.completed && !state.status)
            {
                resp = 0U;
                uds_sendPositiveResponse(UDS_SID_ROUTINE_CONTROL, UDS_ROUTINE_CONTROL_GET_RESULT, &resp, 0x01);
                // clear the state once queried since the erase failed
                FLASH_eraseCompleteAck();
            }
            // in-progress
            else if (state.started && !state.completed)
            {
                resp = 1U;
                uds_sendPositiveResponse(UDS_SID_ROUTINE_CONTROL, UDS_ROUTINE_CONTROL_GET_RESULT, &resp, 0x01);
            }
            // completed and successful
            else if (state.completed && state.status)
            {
                resp = 2U;
                uds_sendPositiveResponse(UDS_SID_ROUTINE_CONTROL, UDS_ROUTINE_CONTROL_GET_RESULT, &resp, 0x01);
                // clear the state once queried if completed
                FLASH_eraseCompleteAck();
            }

            break;
        }

        case UDS_ROUTINE_CONTROL_STOP:
        case UDS_ROUTINE_CONTROL_NONE:
        default:
            uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
            break;
    }
}


/*
 * stopDownload
 * @brief reset internal variables tracking download state
 */
static void stopDownload(void)
{
    uds.bit.downloadStarted   = false;
    uds.downloadCounter       = 0U;
    uds.downloadAddrCurr      = 0U;
    uds.downloadBuffer.u32[0] = 0xFF;
    uds.downloadedBytes       = 0U;
    memset(&uds.downloadDesc, 0x00, sizeof(udsDownloadDesc_S));
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * UDS_init
 * @brief initialize the UDS module
 */
void UDS_init(void)
{
    udsSrv_init();    // initialize the UDS library
    uds.appBootTimer = UDS_STARTUP_WAIT;
}

/*
 * UDS_periodic
 * @brief periodic UDS function. Needs to be called at 1kHz
 */
void UDS_periodic_1kHz(void)
{
    udsSrv_periodic();
    checkTimeoutExpired_1kHz();
}

/*
 * UDS_shouldInhibitBoot
 * @brief whether the UDS module thinks boot should be inhibited or not
 *        (i.e. whether there is UDS active)
 */
bool UDS_shouldInhibitBoot(void)
{
    return !uds.bit.udsTimoutExpired;
}

/*
 * UDS_extendBootTimeout
 * @brief extends how much longer UDS will block boot (saturated at UDS_INACTIVITY_TIMER)
 * @param timeoutMs number of milliseconds to extend the boot timeout by
 */
void UDS_extendBootTimeout(uint8_t timeoutMs)
{
    uds.appBootTimer = (uds.appBootTimer + timeoutMs >= UDS_INACTIVITY_TIMER)
                        ? UDS_INACTIVITY_TIMER
                        : uds.appBootTimer + timeoutMs;
}


/******************************************************************************
 *                U D S   L I B R A R Y   C A L L B A C K S
 ******************************************************************************/

/*
 * uds_cb_ecuReset
 * @brief callback for uds ecu reset service
 */
void uds_cb_ecuReset(udsResetType_E resetType)
{
    switch (resetType)
    {
        case UDS_RESET_TYPE_HARD:
            // have to send positive response from here since this function
            // won't return in this case
            uds_sendPositiveResponse(UDS_SID_ECU_RESET, resetType, NULL, 0U);

            // one day we can have an actual check here to see if the ecu is ready to reset
            // i.e. check eeprom writes finished, etc.
            BLOCKING_DELAY(1000);
            SYS_resetHard();
            break;

        case UDS_RESET_TYPE_KEY:
        case UDS_RESET_TYPE_SOFT:
        case UDS_RESET_RAPID_SHUTDOWN_ENABLE:
        case UDS_RESET_RAPID_SHUTDOWN_DISABLE:
            // not implemented
            uds_sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
            break;

        default:
            uds_sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_GENERAL_REJECT);
            break;
    }
}


/*
 * uds_cb_routineControl
 * @brief callback for uds routine control service
 */
void uds_cb_routineControl(udsRoutineControlType_E routineControlType, uint8_t *payload, uint8_t payloadLengthBytes)
{
    if (payloadLengthBytes < 2U)
    {
        uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_INVALID_LEN_FORMAT);
        return;
    }

    union
    {
        uint8_t  u8[2U];
        uint16_t u16;
    } routineId;

    memcpy(routineId.u8, payload, 2);

    switch (routineId.u16)
    {
        case 0xf00f:
            routine_0xf00f(routineControlType, payload, payloadLengthBytes);
            break;

        default:
            uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_SERVICE_NOT_SUPPORTED);
            break;
    }
}


/*
 * uds_cb_transferStart
 * @brief starts a UDS transfer
 * @param transferType download or upload
 * @param payload payload data array
 * @param payloadLengthBytes payload data array length
 */
void uds_cb_transferStart(udsTransferType_E transferType, uint8_t* payload, uint8_t payloadLengthBytes)
{
    // only support downloads for now
    if (transferType == UDS_TRANSFER_TYPE_UPLOAD)
    {
        uds_sendNegativeResponse(UDS_SID_UPLOAD_START, UDS_NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    // if download is already in progress, reject new one
    if (uds.bit.downloadStarted)
    {
        uds_sendNegativeResponse(UDS_SID_DOWNLOAD_START, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // ensure payload is the right length for the data we expect to see in it
    // TODO: define why this is 10
    // should be obvious from looking at the definition of udsDownloaDesc_S, though
    if (payloadLengthBytes != 10U)
    {
        uds_sendNegativeResponse(UDS_SID_DOWNLOAD_START, UDS_NRC_INVALID_LEN_FORMAT);
        return;
    }

    udsDownloadDesc_S downloadDesc;
    downloadDesc.compressionType = (uint8_t)((payload[0] & 0xF0) >> 4U);
    downloadDesc.encryptionType  = (uint8_t)(payload[0] & 0x0F);
    downloadDesc.sizeLen         = (uint8_t)((payload[1] & 0xF0) >> 4U);
    downloadDesc.addrLen         = (uint8_t)(payload[1] & 0x0F);
    DIAG_PUSH()
    // *FORMAT-OFF*
    DIAG_IGNORE(-Wcast-align)
    // *FORMAT-ON*
    downloadDesc.size = *((uint32_t*)(&payload[2]));
    downloadDesc.addr = *((uint32_t*)(&payload[6]));
    DIAG_POP()

    // we expect address and download size to be 4 bytes (32 bits)
    if ((downloadDesc.addrLen != 4U) || (downloadDesc.sizeLen != 4U))
    {
        uds_sendNegativeResponse(UDS_SID_DOWNLOAD_START, UDS_NRC_INVALID_LEN_FORMAT);
        return;
    }

    // ensure download address is actually in the app's flash region
    if ((downloadDesc.addr < APP_FLASH_START) || (downloadDesc.addr >= APP_FLASH_END))
    {
        uds_sendNegativeResponse(UDS_SID_DOWNLOAD_START, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // ensure requested download size doesn't exceed app's flash
    if ((APP_FLASH_START + downloadDesc.size) >= APP_FLASH_END)
    {
        uds_sendNegativeResponse(UDS_SID_DOWNLOAD_START, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // if we made it to this point, all the parameters were acceptable, so allow the download to start
    uds.downloadDesc          = downloadDesc;
    uds.downloadAddrCurr      = downloadDesc.addr;
    uds.bit.downloadStarted   = true;
    uds.downloadBufferSize    = 0U;
    uds.downloadBuffer.u32[0] = 0xFFU;
    uds.downloadedBytes       = 0U;

    UDS_extendBootTimeout(10);

    // respond with the length of the size parameter, and the size parameter itself
    uint8_t resp[2] = { (1U << 4U), UDS_DOWNLOAD_MAX_BLOCK_SIZE };
    uds_sendPositiveResponse(UDS_SID_DOWNLOAD_START, 0x00, resp, 2U);
}


/*
 * uds_cb_transferStop
 * @brief stops a UDS transfer
 * @param payload payload data array
 * @param payloadLengthBytes payload data array length
 */
void uds_cb_transferStop(uint8_t* payload, uint8_t payloadLengthBytes)
{
    UNUSED(payload);
    UNUSED(payloadLengthBytes);

    // write any leftover bytes, padding as necessary if there are fewer than a word's worth
    if (uds.downloadBufferSize)
    {
        if (uds.downloadBufferSize < 4U)
        {
            // make sure any empty bytes are actually set to 0xFF
            memset(&uds.downloadBuffer.u8[uds.downloadBufferSize], 0xFF, 4U - uds.downloadBufferSize);
        }

        bool result = FLASH_writeWord(uds.downloadAddrCurr, uds.downloadBuffer.u32[0]);

        uds.downloadBuffer.u32[0U] = 0U;
        uds.downloadBufferSize     = 0U;

        if (!result)
        {
            uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_PROGRAMMING_FAILED);
            // reset the current download
            stopDownload();
            return;
        }
    }


    if (uds.bit.downloadStarted)
    {
        // reset the current download
        stopDownload();
    }

    // send positive response either way cause who cares
    uds_sendPositiveResponse(UDS_SID_TRANSFER_STOP, 0x00, NULL, 0U);
}

/*
 * uds_cb_transferPayload
 * @brief receives a block of the transfer payload
 * @param payload payload data array
 * @param payloadLengthBytes payload data array length
 */
void uds_cb_transferPayload(uint8_t *payload, uint8_t payloadLengthBytes)
{
    // only proceed if download has actually been started
    if (!uds.bit.downloadStarted)
    {
        uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    // counter byte isn't counted in UDS_DOWNLOAD_MAX_BLOCK_SIZE, but checksum is
    if ((payloadLengthBytes < 2U)
        || (payloadLengthBytes > (UDS_DOWNLOAD_MAX_BLOCK_SIZE + 1U))
        )
    {
        uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_INVALID_LEN_FORMAT);
        // reset the current download
        stopDownload();
        return;
    }

    // fail if the client is trying to send more data than they originally said they would
    if ((payloadLengthBytes - UDS_DOWNLOAD_BLOCK_SIZE_OFFSET) + uds.downloadedBytes > uds.downloadDesc.size)
    {
        uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_REQUEST_OUT_OF_RANGE);
        stopDownload();
        return;
    }

    // first byte of payload is the block sequence counter
    const uint8_t counter = payload[0U];
    if (counter != uds.downloadCounter)
    {
        uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_BLOCK_SEQ_COUNTER_ERROR);
        // reset the current download
        stopDownload();
        return;
    }

    // note: the overflow here is intentional, this counter is supposed
    // to roll over when it caps out at 0xFF
    uds.downloadCounter++;

    // payload structure is as follows:
    // |  counter  |  first payload byte  |  ...  |  last payload byte  |  checksum byte (if enabled)  |
    // so payload length needs to be offset to get the data length
    const uint8_t dataLengthBytes = payloadLengthBytes - UDS_DOWNLOAD_BLOCK_SIZE_OFFSET;


#if UDS_DOWNLOAD_USE_CRC
    // verify the checksum of the received data
    const uint8_t calculatedCrc = crc8_calculate(0xFF, &payload[1U], dataLengthBytes);
    const uint8_t payloadCrc    = payload[payloadLengthBytes - 1U];
    if (calculatedCrc != payloadCrc)
    {
        uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_XFER_REJECTED);
        return;
    }
#endif // if UDS_DOWNLOAD_USE_CRC

    bool    result        = true; // store the result of the flash writes
    uint8_t payloadOffset = 0U;

    // start by handling anything leftover in the buffer
    if (uds.downloadBufferSize)
    {
        const uint8_t bytesRequired = 4U - uds.downloadBufferSize;
        // if there are enough bytes in the payload to write a word, do so
        if (dataLengthBytes >= bytesRequired)
        {
            memcpy(&uds.downloadBuffer.u8[uds.downloadBufferSize], &payload[1], bytesRequired);
            result                = FLASH_writeWord(uds.downloadAddrCurr, *uds.downloadBuffer.u32);
            uds.downloadAddrCurr += 4U;


            // track how many bytes from this payload we've used
            payloadOffset             = bytesRequired;
            uds.downloadBufferSize    = 0U;
            uds.downloadBuffer.u32[0] = 0xFFU;
        }
    }

    // if there is less than a word of data in the payload, store it in the buffer.
    // at this point, we know the buffer is either empty or has enough room left
    // to store whatever is left in the payload (less than 1 word)
    if (result && (dataLengthBytes < (4U + payloadOffset)))
    {
        memcpy(&uds.downloadBuffer.u8[uds.downloadBufferSize], &payload[1U + payloadOffset], dataLengthBytes - payloadOffset);
        uds.downloadBufferSize = (dataLengthBytes - payloadOffset) + 1U;
    }
    // if there is at least one word left in the payload, write it to flash.
    // if we get to this point, we know the buffer is empty
    else if (result)
    {
        const uint8_t dataLengthWords = (dataLengthBytes - payloadOffset) / 4U;

        if (dataLengthWords > 0U)
        {
            DIAG_PUSH()
            // *FORMAT-OFF*
            DIAG_IGNORE(-Wcast-align)
            // *FORMAT-ON*
            uint32_t *payloadPtr = (uint32_t*)&payload[1U + payloadOffset];
            DIAG_POP()

            // write as much of the data as we can
            result                = result && FLASH_writeWords(uds.downloadAddrCurr, payloadPtr, dataLengthWords);
            uds.downloadAddrCurr += (dataLengthWords * 4U);
        }

        // now store any data that was left in the payload, which will be less than a word (if any)
        // we know the buffer is empty, so this remainder must be able to fit in it
        if (result && ((dataLengthBytes - payloadOffset) % 4))
        {
            // clear the last two bits to effectively floor divide by 4, and then multiply by 4
            uint8_t remainingBytes = (dataLengthBytes - payloadOffset) - ((dataLengthBytes - payloadOffset) & 0xFC);
            while ((remainingBytes > 4U) || (remainingBytes == 0U))
            {
                // this shouldn't be possible. Sit in an infinite loop for debugging purposes
                // FIXME: this probably shouldn't be an infinite loop
            }

            // copy the leftover bytes into the buffer to be processed later
            memcpy(&uds.downloadBuffer.u8[uds.downloadBufferSize], &payload[payloadLengthBytes - remainingBytes - 1U], remainingBytes);
            uds.downloadBufferSize += remainingBytes;

            while (uds.downloadBufferSize > 3U)
            {
                // this shouldn't be possible. Sit in an infinite loop for debugging purposes
                // FIXME: this probably shouldn't be an infinite loop
            }
        }
    }

    if (!result)
    {
        uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_PROGRAMMING_FAILED);
        // reset the current download
        stopDownload();
        return;
    }


    UDS_extendBootTimeout(10);
    uds_sendPositiveResponse(UDS_SID_TRANSFER, 0x00, payload, 1U);
}


/*
 * uds_cb_DIDRead
 * @brief callback for a data ID read
 */
void uds_cb_DIDRead(uint8_t *payload, uint8_t payloadLengthBytes)
{
    if (payloadLengthBytes != 2U)
    {
        uds_sendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_INVALID_LEN_FORMAT);
        return;
    }

    union
    {
        uint8_t  u8[2U];
        uint16_t u16;
    } did;

    memcpy(did.u8, payload, 2);

    switch (did.u16)
    {
        case 0x101:
        {
            // always respond with 0x00 since we're in the bootloader
            uint8_t data = 0x00;
            uds_sendPositiveResponse(UDS_SID_READ_DID, 0x00, &data, 1);
            break;
        }

        default:
            uds_sendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_GENERAL_REJECT);
            break;
    }
}


/******************************************************************************
 *                U D S   L I B R A R Y   F U N C T I O N S
 ******************************************************************************/
// define functions required for isotp library

uint16_t isotp_user_send_can(const uint32_t id, const uint8_t data[], const uint8_t len)
{
    CAN_TxMessage_S msg = {
        .id          = id,
        .lengthBytes = len,
        .data        = { 0U },
    };

    memcpy(&msg.data, data, len);

    return CAN_sendMsg(msg);
}


uint32_t isotp_user_get_ms(void)
{
    return (uint32_t)TIM_getTimeMs();
}


#if ISO_TP_USER_DEBUG_ENABLED
extern void isotp_user_debug(const char* message, ...);
void        isotp_user_debug(const char* message, ...)
{
    UNUSED(message);
}
#endif
