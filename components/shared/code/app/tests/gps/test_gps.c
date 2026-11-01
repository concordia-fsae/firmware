/**
 * @file test_gps.c
 * @brief GNSS application tests using real LwGPS, timers and DMA-fed module callbacks.
 *
 * The first three scenarios are adapted from jl/c-unit-test, commit 1aaf2341.
 * Unlike that branch, all sentences traverse the application's DMA consumer and
 * extractor; no parser-only APIs or private application state are exposed.
 */
#include "app_faultManager.h"
#include "app_gps.h"
#include "drv_outputAD.h"
#include "HW_uart.h"
#include "lwgps_opts.h"
#include "test_freertos.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

#define ARRAY_LEN(x)    (sizeof(x) / sizeof((x)[0]))
static volatile uint8_t *dma_buffer;
static uint32_t         dma_size;
static uint32_t         dma_write;
static bool             dma_running;
static unsigned         dma_starts;
static unsigned         dma_stops;
static bool             faults[TEST_FAULT_COUNT];
static bool             uart_enabled;

uint32_t HW_TIM_getTimeMS(void)                                          { return test_freertos.ticks; }
void     app_faultManager_setFaultState(test_fault_E fault, bool active) { faults[fault] = active; }
void     drv_outputAD_setDigitalActiveState(drv_outputAD_channelDigital_E channel, drv_io_activeState_E state)
{
    TEST_ASSERT_EQUAL_INT(DRV_OUTPUTAD_DIGITAL_MCU_UART_EN, channel);
    uart_enabled = state == DRV_IO_ACTIVE;
}
HW_StatusTypeDef_E HW_UART_startDMARX(HW_UART_port_E port, uint32_t *buffer, uint32_t size)
{
    TEST_ASSERT_EQUAL_INT(HW_UART_PORT_GPS, port);
    TEST_ASSERT_NOT_NULL(buffer);
    dma_buffer  = (volatile uint8_t*)buffer;
    dma_size    = size;
    dma_write   = 0U;
    dma_running = true;
    dma_starts++;
    return HW_OK;
}
HW_StatusTypeDef_E HW_UART_stopDMA(HW_UART_port_E port)
{
    TEST_ASSERT_EQUAL_INT(HW_UART_PORT_GPS, port);
    dma_running = false;
    dma_stops++;
    return HW_OK;
}
uint32_t HW_UART_getDmaRxRemaining(HW_UART_port_E port)
{
    TEST_ASSERT_EQUAL_INT(HW_UART_PORT_GPS, port);
    return dma_size - dma_write;
}
static void poll(void) { app_gps_desc.periodic100Hz_CLK(); }
static void inject(const void *bytes, size_t length)
{
    TEST_ASSERT_TRUE(dma_running);
    for (size_t i = 0U; i < length; i++)
    {
        dma_buffer[dma_write] = ((const uint8_t*)bytes)[i];
        dma_write             = (dma_write + 1U) % dma_size;
    }
}
static void receive_text(const char *sentence)
{
    inject(sentence, strlen(sentence));
    poll();
}
static void receive_payload(const char *payload)
{
    uint8_t checksum = 0U;

    for (const char *c = payload; *c; c++)
    {
        checksum ^= (uint8_t)*c;
    }
    char sentence[256];
    int  length = snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", payload, checksum);
    TEST_ASSERT_TRUE(length > 0 && (size_t)length < sizeof(sentence));
    receive_text(sentence);
}
void setUp(void)
{
    test_freertos_reset();
    memset(faults, 0, sizeof(faults));
    dma_starts   = 0U;
    dma_stops    = 0U;
    uart_enabled = false;
    app_gps_desc.moduleInit();
}
void tearDown(void) { TEST_ASSERT_EQUAL_UINT(0U, test_freertos.critical_depth); }

// Original component input from jl/c-unit-test, including whitespace and the bad RMC checksum.
static const char* const reference_sentences[] = {
    "$GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000*72\r\n",
    "$GNGLL,2447.0870,N,12100.5221,E,175258.000,A,D*42\r\n",
    "$GNGSA,A,3,21, 12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1*18\r\n",
    "$GPGSV,4,1,13,02,72,109,43,24,69,035,48,18,52,330,42,21,49,246,43,1*69\r\n",
    " $GNRMC,175258.000,A,2447.0870,N,12100.5220,E,000.0,000.0,220617,,,D*75\r\n",
    ": $GNVTG,000.0,T,,M,000.0,N,000.0,K,D*16\r\n",
    " $GNZDA,175258.000,22,06,2017,00,00*46\r\n",
    "$PAIRMSG,90,072520.000,3*59\r\n",
    "$PAIRMSG,91,072520.000,1,0*46\r\n",
};

static void provided_gnss_and_pairmsg_stream_updates_state(void)
{
    // Preserve the nominal scenario with the RMC checksum corrected.
    for (size_t i = 0U; i < ARRAY_LEN(reference_sentences); i++)
    {
        receive_text(i == 4U
            ? " $GNRMC,175258.000,A,2447.0870,N,12100.5220,E,000.0,000.0,220617,,,D*74\r\n"
            : reference_sentences[i]);
    }


    app_gps_pos_S     pos;
    app_gps_heading_S heading;
    app_gps_time_S    time;
    app_gps_pairmsg_S pairmsg;
    app_gps_getPos(&pos);
    app_gps_getHeading(&heading);
    app_gps_getTime(&time);
    app_gps_getPairmsg(&pairmsg);

    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    TEST_ASSERT_TRUE(app_gps_getValidDate());
    TEST_ASSERT_EQUAL_UINT8(15U, app_gps_getNumSatellites());
    TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_DIFFERENTIAL, app_gps_getQualityCAN());

    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 24.784784f,  pos.lat);
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 121.008705f, pos.lon);
    TEST_ASSERT_FLOAT_WITHIN(0.01f,    95.2f,       pos.alt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,   0.0f,        heading.course);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,   0.0f,        heading.speedMps);

    TEST_ASSERT_EQUAL_UINT8(22U, time.date);
    TEST_ASSERT_EQUAL_UINT8(6U,  time.month);
    TEST_ASSERT_EQUAL_UINT16(2017U, time.year);
    TEST_ASSERT_EQUAL_UINT8(17U, time.hours);
    TEST_ASSERT_EQUAL_UINT8(52U, time.minutes);
    TEST_ASSERT_EQUAL_UINT8(58U, time.seconds);

    TEST_ASSERT_EQUAL_UINT32(26720000U, pairmsg.utcMs);
    TEST_ASSERT_EQUAL_UINT8(3U, pairmsg.drStage);
    TEST_ASSERT_EQUAL_UINT8(1U, pairmsg.dynamicStatus);
    TEST_ASSERT_EQUAL_UINT8(0U, pairmsg.alarmStatus);

    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getCrcFailures());
    TEST_ASSERT_EQUAL_UINT16(3U, app_gps_getInvalidTransactions());
    TEST_ASSERT_EQUAL_UINT16(6U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGga());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGsa());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGsv());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountRmc());
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getSentenceCountPairMsg());
}

