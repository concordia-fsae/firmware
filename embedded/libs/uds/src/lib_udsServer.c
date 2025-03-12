/*
 * uds.c
 * UDS implementation
 * https://www.csselectronics.com/pages/uds-protocol-tutorial-unified-diagnostic-services
 * https://embetronicx.com/tutorials/automotive/uds-protocol
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "lib_uds.h"

#if UDS_ENABLE_LIB
#include "isotp.h"

#include <stddef.h>    // NULL
#include <string.h>    // memset


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define UDS_SESSION_EXPIRE_TIME    100 // [ms] time without a tester present being received
                                       // after which the current session will be exited

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint32_t         lastTick;                 // [ms] last stored time step tick
    IsoTpLink        isoTpLink;                // isotp link definition struct
    uint8_t          txBuf[ISOTP_TX_BUF_SIZE]; // tx data buffer
    uint8_t          rxBuf[ISOTP_RX_BUF_SIZE]; // rx data buffer
    udsSessionType_E currentSession;           // current session type
    uint16_t         testerPresentTimerMs;     // [ms] time since last tester present received

    struct
    {
        bool initialized: 1;    // true if the uds module has been initialized
        bool unprocessed_message: 1; // true if a message was received 
    } bit;
} udsSrv_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static udsSrv_S udsSrv = { 0U };


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/


/*
 * manageSession
 * @brief manage the UDS session
 * @param requestedSession udsSessionType_E requested session type, or 0U for none
 */
static void manageSession(udsSessionType_E requestedSession)
{
    // change back to default session if we don't receive a TP frame
    // within the defined period
    if (udsSrv.testerPresentTimerMs >= UDS_SESSION_EXPIRE_TIME)
    {
        udsSrv.currentSession = UDS_SESSION_TYPE_DEFAULT;
    }
    else
    {
        udsSrv.testerPresentTimerMs++;
    }

    // check if a session change was requested
    if ((requestedSession != UDS_SESSION_TYPE_NONE))
    {
        // check if the requested transition should be allowed
        udsNegativeResponse_E allowed = uds_cb_sessionChangeAllowed(udsSrv.currentSession, requestedSession);

        if (allowed == UDS_NRC_NONE)
        {
            // if allowed, transition into requested session and send positive response
            udsSrv.currentSession       = requestedSession;
            udsSrv.testerPresentTimerMs = 0U;

            uds_sendPositiveResponse(UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                     requestedSession,
                                     NULL,
                                     0U);
        }
        else
        {
            // otherwise, send negative response
            uds_sendNegativeResponse(UDS_SID_DIAGNOSTIC_SESSION_CONTROL, allowed);
        }
    }

    uint32_t tick = isotp_user_get_ms();
    if (tick != udsSrv.lastTick)
    {
        udsSrv.lastTick = tick;
    }
}


/*
 * handleTesterPresent
 * @brief handle a received tester present request and respond if necessary
 */
static inline void handleTesterPresent(udsRequestDesc_S *req)
{
    // reset tester present timer whenever tp is received
    udsSrv.testerPresentTimerMs = 0U;

    // only send response if required
    if (uds_checkResponseRequired(req))
    {
        // uds_sendPositiveResponseEmpty(UDS_SID_TESTER_PRESENT);
        uint8_t resp = 0U;
        uds_sendPositiveResponse(UDS_SID_TESTER_PRESENT, 0x00, &resp, 1U);
    }
}


/*
 * handleDiagnosticSessionControl
 * @brief handle a received diagnostic session control request
 */
static inline void handleDiagnosticSessionControl(udsRequestDesc_S *req)
{
    if (req->payloadLengthBytes > 0U)
    {
        manageSession(req->payload[0]);
    }
}


