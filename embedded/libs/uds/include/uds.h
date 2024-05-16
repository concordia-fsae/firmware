/*
 * uds.h
 * UDS implementation header
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <stdbool.h>
#include <stdint.h>


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define UDS_SUB_FUNCTION_NONE                 0x00
#define UDS_RESPONSE_NOT_REQUIRED             0x80

#define uds_sendPositiveResponseEmpty(sid)    uds_sendPositiveResponse(sid, 0x00, NULL, 0U)


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    UDS_RESULT_SUCCESS = 0U,
    UDS_RESULT_NOT_INITIALIZED,
    UDS_RESULT_INCORRECT_PAYLOAD_LENGTH,
} udsResult_E;


typedef enum
{
    UDS_RESPONSE_OK = 0U,
    UDS_RESPONSE_NOT_OK,
} udsResponse_E;


// All UDS service identifiers
// Response ID is the request ID + 0x40
// https://www.csselectronics.com/cdn/shop/files/Unified-Diagnostic-Services-UDS-overview-0x22-0x19.png
typedef enum
{
    UDS_SID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_SID_ECU_RESET                  = 0x11,
    UDS_SID_SECURITY_ACCESS            = 0x27,
    UDS_SID_COMMUNICATION_CONTROL      = 0x28,
    UDS_SID_AUTHENTICATION             = 0x29,
    UDS_SID_TESTER_PRESENT             = 0x3E,
    UDS_SID_ACCESS_TIMING_PARAMETERS   = 0x83,
    UDS_SID_SECURED_DATA_TRANSMISSION  = 0x84,
    UDS_SID_CONTROL_DTC_SETTINGS       = 0x85,
    UDS_SID_RESPONSE_ON_EVENT          = 0x86,
    UDS_SID_LINK_CONTROL               = 0x87,
    UDS_SID_READ_DID                   = 0x22,
    UDS_SID_READ_ADDRESS               = 0x23,
    UDS_SID_READ_SCALING_DID           = 0x24,
    UDS_SID_READ_DID_PERIODIC          = 0x2A,
    UDS_SID_DYNAMICALLY_DEFINE_DID     = 0x2C,
    UDS_SID_WRITE_DID                  = 0x2E,
    UDS_SID_WRITE_ADDRESS              = 0x3D,
    UDS_SID_CLEAR_DTC                  = 0x14,
    UDS_SID_READ_DTC                   = 0x19,
    UDS_SID_IO_CONTROL                 = 0x2F,
    UDS_SID_ROUTINE_CONTROL            = 0x31,
    UDS_SID_DOWNLOAD_START             = 0x34,
    UDS_SID_UPLOAD_START               = 0x35,
    UDS_SID_TRANSFER                   = 0x36,
    UDS_SID_TRANSFER_STOP              = 0x37,
    UDS_SID_FILE_TRANSFER              = 0x38,
    UDS_SID_NEGATIVE_RESPONSE_CODE     = 0x7F,
} udsServiceId_E;


typedef enum
{
    UDS_SESSION_TYPE_NONE               = 0x00,
    UDS_SESSION_TYPE_DEFAULT            = 0x01,
    UDS_SESSION_TYPE_PROGRAMMING        = 0x02,
    UDS_SESSION_TYPE_EXTENDED_DIAG      = 0x03,
    UDS_SESSION_TYPE_SAFETY_DIAG        = 0x04,
    UDS_SESSION_RESERVED_START          = 0x05,
    UDS_SESSION_RESERVED_END            = 0x3F,
    UDS_SESSION_OEM_SPECIFIC_START      = 0x40,
    UDS_SESSION_OEM_SPECIFIC_END        = 0x5F,
    UDS_SESSION_SUPPLIER_SPECIFIC_START = 0x60,
    UDS_SESSION_SUPPLIER_SPECIFIC_END   = 0x7E,
    UDS_SESSION_RESERVED                = 0x7F,
} udsSessionType_E;


typedef enum
{
    UDS_RESET_TYPE_HARD = 1U,
    UDS_RESET_TYPE_KEY,
    UDS_RESET_TYPE_SOFT,
    UDS_RESET_RAPID_SHUTDOWN_ENABLE,
    UDS_RESET_RAPID_SHUTDOWN_DISABLE,
} udsResetType_E;


typedef enum
{
    UDS_ROUTINE_CONTROL_NONE = 0x00,
    UDS_ROUTINE_CONTROL_START,
    UDS_ROUTINE_CONTROL_STOP,
    UDS_ROUTINE_CONTROL_GET_RESULT,
} udsRoutineControlType_E;


typedef enum
{
    UDS_TRANSFER_TYPE_NONE = 0x00,
    UDS_TRANSFER_TYPE_DOWNLOAD,
    UDS_TRANSFER_TYPE_UPLOAD,
} udsTransferType_E;


typedef enum
{
    UDS_NRC_NONE                               = 0x00,
    UDS_NRC_GENERAL_REJECT                     = 0x10,
    UDS_NRC_SERVICE_NOT_SUPPORTED              = 0x11,
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED         = 0x12,
    UDS_NRC_INVALID_LEN_FORMAT                 = 0x13,
    UDS_NRC_RESPONSE_TOO_LONG                  = 0x14,
    UDS_NRC_BUSY_REPEAT_REQUEST                = 0x21,
    UDS_NRC_CONDITIONS_NOT_CORRECT             = 0x22,
    UDS_NRC_REQUEST_SEQ_ERROR                  = 0x24,
    UDS_NRC_SUBNET_NO_RESPONSE                 = 0x25,
    UDS_NRC_FAILURE_ABORT                      = 0x26,
    UDS_NRC_REQUEST_OUT_OF_RANGE               = 0x31,
    UDS_NRC_ACCESS_DENIED                      = 0x33,
    UDS_NRC_KEY_INVALID                        = 0x35,
    UDS_NRC_TOO_MANY_ATTEMPTS                  = 0x36,
    UDS_NRC_DELAY_NOT_SATISFIED                = 0x37,
    UDS_NRC_XFER_REJECTED                      = 0x70,
    UDS_NRC_XFER_SUSPENDED                     = 0x71,
    UDS_NRC_PROGRAMMING_FAILED                 = 0x72,
    UDS_NRC_BLOCK_SEQ_COUNTER_ERROR            = 0x73,
    UDS_NRC_RESPONSE_PENDING                   = 0x78,
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_SESSION = 0x7E,
    UDS_NRC_SERVICE_NOT_SUPPORTED_SESSION      = 0x7F,
    UDS_NRC_RPM_TOO_HIGH                       = 0x81,
    UDS_NRC_RPM_TOO_LOW                        = 0x82,
    UDS_NRC_ENGINE_ON                          = 0x83,
    UDS_NRC_ENGINE_OFF                         = 0x84,
    UDS_NRC_ENGINE_ON_TIME_LOW                 = 0x85,
    UDS_NRC_TEMP_TOO_HIGH                      = 0x86,
    UDS_NRC_TEMP_TOO_LOW                       = 0x87,
    UDS_NRC_SPEED_TOO_HIGH                     = 0x88,
    UDS_NRC_SPEED_TOO_LOW                      = 0x89,
    UDS_NRC_THROTTLE_TOO_HIGH                  = 0x8A,
    UDS_NRC_THROTTLE_TOO_LOW                   = 0x8B,
    UDS_NRC_GEAR_NOT_NEUTRAL                   = 0x8C,
    UDS_NRC_GEAR_NOT_D_OR_R                    = 0x8D,
    UDS_NRC_BRAKE_NOT_PRESSED                  = 0x8F,
    UDS_NRC_SHIFTER_NOT_PARK                   = 0x90,
    UDS_NRC_TORQUE_CONVERTER_CLUTCH_LOCKED     = 0x91,
    UDS_NRC_VOLTAGE_TOO_HIGH                   = 0x92,
    UDS_NRC_VOLTAGE_TOO_LOW                    = 0x93,
    UDS_MANUF_SPECIFIC_START                   = 0xF0,
    UDS_MANUF_SPECIFIC_END                     = 0xFE,
} udsNegativeResponse_E;


typedef struct
{
    udsServiceId_E id;
    uint8_t        subFunction;
    uint16_t       payloadLengthBytes;
    uint8_t        *payload;
} udsResponseDesc_S;


typedef struct
{
    udsServiceId_E                           id;
    uint16_t                                 payloadLengthBytes;
    __attribute__((__aligned__(4U))) uint8_t *payload;
} udsRequestDesc_S;


typedef struct
{
    // putting these first even though it doesn't align with the actual
    // layout in the frame because the required alignment will otherwise
    // make this take up more space than it needs to
    __attribute__((__aligned__(4U))) uint32_t size;
    __attribute__((__aligned__(4U))) uint32_t addr;
    uint8_t compressionType:4U;
    uint8_t encryptionType :4U;
    uint8_t sizeLen        :4U;
    uint8_t addrLen        :4U;
} udsDownloadDesc_S;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

// server specific functions
void             udsSrv_init(void);
udsResult_E      udsSrv_processMessage(uint8_t data[], uint8_t dlc);
udsResult_E      udsSrv_send(udsResponseDesc_S response);
udsResult_E      udsSrv_periodic(void);
udsSessionType_E udsSrv_getCurrentSession(void);
uint32_t         udsSrv_timeSinceLastTp(void);


// general functions
udsResult_E uds_sendNegativeResponse(udsServiceId_E sid, udsNegativeResponse_E nrc);
udsResult_E uds_sendPositiveResponse(udsServiceId_E sid, uint8_t subFunction, uint8_t *payload, uint8_t payloadLengthBytes);
bool        uds_checkResponseRequired(udsRequestDesc_S *req);


/******************************************************************************
 *                    C A L L B A C K   P R O T O T Y P E S
 ******************************************************************************/