static void pairmsg_90_updates_dead_reckoning_stage(void)
{
    receive_text("$PAIRMSG,90,072520.000,3*59\r\n");

    app_gps_pairmsg_S pairmsg;
    app_gps_getPairmsg(&pairmsg);

    TEST_ASSERT_EQUAL_UINT32(26720000U, pairmsg.utcMs);
    TEST_ASSERT_EQUAL_UINT8(3U, pairmsg.drStage);
    TEST_ASSERT_EQUAL_UINT8(0U, pairmsg.dynamicStatus);
    TEST_ASSERT_EQUAL_UINT8(0U, pairmsg.alarmStatus);
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getCrcFailures());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getInvalidTransactions());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountPairMsg());
}

static void pairmsg_91_updates_dynamic_status(void)
{
    receive_text("$PAIRMSG,91,072520.000,1,0*46\r\n");

    app_gps_pairmsg_S pairmsg;
    app_gps_getPairmsg(&pairmsg);

    TEST_ASSERT_EQUAL_UINT32(26720000U, pairmsg.utcMs);
    TEST_ASSERT_EQUAL_UINT8(0U, pairmsg.drStage);
    TEST_ASSERT_EQUAL_UINT8(1U, pairmsg.dynamicStatus);
    TEST_ASSERT_EQUAL_UINT8(0U, pairmsg.alarmStatus);
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getCrcFailures());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getInvalidTransactions());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountPairMsg());
}


static void initialization_starts_dma_and_reports_no_data(void)
{
    TEST_ASSERT_TRUE(uart_enabled);
    TEST_ASSERT_EQUAL_UINT(1U, dma_starts);
    TEST_ASSERT_EQUAL_UINT32(10000U, dma_size);
    TEST_ASSERT_EACH_EQUAL_UINT8(0U, (const uint8_t*)dma_buffer, dma_size);
    poll();
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDATANOTVALID]);
    TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSOVERRUN]);
}
static void fragmented_sentence_waits_for_newline(void)
{
    const char *sentence = "$PAIRMSG,90,072520.000,3*59\r\n";

    for (size_t split = 1U; split < strlen(sentence); split++)
    {
        setUp();
        inject(sentence, split);
        poll();
        TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getNumberSamples());
        inject(sentence + split, strlen(sentence) - split);
        poll();
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
        TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getPairmsgRef()->drStage);
    }
}
static void pairmsg_checksum_respects_configuration(void)
{
    receive_text("$PAIRMSG,90,072520.000,3*00\r\n");
#if LWGPS_CFG_CRC
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getCrcFailures());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getNumberSamples());
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
#else
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getCrcFailures());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getPairmsgRef()->drStage);
    TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
#endif
}
static void oversized_sentence_is_dropped_and_next_sentence_recovers(void)
{
    char line[120];

    memset(line, 'x', sizeof(line));
    line[0]                 = '$';
    line[sizeof(line) - 1U] = '\n';
    inject(line, sizeof(line));
    poll();
    receive_payload("PAIRMSG,90,072520.000,3");
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getInvalidTransactions());
}
static void full_dma_buffer_is_discarded_and_reception_restarts(void)
{
    for (uint32_t i = 0U; i < dma_size; i++)
    {
        inject("x", 1U);
    }
    poll();
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSOVERRUN]);
    TEST_ASSERT_EQUAL_UINT(1U, dma_stops);
    TEST_ASSERT_EQUAL_UINT(2U, dma_starts);
    TEST_ASSERT_EACH_EQUAL_UINT8(0U, (const uint8_t*)dma_buffer, dma_size);
    receive_payload("PAIRMSG,90,072520.000,3");
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSOVERRUN]);
}

