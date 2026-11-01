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
#include "lib_yicGnss.h"
#include "task.h"
#include <string.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define BUFFER_SIZE           10000U
#define GPS_TIMEOUT_MS        2000U

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

#if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)
    LIB_BUFFER_CIRC_CREATE(dmaBuffer, volatile uint8_t, BUFFER_SIZE);
    lib_yicGnss_S     receiver;
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
static void updateGPS(void)
{
    const lib_yicGnss_data_S* data = &gps.receiver.data;

    gps.pos.lat               = data->latitude;
    gps.pos.lon               = data->longitude;
    gps.pos.alt               = data->altitude;
    gps.heading.course        = data->course;
    gps.heading.speedMps      = data->speedMps;
    gps.time.date             = data->date;
    gps.time.month            = data->month;
    gps.time.year             = data->year;
    gps.time.hours            = data->hours;
    gps.time.minutes          = data->minutes;
    gps.time.seconds          = data->seconds;
    gps.numSatellites         = data->satellites;
    gps.gpsValidData          = data->rmcValid && data->rmcModeValid &&
                                ((data->method == LIB_YICGNSS_METHOD_SPS) || (data->method == LIB_YICGNSS_METHOD_DIFFERENTIAL) ||
                                 (data->method == LIB_YICGNSS_METHOD_PPS) || (data->method == LIB_YICGNSS_METHOD_DEAD_RECKONING));
    gps.gpsValidTime          = data->timeValid && gps.gpsValidData;
    gps.gpsValidDate          = data->dateValid && gps.gpsValidData;
    gps.pairmsg.utcMs         = data->pairmsg.utcMs;
    gps.pairmsg.drStage       = data->pairmsg.drStage;
    gps.pairmsg.dynamicStatus = data->pairmsg.dynamicStatus;
    gps.pairmsg.alarmStatus   = data->pairmsg.alarmStatus;
}

// Called while the DMA consumer holds the application critical section.
static void processByte(uint8_t byte)
{
    const lib_yicGnss_result_S result        = lib_yicGnss_feed(&gps.receiver, byte);
    drv_timer_S                * sourceTimer = NULL;

    switch (result.sentence)
    {
        case LIB_YICGNSS_SENTENCE_GGA: sourceTimer = &gps.ggaTimeout; break;

        case LIB_YICGNSS_SENTENCE_RMC: sourceTimer = &gps.rmcTimeout; break;

        case LIB_YICGNSS_SENTENCE_GSA: sourceTimer = &gps.gsaTimeout; break;

        default: break;
    }
    switch (result.status)
    {
        case LIB_YICGNSS_RESULT_NONE:
            return;

        case LIB_YICGNSS_RESULT_CHECKSUM_ERROR:
            gps.crcFailures++;
            return;

        case LIB_YICGNSS_RESULT_INVALID:
            gps.invalidTransactions++;
            if (sourceTimer != NULL)
            {
                drv_timer_stop(sourceTimer);
            }
            return;

        case LIB_YICGNSS_RESULT_ACCEPTED:
            break;
    }
    if (sourceTimer != NULL)
    {
        drv_timer_start(sourceTimer, GPS_TIMEOUT_MS);
    }
    switch (result.sentence)
    {
        case LIB_YICGNSS_SENTENCE_GGA: gps.sentenceCountGga++; break;

        case LIB_YICGNSS_SENTENCE_GSA: gps.sentenceCountGsa++; break;

        case LIB_YICGNSS_SENTENCE_GSV: gps.sentenceCountGsv++; break;

        case LIB_YICGNSS_SENTENCE_RMC: gps.sentenceCountRmc++; break;

        case LIB_YICGNSS_SENTENCE_PAIRMSG: gps.sentenceCountPairMsg++; break;

        default: break;
    }
    updateGPS();
    drv_timer_start(&gps.timeout, GPS_TIMEOUT_MS);
    gps.samples++;
}
#endif // if FEATURE_IS_ENABLED(FEATURE_GPSTRANSCEIVER)

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void app_gps_resetBuffers(void)
{
    LIB_BUFFER_CIRC_CLEAR(&gps.dmaBuffer);
    lib_yicGnss_resetStream(&gps.receiver);
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
    const bool dimensionValid = (gps.receiver.data.method == LIB_YICGNSS_METHOD_DEAD_RECKONING) ||
                                ((drv_timer_getState(&gps.gsaTimeout) == DRV_TIMER_RUNNING) &&
                                 ((gps.receiver.data.dimension == LIB_YICGNSS_DIMENSION_2D) || (gps.receiver.data.dimension == LIB_YICGNSS_DIMENSION_3D)));

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
    const lib_yicGnss_data_S* data = &gps.receiver.data;

    // The application must not pair a new time with a date from another epoch.
    return gps.gpsValidTime && data->rmcTimeValid &&
           (data->ggaUtcMs / 1000U == data->rmcUtcMs / 1000U) && app_gps_isValid();
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
    if ((drv_timer_getState(&gps.gsaTimeout) != DRV_TIMER_RUNNING) || (gps.receiver.data.dimension > 3U))
    {
        return 0U;
    }
    return (uint8_t)gps.receiver.data.dimension;
}

CAN_gpsQualityIndicator_E app_gps_getQualityCAN(void)
{
    if (drv_timer_getState(&gps.ggaTimeout) != DRV_TIMER_RUNNING)
    {
        return CAN_GPSQUALITYINDICATOR_FIX_NONE;
    }
    // Explicitly map YIC GGA method codes, without deriving them from GSA.
    switch (gps.receiver.data.method)
    {
        case LIB_YICGNSS_METHOD_SPS: return CAN_GPSQUALITYINDICATOR_SPS;

        case LIB_YICGNSS_METHOD_DIFFERENTIAL: return CAN_GPSQUALITYINDICATOR_DIFFERENTIAL;

        case LIB_YICGNSS_METHOD_PPS: return CAN_GPSQUALITYINDICATOR_PPS;

        case LIB_YICGNSS_METHOD_DEAD_RECKONING: return CAN_GPSQUALITYINDICATOR_DEAD_RECKONING;

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
    lib_yicGnss_init(&gps.receiver);
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
        processByte(ch);
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
