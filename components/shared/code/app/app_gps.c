/**
 * @file app_gps.c
 * @brief Source file for GPS NMEA library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "app_gps.h"
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

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static struct
{
    app_gps_pos_S pos;
    app_gps_heading_S heading;
    app_gps_time_S time;

    drv_timer_S timeout;

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
    LIB_BUFFER_CIRC_CREATE(dmaBuffer, uint8_t, BUFFER_SIZE);
    LIB_BUFFER_FIFO_CREATE(sentence, uint8_t, MAX_NMEA_SENTENCE);
    lwgps_t currentGPS;
    uint16_t crcFailures;
    uint16_t invalidTransactions;
    uint16_t samples;
#endif
} gps;

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
static void parse(uint8_t* sentence, size_t len)
{
    if (lwgps_process(&gps.currentGPS, sentence, len))
    {
        if (gps.currentGPS.p.stat == STAT_UNKNOWN)
        {
            gps.invalidTransactions++;
        }
        else if (gps.currentGPS.p.stat == STAT_CHECKSUM_FAIL)
        {
            gps.crcFailures++;
        }
        else
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
            drv_timer_start(&gps.timeout, GPS_TIMEOUT_MS);
            gps.samples++;
        }
    }
}
#endif

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

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

bool app_gps_isValid(void)
{
    return drv_timer_getState(&gps.timeout) != DRV_TIMER_RUNNING;
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

static void app_gps_init(void)
{
    memset(&gps, 0x00U, sizeof(gps));

    drv_timer_init(&gps.timeout);

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
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
        if (LIB_BUFFER_CIRC_PEEKN(&gps.dmaBuffer, -1))
        {
            HW_UART_stopDMA(HW_UART_PORT_GPS);
            LIB_BUFFER_CIRC_CLEAR(&gps.dmaBuffer);
            HW_UART_startDMARX(HW_UART_PORT_GPS, (uint32_t*)&gps.dmaBuffer, BUFFER_SIZE);
            overrun = true;
        }
        else
        {
            uint8_t ch = LIB_BUFFER_CIRC_GETSET(&gps.dmaBuffer, 0U);
            LIB_BUFFER_FIFO_INSERT(&gps.sentence, ch);
            if (ch == '\n')
            {
                parse((uint8_t*)&gps.sentence.buffer, LIB_BUFFER_FIFO_GETLENGTH(&gps.sentence));
                LIB_BUFFER_FIFO_CLEAR(&gps.sentence);
            }
        }
    }

    const bool gpsValid = app_gps_isValid();

    app_faultManager_setFaultState(GPS_DEVICE_ERROR, gpsValid || overrun);
#else // FEATURE_GPSTRANSCEIVER
    // TODO: Implement GPS listener
#endif // !FEATURE_GPSTRANSCEIVER
}

const ModuleDesc_S app_gps_desc = {
    .moduleInit = &app_gps_init,
    .periodic100Hz_CLK = &app_gps_periodic_100Hz,
};