static void seed_fix(void)
{
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000");
    receive_payload("GNRMC,175258.000,A,2447.0870,N,12100.5220,E,10.0,123.4,220617,,,D");
    receive_payload("GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1");
}
static void invalid_pairmsg_fields_are_rejected_without_refreshing_health(void)
{
    const char *payloads[] = {
        "PAIRMSG",                  "PAIRMSG,",                "PAIRMSG,90",              "PAIRMSG,90,", "PAIRMSG,90,072520",
        "PAIRMSG,90,072520,",       "PAIRMSG,90,072520,-1",    "PAIRMSG,90,072520,256",
        "PAIRMSG,90,072520,3x",     "PAIRMSG,90,072520, 3",    "PAIRMSG,90,072520,3,4",
        "PAIRMSG,91,072520",        "PAIRMSG,91,072520,1",     "PAIRMSG,91,072520,,0",
        "PAIRMSG,91,072520,1,",     "PAIRMSG,91,072520,1,-1",  "PAIRMSG,91,072520,256,0",
        "PAIRMSG,91,072520,1,256",  "PAIRMSG,91,072520,1,0,9",
        "PAIRMSG,92,072520,3",      "PAIRMSG,90x,072520,3",    "PAIRMSG,-90,072520,3",
        "PAIRMSG,90,1,3",           "PAIRMSG,90,12345,3",      "PAIRMSG,90,1234567,3",
        "PAIRMSG,90,240000,3",      "PAIRMSG,90,006000,3",     "PAIRMSG,90,000060,3",
        "PAIRMSG,90,abcdef,3",      "PAIRMSG,90,072520.,3",    "PAIRMSG,90,072520.x,3",
        "PAIRMSG,90,072520.1234,3", "PAIRMSG,90,072520junk,3",
    };

    for (size_t i = 0U; i < ARRAY_LEN(payloads); i++)
    {
        setUp();
        receive_payload(payloads[i]);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(0U, app_gps_getNumberSamples(),       payloads[i]);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(1U, app_gps_getInvalidTransactions(), payloads[i]);
        TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
        TEST_ASSERT_EQUAL_UINT32(0U, app_gps_getPairmsgRef()->utcMs);
    }
}
static void invalid_checksum_formats_do_not_update_state(void)
{
    const char *sentences[] = {
        "$PAIRMSG,90,072520,3\n",            "$PAIRMSG,90,072520,3*\n",
        "$PAIRMSG,90,072520,3*5\n",          "$PAIRMSG,90,072520,3*ZZ\n",
        "$PAIRMSG,90,072520,3*59trailing\n", "$PAIRMSG,90,072520,3**59\n",
    };

    for (size_t i = 0U; i < ARRAY_LEN(sentences); i++)
    {
        setUp();
        receive_text(sentences[i]);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(0U, app_gps_getNumberSamples(), sentences[i]);
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getCrcFailures() + app_gps_getInvalidTransactions());
        TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
    }
}
static void standard_nmea_checksum_respects_configuration(void)
{
    seed_fix();
    const uint16_t samples = app_gps_getNumberSamples();
    receive_text("$GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,0.0,M,19.6,M,,0000*00\r\n");
#if LWGPS_CFG_CRC
    TEST_ASSERT_EQUAL_UINT16(samples, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(1U,      app_gps_getCrcFailures());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 95.2f, app_gps_getPosRef()->alt);
#else
    TEST_ASSERT_EQUAL_UINT16(samples + 1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(0U,           app_gps_getCrcFailures());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, app_gps_getPosRef()->alt);
#endif
}
static void multiple_sentences_in_one_dma_batch_are_extracted_once(void)
{
    const char *stream = "$PAIRMSG,90,072520.000,3*59\r\n$PAIRMSG,91,072520.000,1,0*46\r\n";

    receive_text(stream);
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getPairmsgRef()->drStage);
    TEST_ASSERT_EQUAL_UINT8(1U, app_gps_getPairmsgRef()->dynamicStatus);
    poll();
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getNumberSamples());
}
static void sentence_crossing_dma_wrap_is_extracted(void)
{
    // Move the producer/consumer together to every possible wrap point within a sentence.
    const char *sentence = "$PAIRMSG,90,072520.000,3*59\r\n";

    for (size_t split = 1U; split < strlen(sentence); split++)
    {
        setUp();
        for (uint32_t i = 0U; i < dma_size - split; i++)
        {
            inject("\n", 1U);
        }
        poll();
        receive_text(sentence);
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
        TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getPairmsgRef()->drStage);
        TEST_ASSERT_EQUAL_UINT(0U, dma_stops);
    }
}
static void garbage_and_truncated_sentence_resynchronize_at_dollar(void)
{
    receive_text("noise without newline$GNGGA,truncated");
    receive_text("$PAIRMSG,90,072520.000,3*59\r\n");
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getPairmsgRef()->drStage);
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getInvalidTransactions());
}
static void embedded_nul_does_not_stall_dma_consumption(void)
{
    static const char stream[] = "garbage\0$PAIRMSG,90,072520.000,3*59\r\n";

    inject(stream, sizeof(stream) - 1U);
    poll();
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
}
static void buffer_reset_drops_partial_sentence_and_recovers(void)
{
    receive_text("$PAIRMSG,90,072");
    HW_UART_stopDMA(HW_UART_PORT_GPS);
    app_gps_resetBuffers();
    TEST_ASSERT_EACH_EQUAL_UINT8(0U, (const uint8_t*)dma_buffer, dma_size);
    HW_UART_startDMARX(HW_UART_PORT_GPS, (uint32_t*)dma_buffer, dma_size);
    receive_text("520.000,3*59\r\n");
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getNumberSamples());
    receive_payload("PAIRMSG,90,072520.000,3");
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
}
static void overflow_after_partial_sentence_clears_both_buffers(void)
{
    receive_text("$PAIRMSG,90,072");
    for (uint32_t i = 0U; i < dma_size + 17U; i++)
    {
        inject("x", 1U);
    }
    poll();
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSOVERRUN]);
    receive_text("520.000,3*59\r\n");
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getNumberSamples());
    receive_payload("PAIRMSG,91,072520.000,1,0");
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getPairmsgRef()->drStage);
}
static void timeout_boundary_and_valid_stream_recovery(void)
{
    seed_fix();
    TEST_ASSERT_TRUE(app_gps_isValid());
    test_freertos.ticks = 1999U;
    poll();
    TEST_ASSERT_TRUE(app_gps_isValid());
    test_freertos.ticks = 2000U;
    poll();
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
    TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDATANOTVALID]);
    seed_fix();
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
}
static void invalid_traffic_does_not_prevent_timeout(void)
{
    seed_fix();
    test_freertos.ticks = 1999U;
    receive_payload("PAIRMSG,90,072520.000,256");
    test_freertos.ticks = 2000U;
    poll();
    TEST_ASSERT_FALSE(app_gps_isValid());
}
static void timeout_handles_clock_wraparound(void)
{
    test_freertos.ticks = UINT32_MAX - 999U;
    seed_fix();
    test_freertos.ticks = 999U;
    TEST_ASSERT_TRUE(app_gps_isValid());
    test_freertos.ticks = 1000U;
    TEST_ASSERT_FALSE(app_gps_isValid());
}
static void uart_error_bits_are_counted_independently(void)
{
    app_gps_recordUartError(0U);
    app_gps_recordUartError(HAL_UART_ERROR_ORE);
    app_gps_recordUartError(HAL_UART_ERROR_FE);
    app_gps_recordUartError(HAL_UART_ERROR_NE);
    app_gps_recordUartError(HAL_UART_ERROR_PE);
    app_gps_recordUartError(HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE | HAL_UART_ERROR_NE | HAL_UART_ERROR_PE);
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getUartErrorOreCount());
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getUartErrorFeCount());
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getUartErrorNeCount());
    TEST_ASSERT_EQUAL_UINT16(2U, app_gps_getUartErrorPeCount());
}
static void new_init_clears_state_and_diagnostics(void)
{
    seed_fix();
    app_gps_recordUartError(HAL_UART_ERROR_ORE);
    app_gps_desc.moduleInit();
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getUartErrorOreCount());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, app_gps_getPosRef()->lat);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, app_gps_getHeadingRef()->speedMps);
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getTimeRef()->year);
}
static void quality_validity_and_signed_navigation(void)
{
    TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_FIX_NONE, app_gps_getQualityCAN());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
    receive_payload("GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1");
    const unsigned                  qualities[] = { 0, 1, 2, 6, 7 };
    const CAN_gpsQualityIndicator_E expected[]  = {
        CAN_GPSQUALITYINDICATOR_FIX_NONE,     CAN_GPSQUALITYINDICATOR_SPS,
        CAN_GPSQUALITYINDICATOR_DIFFERENTIAL, CAN_GPSQUALITYINDICATOR_DEAD_RECKONING,
        CAN_GPSQUALITYINDICATOR_FIX_NONE,
    };
    for (size_t i = 0; i < ARRAY_LEN(qualities); i++)
    {
        char payload[100];
        snprintf(payload, sizeof(payload), "GNGGA,175258.000,2447.0870,S,12100.5221,W,%u,15,0.7,95.2,M,19.6,M,,0000", qualities[i]);
        receive_payload(payload);
        receive_payload("GNRMC,175258.000,A,2447.0870,S,12100.5220,W,10.0,123.4,220617,,,D");
        TEST_ASSERT_EQUAL_INT(expected[i], app_gps_getQualityCAN());
        TEST_ASSERT_EQUAL(qualities[i] == 1 || qualities[i] == 2 || qualities[i] == 6, app_gps_isValid());
    }
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, -24.784784f,  app_gps_getPosRef()->lat);
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, -121.008705f, app_gps_getPosRef()->lon);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,   5.14444f,     app_gps_getHeadingRef()->speedMps);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,   123.4f,       app_gps_getHeadingRef()->course);
    receive_payload("GNRMC,175258.000,V,2447.0870,S,12100.5220,W,10.0,123.4,220617,,,D");
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
    seed_fix();
    TEST_ASSERT_TRUE(app_gps_isValid());
}
static void pairmsg_numeric_boundaries_and_field_preservation(void)
{
    const char     *times[]   = { "000000", "235959.999", "123456.1", "123456.12" };
    const uint32_t expected[] = { 0, 86399999, 45296100, 45296120 };

    receive_payload("PAIRMSG,91,000000,255,255");
    for (size_t i = 0; i < ARRAY_LEN(times); i++)
    {
        char payload[80];
        snprintf(payload, sizeof(payload), "PAIRMSG,90,%s,255", times[i]);
        receive_payload(payload);
        TEST_ASSERT_EQUAL_UINT32(expected[i], app_gps_getPairmsgRef()->utcMs);
        TEST_ASSERT_EQUAL_UINT8(255, app_gps_getPairmsgRef()->drStage);
        TEST_ASSERT_EQUAL_UINT8(255, app_gps_getPairmsgRef()->dynamicStatus);
        TEST_ASSERT_EQUAL_UINT8(255, app_gps_getPairmsgRef()->alarmStatus);
    }
    receive_payload("PAIRMSG,91,000000,0,0");
    TEST_ASSERT_EQUAL_UINT8(255, app_gps_getPairmsgRef()->drStage);
    TEST_ASSERT_EQUAL_UINT8(0,   app_gps_getPairmsgRef()->dynamicStatus);
}
static void binary_noise_inside_sentence_is_rejected_and_recovers(void)
{
    const uint8_t invalid[] = { 0, 1, 31, 127, 255 };

    for (size_t i = 0; i < ARRAY_LEN(invalid); i++)
    {
        receive_text("$PAIRMSG,90,");
        inject(&invalid[i], 1);
        receive_text("072520.000,3*59\r\n");
    }
    TEST_ASSERT_EQUAL_UINT16(ARRAY_LEN(invalid), app_gps_getInvalidTransactions());
    TEST_ASSERT_EQUAL_UINT16(0,                  app_gps_getNumberSamples());
    receive_text("$PAIRMSG,90,072520.000,3*59\n");
    TEST_ASSERT_EQUAL_UINT16(1, app_gps_getNumberSamples());
}
static void sentence_length_boundary_and_lowercase_checksum(void)
{
    // Leading zeroes preserve the numeric value while exercising exact frame size.
    char       payload[79];
    const char *prefix = "PAIRMSG,90,000000,";

    memset(payload, '0', sizeof(payload));
    memcpy(payload, prefix, strlen(prefix));
    payload[77] = '\0';    // 77 payload bytes + '$*XX\r\n' = 83.
    receive_payload(payload);
    TEST_ASSERT_EQUAL_UINT16(1, app_gps_getNumberSamples());
    payload[77] = '0';
    payload[78] = '\0';
    receive_payload(payload);
    TEST_ASSERT_EQUAL_UINT16(1, app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(1, app_gps_getInvalidTransactions());
    receive_text("$PAIRMSG,90,072520.000,0*5a\n");
    TEST_ASSERT_EQUAL_UINT16(2, app_gps_getNumberSamples());
    receive_text("$PAIRMSG,90,072520.000,0*5Z\n");
    TEST_ASSERT_EQUAL_UINT16(1, app_gps_getCrcFailures());
}
static void navigation_without_time_does_not_claim_valid_time(void)
{
    receive_payload("GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1");
    TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGsa());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
    TEST_ASSERT_FALSE(app_gps_isValid());
    seed_fix();
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    TEST_ASSERT_TRUE(app_gps_getValidDate());
}
static void original_reference_stream_is_extracted_from_one_dma_batch(void)
{
    for (size_t i = 0U; i < ARRAY_LEN(reference_sentences); i++)
    {
        inject(reference_sentences[i], strlen(reference_sentences[i]));
    }
    poll();
    const uint16_t accepted = LWGPS_CFG_CRC ? 5U : 6U;
    TEST_ASSERT_EQUAL_UINT16(accepted,                app_gps_getNumberSamples());
    TEST_ASSERT_EQUAL_UINT16(LWGPS_CFG_CRC ? 1U : 0U, app_gps_getCrcFailures());
    TEST_ASSERT_EQUAL_UINT16(3U,                      app_gps_getInvalidTransactions());
    TEST_ASSERT_EQUAL_UINT16(1U,                      app_gps_getSentenceCountGga());
    TEST_ASSERT_EQUAL_UINT16(1U,                      app_gps_getSentenceCountGsa());
    TEST_ASSERT_EQUAL_UINT16(1U,                      app_gps_getSentenceCountGsv());
    TEST_ASSERT_EQUAL_UINT16(LWGPS_CFG_CRC ? 0U : 1U, app_gps_getSentenceCountRmc());
    TEST_ASSERT_EQUAL_UINT16(2U,                      app_gps_getSentenceCountPairMsg());
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getPairmsgRef()->drStage);
    TEST_ASSERT_EQUAL_UINT8(1U, app_gps_getPairmsgRef()->dynamicStatus);
    TEST_ASSERT_EQUAL(!LWGPS_CFG_CRC, app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT(0U, dma_stops);
    poll();
    TEST_ASSERT_EQUAL_UINT16(accepted, app_gps_getNumberSamples());
    receive_text(" $GNRMC,175258.000,A,2447.0870,N,12100.5220,E,000.0,000.0,220617,,,D*74\r\n");
    TEST_ASSERT_EQUAL_UINT16(accepted + 1U, app_gps_getNumberSamples());
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    TEST_ASSERT_TRUE(app_gps_getValidDate());
}