static inline void handleEcuReset(udsRequestDesc_S *req)
{
#if defined(UDS_SERVICE_SUPPORTED_ECU_RESET)
    if (req->payloadLengthBytes > 0U)
    {
        uds_cb_ecuReset((udsResetType_E)req->payload[0]);
    }
#else // * if defined(UDS_SERVICE_SUPPORTED_ECU_RESET)
    uds_sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_SERVICE_NOT_SUPPORTED);
#endif // if defined(UDS_SERVICE_SUPPORTED_ECU_RESET)
}


static inline void handleRoutineControl(udsRequestDesc_S *req)
{
#if defined(UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL)
    udsRoutineControlType_E routineControlType = UDS_ROUTINE_CONTROL_NONE;
    uint8_t                 payloadLen         = 0U;
    uint8_t                 payloadStartIdx    = 0U;

    if (req->payloadLengthBytes > 0U)
    {
        routineControlType = req->payload[0U];
        if (req->payloadLengthBytes > 1U)
        {
            payloadLen      = req->payloadLengthBytes - 1U;
            payloadStartIdx = 1U;
        }
    }

    uds_cb_routineControl(
        routineControlType,
        &req->payload[payloadStartIdx],
        payloadLen
        );
#else // if defined(UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL)
    uds_sendNegativeResponse(UDS_SID_ROUTINE_CONTROL, UDS_NRC_SERVICE_NOT_SUPPORTED);
#endif // if defined(UDS_SERVICE_SUPPORTED_ROUTINE_CONTROL)
}


static inline void handleDownloadStart(udsRequestDesc_S *req)
{
#if defined(UDS_SERVICE_SUPPORTED_DOWNLOAD)
    uds_cb_transferStart(UDS_TRANSFER_TYPE_DOWNLOAD, req->payload, req->payloadLengthBytes);
#else
    uds_sendNegativeResponse(UDS_SID_DOWNLOAD_START, UDS_NRC_SERVICE_NOT_SUPPORTED);
#endif
}


static inline void handleTransfer(udsRequestDesc_S *req)
{
#if defined(UDS_SERVICE_SUPPORTED_DOWNLOAD) || defined(UDS_SERVICE_SUPPORTED_UPLOAD)
    uds_cb_transferPayload(req->payload, req->payloadLengthBytes);
#else
    uds_sendNegativeResponse(UDS_SID_TRANSFER, UDS_NRC_SERVICE_NOT_SUPPORTED);
#endif
}


static inline void handleTransferStop(udsRequestDesc_S *req)
{
#if defined(UDS_SERVICE_SUPPORTED_DOWNLOAD) || defined(UDS_SERVICE_SUPPORTED_UPLOAD)
    uds_cb_transferStop(req->payload, req->payloadLengthBytes);
#else
    uds_sendNegativeResponse(UDS_SID_TRANSFER_STOP, UDS_NRC_SERVICE_NOT_SUPPORTED);
#endif
}


static inline void handleDIDRead(udsRequestDesc_S *req)
{
#if defined(UDS_SERVICE_SUPPORTED_DID_READ)
    uds_cb_DIDRead(req->payload, req->payloadLengthBytes);
#else // if defined(UDS_SERVICE_SUPPORTED_DID_READ)
    uds_sendNegativeResponse(UDS_SID_READ_DID, UDS_NRC_SERVICE_NOT_SUPPORTED);
#endif // if defined(UDS_SERVICE_SUPPORTED_DID_READ)
}


/*
 * udsSrv_handleReceive
 * @brief handles received completed uds message
 */
