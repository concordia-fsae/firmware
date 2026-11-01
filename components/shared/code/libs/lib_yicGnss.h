/** @file lib_yicGnss.h
 * @brief Byte-stream decoder for YIC GT-series GNSS receivers.
 *
 * Caller owns the context and serialization. No RTOS, UART, timer or CAN dependency.
 * Raw method/dimension codes are receiver values, not CAN encodings.
 */
#pragma once
#include "lwgps.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIB_YICGNSS_MAX_SENTENCE_LENGTH    83U

typedef enum
{
    LIB_YICGNSS_METHOD_UNAVAILABLE    = 0U,
    LIB_YICGNSS_METHOD_SPS            = 1U,
    LIB_YICGNSS_METHOD_DIFFERENTIAL   = 2U,
    LIB_YICGNSS_METHOD_PPS            = 3U,
    LIB_YICGNSS_METHOD_DEAD_RECKONING = 6U,
} lib_yicGnss_method_E;

typedef enum
{
    LIB_YICGNSS_DIMENSION_UNKNOWN = 0U,
    LIB_YICGNSS_DIMENSION_NO_FIX  = 1U,
    LIB_YICGNSS_DIMENSION_2D      = 2U,
    LIB_YICGNSS_DIMENSION_3D      = 3U,
} lib_yicGnss_dimension_E;

typedef enum
{
    LIB_YICGNSS_SENTENCE_UNKNOWN,
    LIB_YICGNSS_SENTENCE_GGA,
    LIB_YICGNSS_SENTENCE_GSA,
    LIB_YICGNSS_SENTENCE_GSV,
    LIB_YICGNSS_SENTENCE_RMC,
    LIB_YICGNSS_SENTENCE_PAIRMSG,
} lib_yicGnss_sentence_E;

typedef enum
{
    LIB_YICGNSS_RESULT_NONE,
    LIB_YICGNSS_RESULT_ACCEPTED,
    LIB_YICGNSS_RESULT_INVALID,
    LIB_YICGNSS_RESULT_CHECKSUM_ERROR,
} lib_yicGnss_status_E;

typedef struct
{
    lib_yicGnss_status_E   status;
    // INVALID GGA/GSA/RMC identifies the malformed source for caller policy.
    // Framing/checksum failures have UNKNOWN source and do not identify fix loss.
    lib_yicGnss_sentence_E sentence;
} lib_yicGnss_result_S;

typedef struct
{
    float                   latitude;
    float                   longitude;
    float                   altitude;
    float                   course;
    float                   speedMps;
    uint8_t                 date;
    uint8_t                 month;
    uint16_t                year;
    uint8_t                 hours;
    uint8_t                 minutes;
    uint8_t                 seconds;
    uint8_t                 satellites;
    lib_yicGnss_method_E    method;
    lib_yicGnss_dimension_E dimension;
    // Decoded source flags/epochs, not application validity decisions.
    bool                    rmcValid;
    bool                    rmcModeValid;
    bool                    rmcTimeValid;
    uint32_t                ggaUtcMs;
    uint32_t                rmcUtcMs;
    bool                    timeValid;
    bool                    dateValid;
    struct
    {
        uint32_t utcMs;
        uint8_t  drStage;
        uint8_t  dynamicStatus;
        uint8_t  alarmStatus;
    } pairmsg;
} lib_yicGnss_data_S;

typedef struct
{
    // Last accepted decoded data. Read-only to callers.
    lib_yicGnss_data_S data;
    // Decoder implementation state; callers must use init/feed/resetStream.
    lwgps_t            parser;
    uint8_t            sentence[LIB_YICGNSS_MAX_SENTENCE_LENGTH];
    size_t             sentenceLength;
} lib_yicGnss_S;

void                 lib_yicGnss_init(lib_yicGnss_S* receiver);
// Discard a partial frame. Decoded data remain available; caller invalidates freshness.
void                 lib_yicGnss_resetStream(lib_yicGnss_S* receiver);
// One byte may emit an accepted frame or error; NONE means noise/partial frame.
lib_yicGnss_result_S lib_yicGnss_feed(lib_yicGnss_S* receiver, uint8_t byte);