// YIC GT-505GGBL5-DR sections 3.1/3.3: GGA quality describes the
// positioning method; GSA mode describes fix dimension independently.
static void gga_quality_and_gsa_dimension_are_independent(void)
{
    const unsigned qualities[]  = { 0, 1, 2, 3, 6, 7 };
    const unsigned dimensions[] = { 0, 1, 2, 3, 4 };

    for (size_t q = 0U; q < ARRAY_LEN(qualities); q++)
    {
        for (size_t d = 0U; d < ARRAY_LEN(dimensions); d++)
        {
            setUp();
            char payload[100];
            snprintf(payload, sizeof(payload), "GNGGA,175258.000,2447.0870,N,12100.5221,E,%u,15,0.7,95.2,M,19.6,M,,0000", qualities[q]);
            receive_payload(payload);
            snprintf(payload, sizeof(payload), "GNGSA,A,%u,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1", dimensions[d]);
            receive_payload(payload);
            const CAN_gpsQualityIndicator_E methods[] = {
                CAN_GPSQUALITYINDICATOR_FIX_NONE,       CAN_GPSQUALITYINDICATOR_SPS,
                CAN_GPSQUALITYINDICATOR_DIFFERENTIAL,   CAN_GPSQUALITYINDICATOR_PPS,
                CAN_GPSQUALITYINDICATOR_DEAD_RECKONING, CAN_GPSQUALITYINDICATOR_FIX_NONE,
            };
            const CAN_gpsQualityIndicator_E expected = methods[q];
            TEST_ASSERT_EQUAL_UINT8(dimensions[d] <= 3U ? dimensions[d] : 0U, app_gps_getFixDimension());
            TEST_ASSERT_EQUAL_INT(expected, app_gps_getQualityCAN());
        }
    }
}
static void fix_dimension_transitions_without_new_gga(void)
{
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000");
    receive_payload("GNRMC,175258.000,A,2447.0870,N,12100.5220,E,10.0,123.4,220617,,,D");
    // GGA/RMC without a GSA dimension cannot establish a complete GNSS fix.
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getFixDimension());
    const unsigned dimensions[] = { 3, 2, 1, 3 };
    const uint8_t  expected[]   = { 3U, 2U, 1U, 3U };
    for (size_t i = 0U; i < ARRAY_LEN(dimensions); i++)
    {
        char payload[100];
        snprintf(payload, sizeof(payload), "GNGSA,A,%u,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1", dimensions[i]);
        receive_payload(payload);
        TEST_ASSERT_EQUAL_UINT8(expected[i], app_gps_getFixDimension());
        TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_DIFFERENTIAL, app_gps_getQualityCAN());
    }
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,0,00,0.7,95.2,M,19.6,M,,0000");
    TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_FIX_NONE, app_gps_getQualityCAN());
    TEST_ASSERT_FALSE(app_gps_isValid());
}
static void supported_navigation_sentences_accept_gp_and_gn_talkers(void)
{
    const char* payloads[] = {
        "GGA,175258.000,2447.0870,N,12100.5221,E,1,15,0.7,95.2,M,19.6,M,,0000",
        "GSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1",
        "GSV,4,1,13,02,72,109,43,24,69,035,48,18,52,330,42,21,49,246,43,1",
        "RMC,175258.000,A,2447.0870,N,12100.5220,E,10.0,123.4,220617,,,A",
    };
    const char* talkers[] = { "GP", "GN" };

    for (size_t t = 0U; t < ARRAY_LEN(talkers); t++)
    {
        setUp();
        for (size_t i = 0U; i < ARRAY_LEN(payloads); i++)
        {
            char payload[100];
            snprintf(payload, sizeof(payload), "%s%s", talkers[t], payloads[i]);
            receive_payload(payload);
        }
        TEST_ASSERT_EQUAL_UINT16(4U, app_gps_getNumberSamples());
        TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getInvalidTransactions());
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGga());
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGsa());
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountGsv());
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getSentenceCountRmc());
        TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_SPS, app_gps_getQualityCAN());
        TEST_ASSERT_TRUE(app_gps_isValid());
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.144f, app_gps_getHeadingRef()->speedMps);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 123.4f, app_gps_getHeadingRef()->course);
    }
}
// These documented messages are not implemented by the pinned LwGPS version.
// Explicitly characterize the gap; accepting them is not claimed by this suite.
static void unsupported_documented_sentences_do_not_mutate_or_refresh_navigation(void)
{
    const char* payloads[] = {
        "GNGLL,2447.0870,N,12100.5221,E,175258.000,A,D",
        "GNVTG,000.0,T,,M,000.0,N,000.0,K,D",
        "GNZDA,175258.000,22,06,2017,00,00",
        "GLGSV,1,1,01,65,72,109,43,1",
        "GAGSV,1,1,01,02,72,109,43,1",
        "GBGSV,1,1,01,02,72,109,43,1",
        "BDGSV,1,1,01,02,72,109,43,1",
    };

    for (size_t i = 0U; i < ARRAY_LEN(payloads); i++)
    {
        setUp();
        seed_fix();
        app_gps_pos_S before;
        app_gps_getPos(&before);
        test_freertos.ticks = 1999U;
        receive_payload(payloads[i]);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(1U, app_gps_getInvalidTransactions(), payloads[i]);
        TEST_ASSERT_EQUAL_UINT16(3U, app_gps_getNumberSamples());
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, before.lat, app_gps_getPosRef()->lat);
        TEST_ASSERT_FLOAT_WITHIN(0.00001f, before.lon, app_gps_getPosRef()->lon);
        TEST_ASSERT_FLOAT_WITHIN(0.001f,   95.2f,      app_gps_getPosRef()->alt);
        TEST_ASSERT_FLOAT_WITHIN(0.001f,   5.144f,     app_gps_getHeadingRef()->speedMps);
        TEST_ASSERT_EQUAL_UINT16(2017U, app_gps_getTimeRef()->year);
        test_freertos.ticks = 2000U;
        poll();
        TEST_ASSERT_FALSE(app_gps_isValid());
    }
}
static void every_reference_sentence_survives_fragmentation_and_dma_wrap(void)
{
    for (size_t i = 0U; i < ARRAY_LEN(reference_sentences); i++)
    {
        setUp();
        receive_text(reference_sentences[i]);
        const uint16_t samples = app_gps_getNumberSamples();
        const uint16_t invalid = app_gps_getInvalidTransactions();
        const uint16_t crc     = app_gps_getCrcFailures();
        const size_t   length  = strlen(reference_sentences[i]);
        for (size_t split = 1U; split < length; split++)
        {
            setUp();
            // The fragment ends exactly at the DMA wrap boundary.
            for (size_t j = 0U; j < dma_size - split; j++)
            {
                inject("\n", 1U);
            }
            poll();
            inject(reference_sentences[i], split);
            poll();
            TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getNumberSamples());
            inject(reference_sentences[i] + split, length - split);
            poll();
            TEST_ASSERT_EQUAL_UINT16(samples, app_gps_getNumberSamples());
            TEST_ASSERT_EQUAL_UINT16(invalid, app_gps_getInvalidTransactions());
            TEST_ASSERT_EQUAL_UINT16(crc,     app_gps_getCrcFailures());
            TEST_ASSERT_EQUAL_UINT(0U, dma_stops);
        }
    }
}
static void pairmsg_documented_stages_dynamic_states_and_alarm_bits(void)
{
    for (unsigned stage = 0U; stage <= 3U; stage++)
    {
        char payload[80];
        snprintf(payload, sizeof(payload), "PAIRMSG,90,235959.999,%u", stage);
        receive_payload(payload);
        TEST_ASSERT_EQUAL_UINT8(stage, app_gps_getPairmsgRef()->drStage);
        TEST_ASSERT_EQUAL_UINT32(86399999U, app_gps_getPairmsgRef()->utcMs);
    }
    for (unsigned state = 0U; state <= 2U; state++)
    {
        for (unsigned bit = 0U; bit < 8U; bit++)
        {
            char payload[80];
            snprintf(payload, sizeof(payload), "PAIRMSG,91,000000.000,%u,%u", state, 1U << bit);
            receive_payload(payload);
            TEST_ASSERT_EQUAL_UINT8(state,     app_gps_getPairmsgRef()->dynamicStatus);
            TEST_ASSERT_EQUAL_UINT8(1U << bit, app_gps_getPairmsgRef()->alarmStatus);
            TEST_ASSERT_EQUAL_UINT8(3U,        app_gps_getPairmsgRef()->drStage);
        }
    }
    TEST_ASSERT_EQUAL_UINT16(28U, app_gps_getSentenceCountPairMsg());
    TEST_ASSERT_EQUAL_UINT16(0U,  app_gps_getInvalidTransactions());
}
static void multi_sentence_gsv_and_gsa_system_ids_preserve_navigation(void)
{
    seed_fix();
    for (unsigned system = 1U; system <= 4U; system++)
    {
        char payload[100];
        snprintf(payload, sizeof(payload), "GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,%u", system);
        receive_payload(payload);
    }
    for (unsigned signal = 1U; signal <= 4U; signal += 3U)
    {
        for (unsigned sequence = 1U; sequence <= 4U; sequence++)
        {
            char      payload[100];
            const char* satellites[] = {
                "02,72,109,,24,69,035,48,18,52,330,42,21,49,246,43",
                "03,45,100,40,04,40,120,35,05,35,140,30,06,30,160,25",
                "07,25,180,20,08,20,200,15,09,15,220,10,10,10,240,05",
                "11,05,260,",
            };
            snprintf(payload, sizeof(payload), "GPGSV,4,%u,13,%s,%u", sequence, satellites[sequence - 1U], signal);
            receive_payload(payload);
        }
    }
    TEST_ASSERT_EQUAL_UINT16(5U, app_gps_getSentenceCountGsa());
    TEST_ASSERT_EQUAL_UINT16(8U, app_gps_getSentenceCountGsv());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getInvalidTransactions());
    TEST_ASSERT_EQUAL_UINT8(15U, app_gps_getNumSatellites());
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 24.784784f, app_gps_getPosRef()->lat);
    TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_DIFFERENTIAL, app_gps_getQualityCAN());
}