static udsResult_E udsSrv_handleReceive(udsRequestDesc_S *req)
{
    switch (req->id)
    {
        case UDS_SID_TESTER_PRESENT:
            handleTesterPresent(req);
            break;

        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
            handleDiagnosticSessionControl(req);
            break;

        case UDS_SID_ECU_RESET:
            handleEcuReset(req);
            break;

        case UDS_SID_ROUTINE_CONTROL:
            handleRoutineControl(req);
            break;

        case UDS_SID_READ_DID:
            handleDIDRead(req);
            break;

        // generally useful services
        case UDS_SID_WRITE_DID:
        case UDS_SID_READ_ADDRESS:
        case UDS_SID_WRITE_ADDRESS:
        case UDS_SID_IO_CONTROL:
            // not handled yet
            uds_sendNegativeResponse(req->id, UDS_NRC_SERVICE_NOT_SUPPORTED);
            break;

        // services required for downloads
        case UDS_SID_DOWNLOAD_START:
            handleDownloadStart(req);
            break;

        case UDS_SID_TRANSFER:
            handleTransfer(req);
            break;

        case UDS_SID_TRANSFER_STOP:
            handleTransferStop(req);
            break;

        // harder to implement, or just not needed yet
        case UDS_SID_SECURITY_ACCESS:
        case UDS_SID_COMMUNICATION_CONTROL:
        case UDS_SID_AUTHENTICATION:
        case UDS_SID_ACCESS_TIMING_PARAMETERS:
        case UDS_SID_SECURED_DATA_TRANSMISSION:
        case UDS_SID_CONTROL_DTC_SETTINGS:
        case UDS_SID_RESPONSE_ON_EVENT:
        case UDS_SID_LINK_CONTROL:
        case UDS_SID_READ_SCALING_DID:
        case UDS_SID_READ_DID_PERIODIC:
        case UDS_SID_DYNAMICALLY_DEFINE_DID:
        case UDS_SID_CLEAR_DTC:
        case UDS_SID_READ_DTC:
        case UDS_SID_UPLOAD_START:
        case UDS_SID_FILE_TRANSFER:
            // not handled yet
            uds_sendNegativeResponse(req->id, UDS_NRC_SERVICE_NOT_SUPPORTED);
            break;

        default:
            // unrecognized reqest, do nothing
            uds_sendNegativeResponse(req->id, UDS_NRC_GENERAL_REJECT);
            break;
    }
    return UDS_RESULT_SUCCESS;
}


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * udsSrv_init
 * @brief initialize the UDS server
 */
void udsSrv_init(void)
{
    // always start in default session
    udsSrv.currentSession = UDS_SESSION_TYPE_DEFAULT;

    isotp_init_link(&udsSrv.isoTpLink,
                    UDS_RESPONSE_ID,
                    udsSrv.txBuf,
                    ISOTP_TX_BUF_SIZE,
                    udsSrv.rxBuf,
                    ISOTP_RX_BUF_SIZE);

    udsSrv.bit.initialized = true;
    udsSrv.bit.unprocessed_message = false;
}


/*
 * udsSrv_receive
 * @brief process a frame sent to the UDS id
 * @param data uint8_t[] frame payload
 * @param dlc size of payload
 */
udsResult_E udsSrv_processMessage(uint8_t data[], uint8_t dlc)
{
    if (!udsSrv.bit.initialized)
    {
        return UDS_RESULT_NOT_INITIALIZED;
    }

    udsSrv.bit.unprocessed_message = true;
    isotp_on_can_message(&udsSrv.isoTpLink, data, dlc);    // process the isotp frame
    return UDS_RESULT_SUCCESS;
}


/*
 * udsSrv_send
 * @brief send a UDS response
 */
udsResult_E udsSrv_send(udsResponseDesc_S response)
{
    // TODO: prevent stack overflow
    bool    hasSubFunction = (uint8_t)response.subFunction != 0U;
    uint8_t totalLength    = 1U + response.payloadLengthBytes + (hasSubFunction ? 1U : 0U);

    if (totalLength > 7U)
    {
        return UDS_RESULT_INCORRECT_PAYLOAD_LENGTH;
    }

    uint8_t payload[totalLength];
    payload[0] = response.id;
    payload[1] = hasSubFunction ? response.subFunction : 0U;


    if ((response.payloadLengthBytes != 0U) && (response.payload != NULL))
    {
        uint8_t payloadStartIdx = hasSubFunction ? 2U : 1U;
        memcpy(&payload[payloadStartIdx], response.payload, response.payloadLengthBytes);
    }

    // TODO: handle return value
    (void)isotp_send(&udsSrv.isoTpLink, payload, totalLength);

    return UDS_RESULT_SUCCESS;
}

