#include <stddef.h>
#include "app_gps.h"
#include "unity.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0U]))

void setUp(void)
{
}

void tearDown(void)
{
}

static void parse_all(const char* const* sentences, size_t count)
{
    for (size_t i = 0U; i < count; i++)
    {
        app_gps_testParseSentence(sentences[i]);
    }
}

static void provided_gnss_and_pairmsg_stream_updates_state(void)
{
    static const char* const sentences[] = {
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

    app_gps_testInit();
    parse_all(sentences, ARRAY_LEN(sentences));

    app_gps_pos_S pos;
    app_gps_heading_S heading;
    app_gps_time_S time;
    app_gps_pairmsg_S pairmsg;
    app_gps_getPos(&pos);
    app_gps_getHeading(&heading);
    app_gps_getTime(&time);
    app_gps_getPairmsg(&pairmsg);

    TEST_ASSERT_TRUE(app_gps_isValid());
    TEST_ASSERT_TRUE(app_gps_getValidTime());
    TEST_ASSERT_TRUE(app_gps_getValidDate());
    TEST_ASSERT_EQUAL_UINT8(15U, app_gps_getNumSatellites());
    TEST_ASSERT_EQUAL_INT(CAN_GPSQUALITYINDICATOR_FIX_3D, app_gps_getQualityCAN());

    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 24.784784f, pos.lat);
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 121.008705f, pos.lon);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 95.2f, pos.alt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, heading.course);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, heading.speedMps);

    TEST_ASSERT_EQUAL_UINT8(22U, time.date);
    TEST_ASSERT_EQUAL_UINT8(6U, time.month);
    TEST_ASSERT_EQUAL_UINT16(17U, time.year);
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
    app_gps_testInit();
    app_gps_testParseSentence("$PAIRMSG,90,072520.000,3*59\r\n");

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
    app_gps_testInit();
    app_gps_testParseSentence("$PAIRMSG,91,072520.000,1,0*46\r\n");

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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(provided_gnss_and_pairmsg_stream_updates_state);
    RUN_TEST(pairmsg_90_updates_dead_reckoning_stage);
    RUN_TEST(pairmsg_91_updates_dynamic_status);
    return UNITY_END();
}