/*
 * uds_cb_sessionChangeAllowed
 * @brief user function which returns UDS_NRC_NONE if the session transition
 *        should be allowed, or a negative response code otherwise
 * @param currentSession udsSessionType_E the current session type
 * @param requestedSession udsSessionType_E the requested session type
 */
udsNegativeResponse_E uds_cb_sessionChangeAllowed(udsSessionType_E currentSession, udsSessionType_E requestedSession);

/*
 * uds_cb_ecuReset
 * @brief performs a reset of the component
 * @param resetType udsResetType_E the type of reset to perform
 */
void uds_cb_ecuReset(udsResetType_E resetType);


/*
 * uds_cb_routineControl
 * @brief callback after a uds routine control request
 */
void uds_cb_routineControl(udsRoutineControlType_E routineControlType, uint8_t *payload, uint8_t payloadLengthBytes);


/*
 * uds_cb_transferStart
 * @brief callback after a uds transfer start request
 */
void uds_cb_transferStart(udsTransferType_E transferType, uint8_t *payload, uint8_t payloadLengthBytes);


/*
 * uds_cb_transferStop
 * @brief callback after a uds transfer stop request
 */
void uds_cb_transferStop(uint8_t *payload, uint8_t payloadLengthBytes);


/*
 * uds_cb_transfer
 * @brief callback after a uds transfer payload request
 */
void uds_cb_transferPayload(uint8_t *payload, uint8_t payloadLengthBytes);