/*
 * udsSrv_periodic
 * @brief periodic uds function, polls isotp layer and handles completed messages and multi frame transmissions
 */
udsResult_E udsSrv_periodic(void)
{
    udsResult_E ret                        = UDS_RESULT_SUCCESS;
    uint8_t     payload[ISOTP_RX_BUF_SIZE] = { 0U };
    uint16_t    parsedLen;

    isotp_poll(&udsSrv.isoTpLink);

    uint8_t receiveStatus = isotp_receive(&udsSrv.isoTpLink, payload, ISOTP_RX_BUF_SIZE, &parsedLen);
    if (receiveStatus == ISOTP_RET_OK)
    {
        udsRequestDesc_S req =
        {
            .id                 = payload[0U],
            .payloadLengthBytes = parsedLen - 1U,
            .payload            = &payload[1U],
        };
        ret = udsSrv_handleReceive(&req);
        udsSrv.bit.unprocessed_message = false;
    }

    manageSession(0U);

    return ret;
}


/*
 * udsSrv_getCurrentSessionType
 * @brief returns the current uds session
 */
udsSessionType_E udsSrv_getCurrentSession(void)
{
    return udsSrv.currentSession;
}

/*
 * udsSrv_timeSinceLastTp
 * @brief returns the amount of time that has passed since a TP was received
 */
uint32_t udsSrv_timeSinceLastTp(void)
{
    return udsSrv.testerPresentTimerMs;
}


/*
 * uds_checkResponseRequired()
 * @bried check if the given request requires a response
 */
bool uds_checkResponseRequired(udsRequestDesc_S *req)
{
    return (req->payloadLengthBytes == 0U) || !(req->payload[0] & UDS_RESPONSE_NOT_REQUIRED);
}


/*
 * uds_sendNegativeResponse
 * @brief send a negative response to the given service id with the given negative response code
 * @param sid udsServiceId_E service id to respond to
 * @param nrc udsNegativeResponse_E negative service response code to send
 */
udsResult_E uds_sendNegativeResponse(udsServiceId_E sid, udsNegativeResponse_E nrc)
{
    uint8_t           payload[2] = { sid, nrc };
    udsResponseDesc_S resp       = {
        .id                 = UDS_SID_NEGATIVE_RESPONSE_CODE,
        .payload            = payload,
        .payloadLengthBytes =                              2,
    };

    return udsSrv_send(resp);
}


/*
 * uds_sendPositiveResponse
 * @brief send a positive response to the given service id with the given payload
 * @param sid udsServiceId_E service id to respond to
 * @param subFunction uint8_t subFunction for the response, 0x00 if none
 * @param payload uint8_t* payload array, data to send in the response
 * @param payloadLengthBytes uint8_t length of the payload data array, max 8, 0 if there is no payload
 */
udsResult_E uds_sendPositiveResponse(udsServiceId_E sid, uint8_t subFunction, uint8_t *payload, uint8_t payloadLengthBytes)
{
    udsResponseDesc_S resp = {
        .id                 = sid + 0x40,
        .subFunction        = subFunction,
        .payload            = payload,
        .payloadLengthBytes = payloadLengthBytes,
    };

    return udsSrv_send(resp);
}


/*
 * uds_cb_sessionChangeAllowed
 * always allow transition if user has not defined a function to override this weak one
 */
__attribute__((weak)) udsNegativeResponse_E uds_cb_sessionChangeAllowed(udsSessionType_E currentSession, udsSessionType_E requestedSession)
{
    return UDS_NRC_NONE;
}
#endif // UDS_ENABLE_LIB