static void gga_and_gsa_metrics_are_independent(void)
{
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getQualityCAN());
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getFixDimension());
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000");
    TEST_ASSERT_EQUAL_UINT8(2U, app_gps_getQualityCAN());
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getFixDimension());
    receive_payload("GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1");
    TEST_ASSERT_EQUAL_UINT8(2U, app_gps_getQualityCAN());
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getFixDimension());
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,3,15,0.7,95.2,M,19.6,M,,0000");
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getQualityCAN());
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,6,00,0.7,95.2,M,19.6,M,,0000");
    TEST_ASSERT_EQUAL_UINT8(4U, app_gps_getQualityCAN());
    TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getFixDimension());
}


static void auxiliary_traffic_cannot_keep_stale_navigation_valid(void)
{
    const char* auxiliary[] = {
        "PAIRMSG,90,072520.000,3",                                 "PAIRMSG,91,072520.000,1,0",
        "GPGSV,1,1,01,02,72,109,43,1",
        "GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1",
    };

    for (size_t i = 0U; i < ARRAY_LEN(auxiliary); i++)
    {
        setUp();
        seed_fix();
        test_freertos.ticks = 1999U;
        receive_payload(auxiliary[i]);
        test_freertos.ticks = 2000U;
        poll();
        TEST_ASSERT_FALSE(app_gps_isValid());
        TEST_ASSERT_FALSE(app_gps_getValidTime());
        TEST_ASSERT_FALSE(app_gps_getValidDate());
        TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDATANOTVALID]);
        TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
        TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getQualityCAN());
        seed_fix();
        TEST_ASSERT_TRUE(app_gps_isValid());
    }
}
static void gga_and_rmc_must_each_be_fresh(void)
{
    const char* oneSource[] = {
        "GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000",
        "GNRMC,175258.000,A,2447.0870,N,12100.5220,E,10.0,123.4,220617,,,D",
    };

    for (size_t i = 0U; i < ARRAY_LEN(oneSource); i++)
    {
        setUp();
        seed_fix();
        test_freertos.ticks = 1999U;
        receive_payload(oneSource[i]);
        receive_payload("GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1");
        TEST_ASSERT_TRUE(app_gps_isValid());
        test_freertos.ticks = 2000U;
        poll();
        TEST_ASSERT_FALSE(app_gps_isValid());
        TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
        receive_payload(oneSource[1U - i]);
        TEST_ASSERT_TRUE(app_gps_isValid());
    }
}
static void gsa_no_fix_and_expiration_invalidate_gnss_but_allow_dead_reckoning(void)
{
    seed_fix();
    receive_payload("GNGSA,A,1,,,,,,,,,,,,,1.2,0.7,1.0,1");
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT8(1U, app_gps_getFixDimension());
    receive_payload("GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1");
    TEST_ASSERT_TRUE(app_gps_isValid());
    test_freertos.ticks = 2000U;
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000");
    receive_payload("GNRMC,175258.000,A,2447.0870,N,12100.5220,E,10.0,123.4,220617,,,D");
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getFixDimension());
    TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_DIFFERENTIAL, app_gps_getQualityCAN());
    receive_payload("GNGGA,175258.000,2447.0870,N,12100.5221,E,6,00,0.7,95.2,M,19.6,M,,0000");
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT8(4U, app_gps_getQualityCAN());
    receive_payload("GNGSA,A,1,,,,,,,,,,,,,1.2,0.7,1.0,1");
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT8(1U, app_gps_getFixDimension());
}
static void buffer_recovery_invalidates_cached_fix_until_navigation_returns(void)
{
    seed_fix();
    HW_UART_stopDMA(HW_UART_PORT_GPS);
    app_gps_resetBuffers();
    HW_UART_startDMARX(HW_UART_PORT_GPS, (uint32_t*)dma_buffer, dma_size);
    TEST_ASSERT_FALSE(app_gps_isValid());
    receive_payload("PAIRMSG,90,072520.000,3");
    TEST_ASSERT_FALSE(app_gps_isValid());
    seed_fix();
    TEST_ASSERT_TRUE(app_gps_isValid());
}

