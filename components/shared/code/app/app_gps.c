/**
 * @file app_gps.c
 * @brief Source file for GPS NMEA library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_faultManager.h"
#include "app_gps.h"
#include "drv_outputAD.h"
#include "drv_timer.h"
#include "FreeRTOS.h"
#include "HW_gpio.h"
#include "HW_uart.h"
#include "lib_buffer.h"
#include "lwgps.h"
#include "task.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BUFFER_SIZE           10000U
#define MAX_NMEA_SENTENCE     83U
#define GPS_TIMEOUT_MS        2000U
#define GPS_YEAR_OFFSET       2000U

#define GPS_DEVICE_ERROR      FM_FAULT_VCFRONT_GPSDEVICEERROR
#define GPS_DEVICE_OVERRUN    FM_FAULT_VCFRONT_GPSOVERRUN
#define GPS_DEVICE_INVALID    FM_FAULT_VCFRONT_GPSDATANOTVALID

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    bool              gpsValidData;
    bool              gpsValidTime;
    bool              gpsValidDate;
    uint8_t           numSatellites;
    app_gps_pos_S     pos;
    app_gps_heading_S heading;
    app_gps_time_S    time;
    app_gps_pairmsg_S pairmsg;

    drv_timer_S       timeout;
    drv_timer_S       ggaTimeout;
    drv_timer_S       rmcTimeout;
    drv_timer_S       gsaTimeout;
    bool              rmcModeValid;
    bool              rmcTimeValid;
    uint32_t          ggaUtcMs;
    uint32_t          rmcUtcMs;

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
    LIB_BUFFER_CIRC_CREATE(dmaBuffer, volatile uint8_t, BUFFER_SIZE);
    uint8_t           sentence[MAX_NMEA_SENTENCE];
    size_t            sentenceLength;
    lwgps_t           currentGPS;
    uint16_t          crcFailures;
    uint16_t          invalidTransactions;
    uint16_t          samples;
    uint16_t          sentenceCountGga;
    uint16_t          sentenceCountGsa;
    uint16_t          sentenceCountGsv;
    uint16_t          sentenceCountRmc;
    uint16_t          sentenceCountPairMsg;
    volatile uint16_t uartErrorOreCount;
    volatile uint16_t uartErrorFeCount;
    volatile uint16_t uartErrorNeCount;
    volatile uint16_t uartErrorPeCount;
#endif // if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
} gps;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
static bool recordNonPairmsgCount(lwgps_statement_t statement)
{
    switch (statement)
    {
        case STAT_GGA:
            gps.sentenceCountGga++;
            break;

        case STAT_GSA:
            gps.sentenceCountGsa++;
            break;

        case STAT_GSV:
            gps.sentenceCountGsv++;
            break;

        case STAT_RMC:
            gps.sentenceCountRmc++;
            break;

        default:
            return false;
    }
    return true;
}

static void updateGPS(void)
{
    taskENTER_CRITICAL();
    gps.pos.lat          = gps.currentGPS.latitude;
    gps.pos.lon          = gps.currentGPS.longitude;
    gps.pos.alt          = gps.currentGPS.altitude;

    gps.heading.course   = gps.currentGPS.course;
    gps.heading.speedMps = lwgps_to_speed(gps.currentGPS.speed, LWGPS_SPEED_MPS);

    gps.time.date        = gps.currentGPS.date;
    gps.time.month       = gps.currentGPS.month;
    gps.time.year        = gps.currentGPS.year + GPS_YEAR_OFFSET;
    gps.time.hours       = gps.currentGPS.hours;
    gps.time.minutes     = gps.currentGPS.minutes;
    gps.time.seconds     = gps.currentGPS.seconds;

    gps.numSatellites    = gps.currentGPS.sats_in_use;
    // Source freshness and GSA dimension are checked by app_gps_isValid().
    gps.gpsValidData     = gps.currentGPS.is_valid && gps.rmcModeValid &&
                           (((gps.currentGPS.fix >= 1U) && (gps.currentGPS.fix <= 3U)) || (gps.currentGPS.fix == 6U));
    gps.gpsValidTime     = gps.currentGPS.time_valid && gps.gpsValidData;
    gps.gpsValidDate     = gps.currentGPS.date_valid && gps.gpsValidData;
    taskEXIT_CRITICAL();
}

static bool parseUint8(const char* field, uint8_t* result)
{
    uint32_t value = 0U;

    if (*field == '\0')
    {
        return false;
    }
    for (const char* p = field; *p != '\0'; p++)
    {
        if (!isdigit((unsigned char)*p))
        {
            return false;
        }
        value = value * 10U + (uint32_t)(*p - '0');
        if (value > UINT8_MAX)
        {
            return false;
        }
    }
    *result = (uint8_t)value;
    return true;
}

static bool parsePairmsgUtcMs(const char* field, uint32_t* utcMs)
{
    const size_t length = strlen(field);

    // HHMMSS, optionally followed by one to three fractional-second digits.
    if ((length < 6U) || (length > 10U) || (length == 7U))
    {
        return false;
    }
    for (size_t i = 0U; i < length; i++)
    {
        if (i == 6U)
        {
            if (field[i] != '.')
            {
                return false;
            }
        }
        else if (!isdigit((unsigned char)field[i]))
        {
            return false;
        }
    }
    const uint32_t hour   = (uint32_t)((field[0] - '0') * 10 + field[1] - '0');
    const uint32_t minute = (uint32_t)((field[2] - '0') * 10 + field[3] - '0');
    const uint32_t second = (uint32_t)((field[4] - '0') * 10 + field[5] - '0');
    if ((hour > 23U) || (minute > 59U) || (second > 59U))
    {
        return false;
    }
    uint32_t milliseconds = 0U;
    uint32_t scale        = 100U;
    for (size_t i = 7U; i < length; i++)
    {
        milliseconds += (uint32_t)(field[i] - '0') * scale;
        scale        /= 10U;
    }
    *utcMs = ((hour * 3600U) + minute * 60U + second) * 1000U + milliseconds;
    return true;
}

static bool parseChecksum(const char* payload, const char* checksum)
{
    if (!isxdigit((unsigned char)checksum[0]) || !isxdigit((unsigned char)checksum[1]))
    {
        return false;
    }
# if LWGPS_CFG_CRC
    uint8_t calculated = 0U;
    for (const char* p = payload; *p != '\0'; p++)
    {
        calculated ^= (uint8_t)*p;
    }
    uint8_t expected = 0U;
    for (uint8_t i = 0U; i < 2U; i++)
    {
        const char    c     = (char)toupper((unsigned char)checksum[i]);
        const uint8_t digit = (uint8_t)((c >= 'A') ? (c - 'A' + 10) : (c - '0'));
        expected = (uint8_t)((expected << 4U) | digit);
    }
    return calculated == expected;
# else // if LWGPS_CFG_CRC
    (void)payload;
    return true;
# endif // if LWGPS_CFG_CRC
}

static bool parsePairmsg(char* payload)
{
    // Retain empty fields so missing values cannot shift later fields left.
    char   * fields[5U];
    size_t count    = 0U;
    char   * cursor = payload;

    for (;;)
    {
        if (count == COUNTOF(fields))
        {
            return false;
        }
        fields[count++] = cursor;
        char* comma = strchr(cursor, ',');
        if (comma == NULL)
        {
            break;
        }
        *comma = '\0';
        cursor = comma + 1;
    }
    if (count < 4U)
    {
        return false;
    }
    uint8_t  messageId;
    uint32_t utcMs;
    uint8_t  first;
    uint8_t  second;
    if (!parseUint8(fields[1], &messageId) || !parsePairmsgUtcMs(fields[2], &utcMs) ||
        !parseUint8(fields[3], &first)
        )
    {
        return false;
    }
    if ((messageId == 90U) && (count == 4U))
    {
        taskENTER_CRITICAL();
        gps.pairmsg.utcMs   = utcMs;
        gps.pairmsg.drStage = first;
        taskEXIT_CRITICAL();
    }
    else if ((messageId == 91U) && (count == 5U) && parseUint8(fields[4], &second))
    {
        taskENTER_CRITICAL();
        gps.pairmsg.utcMs         = utcMs;
        gps.pairmsg.dynamicStatus = first;
        gps.pairmsg.alarmStatus   = second;
        taskEXIT_CRITICAL();
    }
    else
    {
        return false;
    }
    return true;
}

// Validate fields that feed navigation/validity before the permissive LwGPS parser.
static bool parseDecimal(const char* field, double* result)
{
    const char * p      = field;
    const bool negative = *p == '-';

    if (negative)
    {
        p++;
    }
    bool   digit = false;
    bool   dot   = false;
    double value = 0.0;
    double scale = 0.1;
    for (; *p != '\0'; p++)
    {
        if ((*p == '.') && !dot && digit)
        {
            dot   = true;
            digit = false;
        }
        else if (isdigit((unsigned char)*p))
        {
            digit = true;
            if (dot)
            {
                value += (double)(*p - '0') * scale;
                scale *= 0.1;
            }
            else
            {
                value = value * 10.0 + (double)(*p - '0');
            }
        }
        else
        {
            return false;
        }
    }
    if (!digit)
    {
        return false;
    }
    *result = negative ? -value : value;
    return true;
}

static bool decimalInRange(const char* field, double minimum, double maximum)
{
    double value;

    return parseDecimal(field, &value) && (value >= minimum) && (value <= maximum);
}

static bool coordinateValid(const char* field, const char* hemisphere, bool latitude)
{
    const size_t degreesLength  = latitude ? 4U : 5U;
    const char   * dot          = strchr(field, '.');
    const size_t integralLength = (dot == NULL) ? strlen(field) : (size_t)(dot - field);

    if ((integralLength != degreesLength) || (strlen(hemisphere) != 1U))
    {
        return false;
    }
    if (latitude ? ((hemisphere[0] != 'N') && (hemisphere[0] != 'S')) :
        ((hemisphere[0] != 'E') && (hemisphere[0] != 'W'))
        )
    {
        return false;
    }
    double value;
    if (!parseDecimal(field, &value) || (value < 0.0) || (value > (latitude ? 9000.0 : 18000.0)))
    {
        return false;
    }
    const unsigned degrees = (unsigned)value / 100U;
    return value - (double)degrees * 100.0 < 60.0;
}

static bool dateValid(const char* field)
{
    if (strlen(field) != 6U)
    {
        return false;
    }
    for (size_t i = 0U; i < 6U; i++)
    {
        if (!isdigit((unsigned char)field[i]))
        {
            return false;
        }
    }
    const unsigned        day    = (unsigned)((field[0] - '0') * 10 + field[1] - '0');
    const unsigned        month  = (unsigned)((field[2] - '0') * 10 + field[3] - '0');
    const unsigned        year   = (unsigned)((field[4] - '0') * 10 + field[5] - '0');
    static const unsigned days[] = { 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U };
    if ((month == 0U) || (month > 12U) || (day == 0U))
    {
        return false;
    }
    // The application interprets NMEA years as 2000..2099.
    const unsigned limit = days[month - 1U] + (((month == 2U) && (year % 4U == 0U)) ? 1U : 0U);
    return day <= limit;
}

static bool validateNavigation(char* payload)
{
    if ((strncmp(payload, "$GP", 3U) != 0) && (strncmp(payload, "$GN", 3U) != 0))
    {
        return true;
    }
    const bool gga = strncmp(payload + 3, "GGA", 3U) == 0;
    const bool rmc = strncmp(payload + 3, "RMC", 3U) == 0;
    const bool gsa = strncmp(payload + 3, "GSA", 3U) == 0;
    if (!gga && !rmc && !gsa)
    {
        return true;
    }
    // A malformed navigation report must not leave a previous fix marked valid.
    if (gga)
    {
        drv_timer_stop(&gps.ggaTimeout);
    }
    if (rmc)
    {
        drv_timer_stop(&gps.rmcTimeout);
    }
    if (gsa)
    {
        drv_timer_stop(&gps.gsaTimeout);
    }
    char   * fields[20U];
    size_t count    = 0U;
    char   * cursor = payload;
    for (;;)
    {
        if (count == COUNTOF(fields))
        {
            return false;
        }
        fields[count++] = cursor;
        char* comma = strchr(cursor, ',');
        if (comma == NULL)
        {
            break;
        }
        *comma = '\0';
        cursor = comma + 1;
    }
    for (size_t i = 0U; i < count; i++)
    {
        // LwGPS truncates longer terms; do not validate a value it cannot retain.
        if (strlen(fields[i]) >= sizeof(gps.currentGPS.p.term_str))
        {
            return false;
        }
    }
    if (strlen(fields[0]) != 6U)
    {
        return false;
    }
    uint8_t  value;
    uint32_t utcMs = 0U;
    if (gga)
    {
        if ((count != 15U) || !parseUint8(fields[6], &value) ||
            ((value != 0U) && (value != 1U) && (value != 2U) && (value != 3U) && (value != 6U))
            )
        {
            return false;
        }
        const bool hasFix = value != 0U;
        if ((*fields[1] != '\0') && !parsePairmsgUtcMs(fields[1], &utcMs))
        {
            return false;
        }
        if (hasFix || (*fields[2] != '\0') || (*fields[3] != '\0') || (*fields[4] != '\0') || (*fields[5] != '\0'))
        {
            if (!coordinateValid(fields[2], fields[3], true) || !coordinateValid(fields[4], fields[5], false))
            {
                return false;
            }
        }
        if (!parseUint8(fields[7], &value) || (value > 56U))
        {
            return false;
        }
        if ((*fields[8] != '\0') && !decimalInRange(fields[8], 0.0, 99.9))
        {
            return false;
        }
        if (hasFix || (*fields[9] != '\0'))
        {
            if (!decimalInRange(fields[9], -9999.9, 17999.9) || (strcmp(fields[10], "M") != 0))
            {
                return false;
            }
        }
        gps.ggaUtcMs = utcMs;
    }
    else if (rmc)
    {
        if ((count != 13U) || ((strcmp(fields[2], "A") != 0) && (strcmp(fields[2], "V") != 0)) ||
            ((strcmp(fields[12], "A") != 0) && (strcmp(fields[12], "D") != 0) &&
             (strcmp(fields[12], "E") != 0) && (strcmp(fields[12], "N") != 0))
            )
        {
            return false;
        }
        const bool hasFix = (fields[2][0] == 'A') && (fields[12][0] != 'N');
        if ((*fields[1] != '\0') && !parsePairmsgUtcMs(fields[1], &utcMs))
        {
            return false;
        }
        if (hasFix || (*fields[3] != '\0') || (*fields[4] != '\0') || (*fields[5] != '\0') || (*fields[6] != '\0'))
        {
            if (!coordinateValid(fields[3], fields[4], true) || !coordinateValid(fields[5], fields[6], false))
            {
                return false;
            }
        }
        if ((hasFix || (*fields[7] != '\0')) && !decimalInRange(fields[7], 0.0, 999.9))
        {
            return false;
        }
        if ((*fields[8] != '\0') && !decimalInRange(fields[8], 0.0, 359.9))
        {
            return false;
        }
        if ((*fields[9] != '\0') && !dateValid(fields[9]))
        {
            return false;
        }
        gps.rmcModeValid = fields[12][0] != 'N';
        gps.rmcTimeValid = *fields[1] != '\0';
        gps.rmcUtcMs     = utcMs;
    }
    else
    {
        if (((count != 18U) && (count != 19U)) || ((strcmp(fields[1], "A") != 0) && (strcmp(fields[1], "M") != 0)) ||
            !parseUint8(fields[2], &value) || (value < 1U) || (value > 3U)
            )
        {
            return false;
        }
        if ((count == 19U) && (!parseUint8(fields[18], &value) || (value < 1U) || (value > 4U)))
        {
            return false;
        }
    }
    return true;
}

static void parse(const uint8_t* sentence, size_t len)
{
    char buffer[MAX_NMEA_SENTENCE + 1U];

    memcpy(buffer, sentence, len);
    buffer[len]   = '\0';
    // Extraction supplies a complete newline-terminated sentence.
    buffer[--len] = '\0';
    // Every extracted frame starts with '$', so len is still at least one.
    if (buffer[len - 1U] == '\r')
    {
        buffer[--len] = '\0';
    }
    char* star = strchr(buffer, '*');
    if ((star == NULL) || (strlen(star + 1) != 2U))
    {
        gps.invalidTransactions++;
        return;
    }
    *star = '\0';
    // Respect the receiver configuration for both NMEA and PAIRMSG checksums.
    // With CRC enabled, reject mismatches before LwGPS can update state.
    if (!parseChecksum(buffer + 1, star + 1))
    {
        gps.crcFailures++;
        return;
    }
    if (strncmp(buffer, "$PAIRMSG,", 9U) == 0)
    {
        if (!parsePairmsg(buffer))
        {
            gps.invalidTransactions++;
            return;
        }
        gps.sentenceCountPairMsg++;
    }
    else
    {
        if (!validateNavigation(buffer))
        {
            gps.invalidTransactions++;
            return;
        }
        // LwGPS commits on CR, even when the source frame was LF-only.
        memcpy(buffer, sentence, len);
        buffer[len] = '\r';
        lwgps_process(&gps.currentGPS, buffer, len + 1U);
        if (!recordNonPairmsgCount(gps.currentGPS.p.stat))
        {
            gps.invalidTransactions++;
            return;
        }
        switch (gps.currentGPS.p.stat)
        {
            case STAT_GGA:
                drv_timer_start(&gps.ggaTimeout, GPS_TIMEOUT_MS);
                break;

            case STAT_RMC:
                drv_timer_start(&gps.rmcTimeout, GPS_TIMEOUT_MS);
                break;

            case STAT_GSA:
                drv_timer_start(&gps.gsaTimeout, GPS_TIMEOUT_MS);
                break;

            default:
                break;
        }
        updateGPS();
    }
    drv_timer_start(&gps.timeout, GPS_TIMEOUT_MS);
    gps.samples++;
}

// Sentence storage is linear and bounded. A new '$' is a resynchronization
// point even if the preceding sentence was truncated or had no newline.
static void extractSentence(uint8_t ch)
{
    if (ch == '$')
    {
        if (gps.sentenceLength != 0U)
        {
            gps.invalidTransactions++;
        }
        gps.sentenceLength = 0U;
    }
    else if (gps.sentenceLength == 0U)
    {
        return;
    }

    if ((gps.sentenceLength == MAX_NMEA_SENTENCE) ||
        ((ch < 0x20U) && (ch != '\r') && (ch != '\n')) || (ch > 0x7eU)
        )
    {
        gps.invalidTransactions++;
        gps.sentenceLength = 0U;
        return;
    }
    gps.sentence[gps.sentenceLength++] = ch;
    if (ch == '\n')
    {
        parse(gps.sentence, gps.sentenceLength);
        gps.sentenceLength = 0U;
    }
}
#endif // if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void app_gps_resetBuffers(void)
{
    LIB_BUFFER_CIRC_CLEAR(&gps.dmaBuffer);
    gps.sentenceLength = 0U;
    // UART/DMA recovery invalidates the previous navigation snapshot.
    drv_timer_stop(&gps.ggaTimeout);
    drv_timer_stop(&gps.rmcTimeout);
    drv_timer_stop(&gps.gsaTimeout);
}

void app_gps_getPos(app_gps_pos_S* pos)
{
    taskENTER_CRITICAL();
    memcpy(pos, &gps.pos, sizeof(*pos));
    taskEXIT_CRITICAL();
}

void app_gps_getHeading(app_gps_heading_S* heading)
{
    taskENTER_CRITICAL();
    memcpy(heading, &gps.heading, sizeof(*heading));
    taskEXIT_CRITICAL();
}

void app_gps_getTime(app_gps_time_S* time)
{
    taskENTER_CRITICAL();
    memcpy(time, &gps.time, sizeof(*time));
    taskEXIT_CRITICAL();
}

void app_gps_getPairmsg(app_gps_pairmsg_S* pairmsg)
{
    taskENTER_CRITICAL();
    memcpy(pairmsg, &gps.pairmsg, sizeof(*pairmsg));
    taskEXIT_CRITICAL();
}

app_gps_pos_S* app_gps_getPosRef(void)
{
    return &gps.pos;
}

app_gps_heading_S* app_gps_getHeadingRef(void)
{
    return &gps.heading;
}

app_gps_time_S* app_gps_getTimeRef(void)
{
    return &gps.time;
}

app_gps_pairmsg_S* app_gps_getPairmsgRef(void)
{
    return &gps.pairmsg;
}

bool app_gps_isValid(void)
{
    const bool sourcesFresh = (drv_timer_getState(&gps.ggaTimeout) == DRV_TIMER_RUNNING) &&
                              (drv_timer_getState(&gps.rmcTimeout) == DRV_TIMER_RUNNING);
    // GNSS requires a fresh GSA dimension as well as GGA/RMC.
    // Dead reckoning does not require a satellite fix.
    const bool dimensionValid = (gps.currentGPS.fix == 6U) ||
                                ((drv_timer_getState(&gps.gsaTimeout) == DRV_TIMER_RUNNING) &&
                                 ((gps.currentGPS.fix_mode == 2U) || (gps.currentGPS.fix_mode == 3U)));

    return sourcesFresh && gps.gpsValidData && dimensionValid;
}

uint16_t app_gps_getCrcFailures(void)
{
    return gps.crcFailures;
}

uint16_t app_gps_getInvalidTransactions(void)
{
    return gps.invalidTransactions;
}

uint16_t app_gps_getNumberSamples(void)
{
    return gps.samples;
}

uint16_t app_gps_getSentenceCountGga(void)
{
    return gps.sentenceCountGga;
}

uint16_t app_gps_getSentenceCountGsa(void)
{
    return gps.sentenceCountGsa;
}

uint16_t app_gps_getSentenceCountGsv(void)
{
    return gps.sentenceCountGsv;
}

uint16_t app_gps_getSentenceCountRmc(void)
{
    return gps.sentenceCountRmc;
}

uint16_t app_gps_getSentenceCountPairMsg(void)
{
    return gps.sentenceCountPairMsg;
}

uint16_t app_gps_getUartErrorOreCount(void)
{
    return gps.uartErrorOreCount;
}

uint16_t app_gps_getUartErrorFeCount(void)
{
    return gps.uartErrorFeCount;
}

uint16_t app_gps_getUartErrorNeCount(void)
{
    return gps.uartErrorNeCount;
}

uint16_t app_gps_getUartErrorPeCount(void)
{
    return gps.uartErrorPeCount;
}

void app_gps_recordUartError(uint32_t errorCode)
{
    if ((errorCode & HAL_UART_ERROR_ORE) != 0U)
    {
        gps.uartErrorOreCount++;
    }
    if ((errorCode & HAL_UART_ERROR_FE) != 0U)
    {
        gps.uartErrorFeCount++;
    }
    if ((errorCode & HAL_UART_ERROR_NE) != 0U)
    {
        gps.uartErrorNeCount++;
    }
    if ((errorCode & HAL_UART_ERROR_PE) != 0U)
    {
        gps.uartErrorPeCount++;
    }
}

bool app_gps_getValidTime(void)
{
    // Do not pair a new GGA time with a date from a different RMC epoch.
    return gps.gpsValidTime && gps.rmcTimeValid &&
           (gps.ggaUtcMs / 1000U == gps.rmcUtcMs / 1000U) && app_gps_isValid();
}

bool app_gps_getValidDate(void)
{
    return gps.gpsValidDate && app_gps_getValidTime();
}

uint8_t app_gps_getNumSatellites(void)
{
    return gps.numSatellites;
}

uint8_t app_gps_getFixDimension(void)
{
    if ((drv_timer_getState(&gps.gsaTimeout) != DRV_TIMER_RUNNING) || (gps.currentGPS.fix_mode > 3U))
    {
        return 0U;
    }
    return gps.currentGPS.fix_mode;
}

CAN_gpsQualityIndicator_E app_gps_getQualityCAN(void)
{
    if (drv_timer_getState(&gps.ggaTimeout) != DRV_TIMER_RUNNING)
    {
        return CAN_GPSQUALITYINDICATOR_FIX_NONE;
    }
    // Explicitly map YIC GGA method codes, without deriving them from GSA.
    switch (gps.currentGPS.fix)
    {
        case 1U: return CAN_GPSQUALITYINDICATOR_SPS;

        case 2U: return CAN_GPSQUALITYINDICATOR_DIFFERENTIAL;

        case 3U: return CAN_GPSQUALITYINDICATOR_PPS;

        case 6U: return CAN_GPSQUALITYINDICATOR_DEAD_RECKONING;

        default: return CAN_GPSQUALITYINDICATOR_FIX_NONE;
    }
}

static void app_gps_init(void)
{
    memset(&gps, 0x00U, sizeof(gps));

    drv_timer_init(&gps.timeout);
    drv_timer_init(&gps.ggaTimeout);
    drv_timer_init(&gps.rmcTimeout);
    drv_timer_init(&gps.gsaTimeout);

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
    LIB_BUFFER_CIRC_CLEAR(&gps.dmaBuffer);
    HW_UART_startDMARX(HW_UART_PORT_GPS, (uint32_t*)&gps.dmaBuffer, BUFFER_SIZE);
    drv_outputAD_setDigitalActiveState(DRV_OUTPUTAD_DIGITAL_MCU_UART_EN, DRV_IO_ACTIVE);
#endif // FEATURE_GPSTRANSCEIVER
}

static void app_gps_periodic_100Hz(void)
{
#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
    bool overrun = false;

    // DMA's producer position, unlike a zero-byte sentinel, also lets us
    // consume binary noise/NULs and resynchronize at the next sentence.
    uint32_t remaining = HW_UART_getDmaRxRemaining(HW_UART_PORT_GPS);
    size_t   writePos  = (BUFFER_SIZE - remaining) % BUFFER_SIZE;
    while ((gps.dmaBuffer.currentPos != writePos) || LIB_BUFFER_CIRC_PEEK(&gps.dmaBuffer))
    {
        // Inspect the predecessor BEFORE clearing/advancing the consumer.
        // Nonzero data in a consumed slot means the DMA producer has lapped us.
        if (LIB_BUFFER_CIRC_PEEKN(&gps.dmaBuffer, -1))
        {
            HW_UART_stopDMA(HW_UART_PORT_GPS);
            app_gps_resetBuffers();
            HW_UART_startDMARX(HW_UART_PORT_GPS, (uint32_t*)&gps.dmaBuffer, BUFFER_SIZE);
            overrun = true;
            break;
        }
        taskENTER_CRITICAL();
        uint8_t ch = LIB_BUFFER_CIRC_GETSET(&gps.dmaBuffer, 0U);
        extractSentence(ch);
        taskEXIT_CRITICAL();
        remaining = HW_UART_getDmaRxRemaining(HW_UART_PORT_GPS);
        writePos  = (BUFFER_SIZE - remaining) % BUFFER_SIZE;
    }

    const bool gpsValid     = drv_timer_getState(&gps.timeout) == DRV_TIMER_RUNNING;
    const bool gpsDataValid = app_gps_isValid();

    app_faultManager_setFaultState(GPS_DEVICE_ERROR,   !gpsValid);
    app_faultManager_setFaultState(GPS_DEVICE_OVERRUN, overrun);
    app_faultManager_setFaultState(GPS_DEVICE_INVALID, !gpsDataValid);
#else // FEATURE_GPSTRANSCEIVER
    // TODO: Implement GPS listener
#endif // !FEATURE_GPSTRANSCEIVER
}

const ModuleDesc_S app_gps_desc = {
    .moduleInit        = &app_gps_init,
    .periodic100Hz_CLK = &app_gps_periodic_100Hz,
};
