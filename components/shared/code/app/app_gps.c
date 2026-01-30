/**
 * @file app_gps.c
 * @brief Source file for GPS NMEA library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_gps.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "lwgps.h"
#include "drv_timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lib_buffer.h"
#include "app_faultManager.h"
#include "HW_uart.h"
#include "HW_gpio.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BUFFER_SIZE 2048U
#define MAX_NMEA_SENTENCE 82U
#define GPS_TIMEOUT_MS 2000U

#define GPS_DEVICE_ERROR FM_FAULT_VCFRONT_GPSDEVICEERROR
#define GPS_DEVICE_OVERRUN FM_FAULT_VCFRONT_GPSOVERRUN

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    app_gps_pos_S pos;
    app_gps_heading_S heading;
    app_gps_time_S time;
    app_gps_pairmsg_S pairmsg;

    drv_timer_S timeout;

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
    LIB_BUFFER_CIRC_CREATE(dmaBuffer, volatile uint8_t, BUFFER_SIZE);
    LIB_BUFFER_FIFO_CREATE(sentence, uint8_t, MAX_NMEA_SENTENCE);
    lwgps_t currentGPS;
    uint16_t crcFailures;
    uint16_t invalidTransactions;
    uint16_t samples;
    volatile uint16_t uartErrorOreCount;
    volatile uint16_t uartErrorFeCount;
    volatile uint16_t uartErrorNeCount;
    volatile uint16_t uartErrorPeCount;
#endif
} gps;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
static void updateGPS(void)
{
    taskENTER_CRITICAL();
    gps.pos.lat = gps.currentGPS.latitude;
    gps.pos.lon = gps.currentGPS.longitude;
    gps.pos.alt = gps.currentGPS.altitude;

    gps.heading.course = gps.currentGPS.course;
    gps.heading.speedMps = lwgps_to_speed(gps.currentGPS.speed, LWGPS_SPEED_MPS);

    gps.time.date = gps.currentGPS.date;
    gps.time.month = gps.currentGPS.month;
    gps.time.year = gps.currentGPS.year;
    gps.time.hours = gps.currentGPS.hours;
    gps.time.seconds = gps.currentGPS.seconds;
    taskEXIT_CRITICAL();
}

static bool parsePairmsgUtcMs(const char* field, uint32_t* utcMs)
{
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    uint32_t millisecond = 0U;

    if ((field == NULL) || (field[0] == '\0'))
    {
        return false;
    }

    if (!(isdigit((unsigned char)field[0]) && isdigit((unsigned char)field[1]) &&
          isdigit((unsigned char)field[2]) && isdigit((unsigned char)field[3]) &&
          isdigit((unsigned char)field[4]) && isdigit((unsigned char)field[5])))
    {
        return false;
    }

    hour = (uint32_t)((field[0] - '0') * 10 + (field[1] - '0'));
    minute = (uint32_t)((field[2] - '0') * 10 + (field[3] - '0'));
    second = (uint32_t)((field[4] - '0') * 10 + (field[5] - '0'));

    if ((hour > 23U) || (minute > 59U) || (second > 59U))
    {
        return false;
    }

    const char* dot = strchr(field, '.');
    if (dot != NULL)
    {
        uint32_t scale = 100U;
        const char* p = dot + 1;
        while ((*p != '\0') && isdigit((unsigned char)*p) && (scale > 0U))
        {
            millisecond += (uint32_t)(*p - '0') * scale;
            scale /= 10U;
            p++;
        }
    }

    *utcMs = ((hour * 3600U) + (minute * 60U) + second) * 1000U + millisecond;
    return true;
}

static bool parsePairmsgChecksum(const char* payload, const char* checksum)
{
    uint8_t calc = 0U;
    const char* p = payload;

    while ((p != NULL) && (*p != '\0'))
    {
        calc ^= (uint8_t)(*p);
        p++;
    }

    if ((checksum == NULL) || !isxdigit((unsigned char)checksum[0]) || !isxdigit((unsigned char)checksum[1]))
    {
        return false;
    }

    uint8_t expected = 0U;
    for (uint8_t i = 0U; i < 2U; i++)
    {
        char c = (char)toupper((unsigned char)checksum[i]);
        expected = (uint8_t)(expected << 4);
        if (c >= 'A')
        {
            expected = (uint8_t)(expected | (uint8_t)(c - 'A' + 10));
        }
        else
        {
            expected = (uint8_t)(expected | (uint8_t)(c - '0'));
        }
    }

    return calc == expected;
}

static bool parsePairmsg(const uint8_t* sentence, size_t len)
{
    char buffer[MAX_NMEA_SENTENCE + 1U];
    size_t copyLen = len;
    char* cursor = NULL;

    if (copyLen > MAX_NMEA_SENTENCE)
    {
        copyLen = MAX_NMEA_SENTENCE;
    }

    memcpy(buffer, sentence, copyLen);
    buffer[copyLen] = '\0';

    while (copyLen > 0U)
    {
        char tail = buffer[copyLen - 1U];
        if ((tail != '\r') && (tail != '\n'))
        {
            break;
        }
        buffer[copyLen - 1U] = '\0';
        copyLen--;
    }

    if (strncmp(buffer, "$PAIRMSG,", 9U) != 0)
    {
        return false;
    }

    char* star = strchr(buffer, '*');
    if (star == NULL)
    {
        gps.invalidTransactions++;
        return true;
    }

    *star = '\0';
    const char* checksum = star + 1;

    if (!parsePairmsgChecksum(buffer + 1, checksum))
    {
        gps.crcFailures++;
        return true;
    }

    cursor = buffer;
    char* token = cursor;
    char* comma = strchr(cursor, ',');
    if (comma != NULL)
    {
        *comma = '\0';
        cursor = comma + 1;
    }
    else
    {
        cursor = NULL;
    }
    if (token == NULL)
    {
        gps.invalidTransactions++;
        return true;
    }

    token = cursor;
    if (token != NULL)
    {
        comma = strchr(token, ',');
        if (comma != NULL)
        {
            *comma = '\0';
            cursor = comma + 1;
        }
        else
        {
            cursor = NULL;
        }
    }
    if (token == NULL)
    {
        gps.invalidTransactions++;
        return true;
    }

    const long messageId = strtol(token, NULL, 10);
    uint32_t utcMs = 0U;
    uint8_t drStage = gps.pairmsg.drStage;
    uint8_t dynamicStatus = gps.pairmsg.dynamicStatus;
    uint8_t alarmStatus = gps.pairmsg.alarmStatus;

    token = cursor;
    if (token != NULL)
    {
        comma = strchr(token, ',');
        if (comma != NULL)
        {
            *comma = '\0';
            cursor = comma + 1;
        }
        else
        {
            cursor = NULL;
        }
    }
    if (!parsePairmsgUtcMs(token, &utcMs))
    {
        gps.invalidTransactions++;
        return true;
    }

    if (messageId == 90L)
    {
        token = cursor;
        if (token != NULL)
        {
            comma = strchr(token, ',');
            if (comma != NULL)
            {
                *comma = '\0';
                cursor = comma + 1;
            }
            else
            {
                cursor = NULL;
            }
        }
        if (token == NULL)
        {
            gps.invalidTransactions++;
            return true;
        }

        drStage = (uint8_t)strtoul(token, NULL, 10);
        taskENTER_CRITICAL();
        gps.pairmsg.utcMs = utcMs;
        gps.pairmsg.drStage = drStage;
        taskEXIT_CRITICAL();
        return true;
    }
    else if (messageId == 91L)
    {
        token = cursor;
        if (token != NULL)
        {
            comma = strchr(token, ',');
            if (comma != NULL)
            {
                *comma = '\0';
                cursor = comma + 1;
            }
            else
            {
                cursor = NULL;
            }
        }
        if (token == NULL)
        {
            gps.invalidTransactions++;
            return true;
        }
        dynamicStatus = (uint8_t)strtoul(token, NULL, 10);

        token = cursor;
        if (token != NULL)
        {
            comma = strchr(token, ',');
            if (comma != NULL)
            {
                *comma = '\0';
                cursor = comma + 1;
            }
            else
            {
                cursor = NULL;
            }
        }
        if (token == NULL)
        {
            gps.invalidTransactions++;
            return true;
        }
        alarmStatus = (uint8_t)strtoul(token, NULL, 10);

        taskENTER_CRITICAL();
        gps.pairmsg.utcMs = utcMs;
        gps.pairmsg.dynamicStatus = dynamicStatus;
        gps.pairmsg.alarmStatus = alarmStatus;
        taskEXIT_CRITICAL();
        return true;
    }

    gps.invalidTransactions++;
    return true;
}

static void parse(uint8_t* sentence, size_t len)
{
    lwgps_process(&gps.currentGPS, sentence, len);

    if (gps.currentGPS.p.stat == STAT_UNKNOWN)
    {
        if (parsePairmsg(sentence, len))
        {
            drv_timer_start(&gps.timeout, GPS_TIMEOUT_MS);
            gps.samples++;
        }
        else
        {
            gps.invalidTransactions++;
        }
    }
    else if (gps.currentGPS.p.stat == STAT_CHECKSUM_FAIL)
    {
        gps.crcFailures++;
    }
    else
    {
        updateGPS();
        drv_timer_start(&gps.timeout, GPS_TIMEOUT_MS);
        gps.samples++;
    }
}
#endif

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void app_gps_resetBuffers(void)
{
    LIB_BUFFER_CIRC_CLEAR(&gps.dmaBuffer);
    LIB_BUFFER_FIFO_CLEAR(&gps.sentence);
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
    return drv_timer_getState(&gps.timeout) == DRV_TIMER_RUNNING;
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

static void app_gps_init(void)
{
    memset(&gps, 0x00U, sizeof(gps));

    drv_timer_init(&gps.timeout);

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

    while (LIB_BUFFER_CIRC_PEEK(&gps.dmaBuffer))
    {
        uint8_t ch = LIB_BUFFER_CIRC_GETSET(&gps.dmaBuffer, 0U);

        if (LIB_BUFFER_CIRC_PEEKN(&gps.dmaBuffer, -1))
        {
            HW_UART_stopDMA(HW_UART_PORT_GPS);
            app_gps_resetBuffers();
            HW_UART_startDMARX(HW_UART_PORT_GPS, (uint32_t*)&gps.dmaBuffer, BUFFER_SIZE);
            overrun = true;
        }
        else
        {
            LIB_BUFFER_FIFO_INSERT(&gps.sentence, ch);
            if (ch == '\n')
            {
                parse((uint8_t*)&gps.sentence.buffer, LIB_BUFFER_FIFO_GETLENGTH(&gps.sentence));
                LIB_BUFFER_FIFO_CLEAR(&gps.sentence);
            }
        }
    }

    const bool gpsValid = app_gps_isValid();

    app_faultManager_setFaultState(GPS_DEVICE_ERROR, !gpsValid);
    app_faultManager_setFaultState(GPS_DEVICE_OVERRUN, overrun);
#else // FEATURE_GPSTRANSCEIVER
    // TODO: Implement GPS listener
#endif // !FEATURE_GPSTRANSCEIVER
}

const ModuleDesc_S app_gps_desc = {
    .moduleInit = &app_gps_init,
    .periodic100Hz_CLK = &app_gps_periodic_100Hz,
};
