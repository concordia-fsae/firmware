/** @file lib_yicGnss.c
 * @brief YIC sentence extraction, checksum policy and field decoding.
 */
#include "lib_yicGnss.h"
#include <ctype.h>
#include <string.h>

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
#if LWGPS_CFG_CRC
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
#else // if LWGPS_CFG_CRC
    (void)payload;
    return true;
#endif // if LWGPS_CFG_CRC
}

static bool parsePairmsg(lib_yicGnss_S* receiver, char* payload)
{
    // Retain empty fields so missing values cannot shift later fields left.
    char   * fields[5U];
    size_t count    = 0U;
    char   * cursor = payload;

    for (;;)
    {
        if (count == (sizeof(fields) / sizeof(fields[0])))
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
        receiver->data.pairmsg.utcMs   = utcMs;
        receiver->data.pairmsg.drStage = first;
    }
    else if ((messageId == 91U) && (count == 5U) && parseUint8(fields[4], &second))
    {
        receiver->data.pairmsg.utcMs         = utcMs;
        receiver->data.pairmsg.dynamicStatus = first;
        receiver->data.pairmsg.alarmStatus   = second;
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

static bool validateNavigation(lib_yicGnss_S* receiver, char* payload)
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
    char   * fields[20U];
    size_t count    = 0U;
    char   * cursor = payload;
    for (;;)
    {
        if (count == (sizeof(fields) / sizeof(fields[0])))
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
        if (strlen(fields[i]) >= sizeof(receiver->parser.p.term_str))
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
        receiver->data.ggaUtcMs = utcMs;
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
        receiver->data.rmcModeValid = fields[12][0] != 'N';
        receiver->data.rmcTimeValid = *fields[1] != '\0';
        receiver->data.rmcUtcMs     = utcMs;
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


static lib_yicGnss_sentence_E classifySentence(const char* payload)
{
    if (strncmp(payload, "$PAIRMSG,", 9U) == 0)
    {
        return LIB_YICGNSS_SENTENCE_PAIRMSG;
    }
    if ((strncmp(payload, "$GP", 3U) == 0) || (strncmp(payload, "$GN", 3U) == 0))
    {
        if (strncmp(payload + 3, "GGA", 3U) == 0)
        {
            return LIB_YICGNSS_SENTENCE_GGA;
        }
        if (strncmp(payload + 3, "GSA", 3U) == 0)
        {
            return LIB_YICGNSS_SENTENCE_GSA;
        }
        if (strncmp(payload + 3, "GSV", 3U) == 0)
        {
            return LIB_YICGNSS_SENTENCE_GSV;
        }
        if (strncmp(payload + 3, "RMC", 3U) == 0)
        {
            return LIB_YICGNSS_SENTENCE_RMC;
        }
    }
    return LIB_YICGNSS_SENTENCE_UNKNOWN;
}

static void updateData(lib_yicGnss_S* receiver)
{
    const lwgps_t     * parsed = &receiver->parser;
    lib_yicGnss_data_S* data   = &receiver->data;

    data->latitude   = parsed->latitude;
    data->longitude  = parsed->longitude;
    data->altitude   = parsed->altitude;
    data->course     = parsed->course;
    data->speedMps   = lwgps_to_speed(parsed->speed, LWGPS_SPEED_MPS);
    data->date       = parsed->date;
    data->month      = parsed->month;
    data->year       = parsed->year + 2000U;
    data->hours      = parsed->hours;
    data->minutes    = parsed->minutes;
    data->seconds    = parsed->seconds;
    data->satellites = parsed->sats_in_use;
    data->method     = (lib_yicGnss_method_E)parsed->fix;
    data->dimension  = (lib_yicGnss_dimension_E)parsed->fix_mode;
    data->rmcValid   = parsed->is_valid;
    data->timeValid  = parsed->time_valid;
    data->dateValid  = parsed->date_valid;
}

static lib_yicGnss_result_S parseFrame(lib_yicGnss_S* receiver)
{
    lib_yicGnss_result_S result = { LIB_YICGNSS_RESULT_INVALID, LIB_YICGNSS_SENTENCE_UNKNOWN };
    size_t               len    = receiver->sentenceLength;
    char                 buffer[LIB_YICGNSS_MAX_SENTENCE_LENGTH + 1U];

    memcpy(buffer, receiver->sentence, len);
    buffer[--len] = '\0';    // Every extracted frame starts with '$' and ends with LF.
    if (buffer[len - 1U] == '\r')
    {
        buffer[--len] = '\0';
    }
    char* star = strchr(buffer, '*');
    if ((star == NULL) || (strlen(star + 1) != 2U))
    {
        return result;
    }
    *star = '\0';
    if (!parseChecksum(buffer + 1, star + 1))
    {
        result.status = LIB_YICGNSS_RESULT_CHECKSUM_ERROR;
        return result;
    }
    result.sentence = classifySentence(buffer);
    if (result.sentence == LIB_YICGNSS_SENTENCE_PAIRMSG)
    {
        if (!parsePairmsg(receiver, buffer))
        {
            return result;
        }
    }
    else
    {
        if (!validateNavigation(receiver, buffer))
        {
            return result;
        }
        // LwGPS commits only on CR; also accept LF-only source frames.
        memcpy(buffer, receiver->sentence, len);
        buffer[len] = '\r';
        lwgps_process(&receiver->parser, buffer, len + 1U);
        if (result.sentence == LIB_YICGNSS_SENTENCE_UNKNOWN)
        {
            return result;
        }
        updateData(receiver);
    }
    result.status = LIB_YICGNSS_RESULT_ACCEPTED;
    return result;
}

void lib_yicGnss_init(lib_yicGnss_S* receiver)
{
    memset(receiver, 0, sizeof(*receiver));
}

void lib_yicGnss_resetStream(lib_yicGnss_S* receiver)
{
    receiver->sentenceLength = 0U;
}

lib_yicGnss_result_S lib_yicGnss_feed(lib_yicGnss_S* receiver, uint8_t byte)
{
    lib_yicGnss_result_S result = { LIB_YICGNSS_RESULT_NONE, LIB_YICGNSS_SENTENCE_UNKNOWN };

    if (byte == '$')
    {
        if (receiver->sentenceLength != 0U)
        {
            result.status = LIB_YICGNSS_RESULT_INVALID;
        }
        receiver->sentenceLength = 0U;
    }
    else if (receiver->sentenceLength == 0U)
    {
        return result;
    }
    if ((receiver->sentenceLength == LIB_YICGNSS_MAX_SENTENCE_LENGTH) ||
        ((byte < 0x20U) && (byte != '\r') && (byte != '\n')) || (byte > 0x7eU)
        )
    {
        receiver->sentenceLength = 0U;
        result.status            = LIB_YICGNSS_RESULT_INVALID;
        return result;
    }
    receiver->sentence[receiver->sentenceLength++] = byte;
    if (byte == '\n')
    {
        result                   = parseFrame(receiver);
        receiver->sentenceLength = 0U;
    }
    return result;
}