static void receive_with_field(const char* payload, unsigned index, const char* replacement)
{
    const char* start = payload;

    for (unsigned i = 0U; i < index; i++)
    {
        start = strchr(start, ',');
        TEST_ASSERT_NOT_NULL(start);
        start++;
    }
    const char* end = strchr(start, ',');
    if (end == NULL)
    {
        end = payload + strlen(payload);
    }
    char      changed[200];
    const int length = snprintf(changed, sizeof(changed), "%.*s%s%s", (int)(start - payload), payload, replacement, end);
    TEST_ASSERT_TRUE(length > 0 && (size_t)length < sizeof(changed));
    receive_payload(changed);
}
static const char* const valid_navigation[] = {
    "GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000",
    "GNRMC,175258.000,A,2447.0870,N,12100.5220,E,10.0,123.4,220617,,,D",
    "GNGSA,A,3,21,12,15,18,20,24,10,32,25,13,,,1.2,0.7,1.0,1",
};
static void malformed_navigation_fields_invalidate_their_source(void)
{
    const struct { unsigned source; unsigned field; const char* value; } cases[] = {
        { 0, 1, "240000"        }, { 0, 1, "126000"    }, { 0,  1, "120060"    }, { 0, 1, "abcdef"     },
        { 0, 1, "123456.x"      }, { 0, 2, "2460.0000" }, { 0,  2, "9100.0000" },
        { 0, 2, "9000.0001"     }, { 0, 2, ""          }, { 0,  3, "X"         }, { 0, 4, "18100.0000" },
        { 0, 4, "18000.0001"    }, { 0, 5, "N"         }, { 0,  6, "7"         }, { 0, 6, "255"        },
        { 0, 6, "-1"            }, { 0, 6, "1x"        }, { 0,  7, "57"        }, { 0, 7, "abc"        },
        { 0, 8, "NaN"           }, { 0, 9, "nan"       }, { 0,  9, "inf"       }, { 0, 9, "18000"      }, { 0, 10, "F" },
        { 1, 1, "240000"        }, { 1, 2, "X"         }, { 1,  3, ""          }, { 1, 4, "X"          },
        { 1, 7, "-1"            }, { 1, 7, "nan"       }, { 1,  7, ""          }, { 1, 8, "360"        },
        { 1, 9, "310221"        }, { 1, 9, "290223"    }, { 1,  9, "001224"    },
        { 1, 9, "011324"        }, { 1, 9, "abcdef"    }, { 1, 12, "X"         },
        { 2, 1, "X"             }, { 2, 2, "0"         }, { 2,  2, "4"         }, { 2, 2, ""           }, { 2, 18, "5" },
        { 1, 7, "0000000000001" },
    };
    for (size_t i = 0U; i < ARRAY_LEN(cases); i++)
    {
        setUp();
        seed_fix();
        test_freertos.ticks = 1999U;
        receive_with_field(valid_navigation[cases[i].source], cases[i].field, cases[i].value);
        TEST_ASSERT_FALSE_MESSAGE(app_gps_isValid(), cases[i].value);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(1U, app_gps_getInvalidTransactions(), cases[i].value);
        TEST_ASSERT_EQUAL_UINT16(3U, app_gps_getNumberSamples());
        TEST_ASSERT_FALSE(app_gps_getValidTime());
        TEST_ASSERT_FALSE(app_gps_getValidDate());
        test_freertos.ticks = 2000U;
        poll();
        TEST_ASSERT_TRUE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
        receive_payload(valid_navigation[2]);
        seed_fix();
        TEST_ASSERT_TRUE(app_gps_isValid());
    }
}
static void truncated_or_spoofed_navigation_cannot_reuse_previous_fix(void)
{
    const char* frames[] = { "GNGGA", "GNGGAjunk,1", "GNRMC,123456,A", "GNGSA,A" };

    for (size_t i = 0U; i < ARRAY_LEN(frames); i++)
    {
        setUp();
        seed_fix();
        receive_payload(frames[i]);
        TEST_ASSERT_FALSE(app_gps_isValid());
        TEST_ASSERT_EQUAL_UINT16(1U, app_gps_getInvalidTransactions());
    }
}
static void explicit_no_fix_states_clear_validity_in_any_sentence_order(void)
{
    const unsigned orders[][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };

    for (size_t i = 0U; i < ARRAY_LEN(orders); i++)
    {
        setUp();
        for (size_t j = 0U; j < 3U; j++)
        {
            receive_payload(valid_navigation[orders[i][j]]);
        }
        TEST_ASSERT_TRUE(app_gps_isValid());
        TEST_ASSERT_EQUAL_UINT8(2U, app_gps_getQualityCAN());
        TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getFixDimension());
        receive_with_field(valid_navigation[0], 6, "0");
        TEST_ASSERT_FALSE(app_gps_isValid());
        TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getQualityCAN());
        TEST_ASSERT_EQUAL_UINT8(3U, app_gps_getFixDimension());
        receive_payload(valid_navigation[0]);
        TEST_ASSERT_TRUE(app_gps_isValid());
        receive_with_field(valid_navigation[1], 2, "V");
        TEST_ASSERT_FALSE(app_gps_isValid());
        receive_payload(valid_navigation[1]);
        TEST_ASSERT_TRUE(app_gps_isValid());
        receive_with_field(valid_navigation[1], 12, "N");
        TEST_ASSERT_FALSE(app_gps_isValid());
        receive_payload(valid_navigation[1]);
        TEST_ASSERT_TRUE(app_gps_isValid());
        receive_with_field(valid_navigation[2], 2, "1");
        TEST_ASSERT_FALSE(app_gps_isValid());
        TEST_ASSERT_EQUAL_UINT8(2U, app_gps_getQualityCAN());
        TEST_ASSERT_EQUAL_UINT8(1U, app_gps_getFixDimension());
    }
}
static void valid_empty_time_and_date_do_not_claim_clock_validity(void)
{
    seed_fix();
    receive_with_field(valid_navigation[0], 1, "");
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    receive_payload(valid_navigation[0]);
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    receive_with_field(valid_navigation[1], 9, "");
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
    receive_with_field(valid_navigation[1], 9, "290224");
    TEST_ASSERT_TRUE(app_gps_getValidDate());
    TEST_ASSERT_EQUAL_UINT8(29U, app_gps_getTimeRef()->date);
    TEST_ASSERT_EQUAL_UINT16(2024U, app_gps_getTimeRef()->year);
}
static void no_fix_reports_with_empty_coordinates_are_accepted_as_no_fix(void)
{
    seed_fix();
    receive_payload("GNGGA,175258.000,,,,,0,00,,,,,,,");
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT8(0U, app_gps_getQualityCAN());
    receive_payload("GNRMC,175258.000,V,,,,,,,220617,,,N");
    TEST_ASSERT_FALSE(app_gps_isValid());
    TEST_ASSERT_EQUAL_UINT16(0U, app_gps_getInvalidTransactions());
    TEST_ASSERT_FALSE(faults[FM_FAULT_VCFRONT_GPSDEVICEERROR]);
    seed_fix();
    TEST_ASSERT_TRUE(app_gps_isValid());
}
static void standard_lf_only_sentences_commit_parser_state(void)
{
    receive_text("$GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000*72\n");
    receive_text("$GNRMC,175258.000,A,2447.0870,N,12100.5220,E,000.0,000.0,220617,,,D*74\n");
    receive_payload(valid_navigation[2]);
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    TEST_ASSERT_TRUE(app_gps_getValidDate());
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 24.784784f, app_gps_getPosRef()->lat);
    TEST_ASSERT_EQUAL_UINT16(2017U, app_gps_getTimeRef()->year);
}


static void clock_validity_requires_matching_gga_and_rmc_seconds(void)
{
    seed_fix();
    receive_with_field(valid_navigation[0], 1, "000000.000");
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
    receive_payload("GNRMC,000000.000,A,2447.0870,N,12100.5220,E,10.0,123.4,230617,,,D");
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    TEST_ASSERT_TRUE(app_gps_getValidDate());
    TEST_ASSERT_EQUAL_UINT8(0U,  app_gps_getTimeRef()->hours);
    TEST_ASSERT_EQUAL_UINT8(23U, app_gps_getTimeRef()->date);
    receive_with_field(valid_navigation[1], 1, "");
    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_FALSE(app_gps_getValidTime());
    TEST_ASSERT_FALSE(app_gps_getValidDate());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(clock_validity_requires_matching_gga_and_rmc_seconds);
    RUN_TEST(malformed_navigation_fields_invalidate_their_source);
    RUN_TEST(truncated_or_spoofed_navigation_cannot_reuse_previous_fix);
    RUN_TEST(explicit_no_fix_states_clear_validity_in_any_sentence_order);
    RUN_TEST(valid_empty_time_and_date_do_not_claim_clock_validity);
    RUN_TEST(no_fix_reports_with_empty_coordinates_are_accepted_as_no_fix);
    RUN_TEST(standard_lf_only_sentences_commit_parser_state);

    RUN_TEST(auxiliary_traffic_cannot_keep_stale_navigation_valid);
    RUN_TEST(gga_and_rmc_must_each_be_fresh);
    RUN_TEST(gsa_no_fix_and_expiration_invalidate_gnss_but_allow_dead_reckoning);
    RUN_TEST(buffer_recovery_invalidates_cached_fix_until_navigation_returns);

    RUN_TEST(gga_and_gsa_metrics_are_independent);
    RUN_TEST(gga_quality_and_gsa_dimension_are_independent);
    RUN_TEST(fix_dimension_transitions_without_new_gga);
    RUN_TEST(supported_navigation_sentences_accept_gp_and_gn_talkers);
    RUN_TEST(unsupported_documented_sentences_do_not_mutate_or_refresh_navigation);
    RUN_TEST(every_reference_sentence_survives_fragmentation_and_dma_wrap);
    RUN_TEST(pairmsg_documented_stages_dynamic_states_and_alarm_bits);
    RUN_TEST(multi_sentence_gsv_and_gsa_system_ids_preserve_navigation);

    RUN_TEST(original_reference_stream_is_extracted_from_one_dma_batch);
    RUN_TEST(navigation_without_time_does_not_claim_valid_time);
    RUN_TEST(quality_validity_and_signed_navigation);
    RUN_TEST(pairmsg_numeric_boundaries_and_field_preservation);
    RUN_TEST(binary_noise_inside_sentence_is_rejected_and_recovers);
    RUN_TEST(sentence_length_boundary_and_lowercase_checksum);
    RUN_TEST(provided_gnss_and_pairmsg_stream_updates_state);
    RUN_TEST(pairmsg_90_updates_dead_reckoning_stage);
    RUN_TEST(pairmsg_91_updates_dynamic_status);
    RUN_TEST(initialization_starts_dma_and_reports_no_data);
    RUN_TEST(fragmented_sentence_waits_for_newline);
    RUN_TEST(pairmsg_checksum_respects_configuration);
    RUN_TEST(oversized_sentence_is_dropped_and_next_sentence_recovers);
    RUN_TEST(full_dma_buffer_is_discarded_and_reception_restarts);
    RUN_TEST(invalid_pairmsg_fields_are_rejected_without_refreshing_health);
    RUN_TEST(invalid_checksum_formats_do_not_update_state);
    RUN_TEST(standard_nmea_checksum_respects_configuration);
    RUN_TEST(multiple_sentences_in_one_dma_batch_are_extracted_once);
    RUN_TEST(sentence_crossing_dma_wrap_is_extracted);
    RUN_TEST(garbage_and_truncated_sentence_resynchronize_at_dollar);
    RUN_TEST(embedded_nul_does_not_stall_dma_consumption);
    RUN_TEST(buffer_reset_drops_partial_sentence_and_recovers);
    RUN_TEST(overflow_after_partial_sentence_clears_both_buffers);
    RUN_TEST(timeout_boundary_and_valid_stream_recovery);
    RUN_TEST(invalid_traffic_does_not_prevent_timeout);
    RUN_TEST(timeout_handles_clock_wraparound);
    RUN_TEST(uart_error_bits_are_counted_independently);
    RUN_TEST(new_init_clears_state_and_diagnostics);
    return UNITY_END();
}
