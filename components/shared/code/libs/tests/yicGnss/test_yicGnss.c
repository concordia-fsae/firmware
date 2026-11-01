#include "lib_yicGnss.h"
#include "unity.h"
#include <string.h>

static lib_yicGnss_S receiver;

void                        setUp(void)    { lib_yicGnss_init(&receiver); }
void                        tearDown(void) {}

static lib_yicGnss_result_S feedText(lib_yicGnss_S* context, const char* text)
{
    lib_yicGnss_result_S result = { LIB_YICGNSS_RESULT_NONE, LIB_YICGNSS_SENTENCE_UNKNOWN };

    while (*text != '\0')
    {
        result = lib_yicGnss_feed(context, (uint8_t)*text++);
    }
    return result;
}

static void caller_owned_instances_do_not_share_partial_frames_or_data(void)
{
    lib_yicGnss_S other;

    lib_yicGnss_init(&other);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_NONE, feedText(&receiver, "$PAIRMSG,90,072").status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_ACCEPTED,
                          feedText(&other, "$PAIRMSG,91,072520.000,1,0*46\r\n").status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_ACCEPTED,
                          feedText(&receiver, "520.000,3*59\r\n").status);
    TEST_ASSERT_EQUAL_UINT8(3U, receiver.data.pairmsg.drStage);
    TEST_ASSERT_EQUAL_UINT8(0U, receiver.data.pairmsg.dynamicStatus);
    TEST_ASSERT_EQUAL_UINT8(0U, other.data.pairmsg.drStage);
    TEST_ASSERT_EQUAL_UINT8(1U, other.data.pairmsg.dynamicStatus);
}

static void malformed_navigation_identifies_source_without_mutating_last_data(void)
{
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_ACCEPTED,
                          feedText(&receiver, "$GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000*72\r\n").status);
    const lib_yicGnss_data_S   previous = receiver.data;
    // Valid XOR checksum, incomplete GGA fields.
    const lib_yicGnss_result_S result   = feedText(&receiver, "$GNGGA*48\n");
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_INVALID, result.status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_SENTENCE_GGA,   result.sentence);
    TEST_ASSERT_EQUAL_MEMORY(&previous, &receiver.data, sizeof(previous));
}

static void framing_errors_have_no_navigation_source_and_resynchronize(void)
{
    feedText(&receiver, "$truncated");
    lib_yicGnss_result_S result = lib_yicGnss_feed(&receiver, '$');
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_INVALID,   result.status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_SENTENCE_UNKNOWN, result.sentence);
    result = feedText(&receiver, "PAIRMSG,90,072520.000,3*59\r\n");
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_ACCEPTED,  result.status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_SENTENCE_PAIRMSG, result.sentence);
    TEST_ASSERT_EQUAL_UINT8(3U, receiver.data.pairmsg.drStage);
}

static void reset_stream_discards_only_assembly_and_init_clears_all_data(void)
{
    feedText(&receiver, "$PAIRMSG,91,072520.000,1,0*46\r\n");
    feedText(&receiver, "$PAIRMSG,90,072");
    lib_yicGnss_resetStream(&receiver);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_NONE, feedText(&receiver, "520.000,3*59\r\n").status);
    TEST_ASSERT_EQUAL_UINT8(1U, receiver.data.pairmsg.dynamicStatus);
    TEST_ASSERT_EQUAL_UINT8(0U, receiver.data.pairmsg.drStage);
    feedText(&receiver, "$PAIRMSG,90,072520.000,3*59\n");
    TEST_ASSERT_EQUAL_UINT8(3U, receiver.data.pairmsg.drStage);
    lib_yicGnss_init(&receiver);
    TEST_ASSERT_EQUAL_UINT8(0U, receiver.data.pairmsg.dynamicStatus);
    TEST_ASSERT_EQUAL_UINT8(0U, receiver.data.pairmsg.drStage);
    TEST_ASSERT_FALSE(receiver.data.rmcValid);
}

static void checksum_policy_is_applied_before_any_decoded_update(void)
{
    const lib_yicGnss_result_S result = feedText(&receiver, "$PAIRMSG,90,072520.000,3*00\r\n");

#if LWGPS_CFG_CRC
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_CHECKSUM_ERROR, result.status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_SENTENCE_UNKNOWN,      result.sentence);
    TEST_ASSERT_EQUAL_UINT8(0U, receiver.data.pairmsg.drStage);
#else
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_RESULT_ACCEPTED,  result.status);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_SENTENCE_PAIRMSG, result.sentence);
    TEST_ASSERT_EQUAL_UINT8(3U, receiver.data.pairmsg.drStage);
#endif
}

static void decoder_reports_fields_without_application_fix_policy(void)
{
    feedText(&receiver, "$GNGGA,175258.000,2447.0870,N,12100.5221,E,2,15,0.7,95.2,M,19.6,M,,0000*72\n");
    feedText(&receiver, "$GNRMC,175258.000,A,2447.0870,N,12100.5220,E,000.0,000.0,220617,,,D*74\n");
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_METHOD_DIFFERENTIAL, receiver.data.method);
    // No GSA has arrived. Raw RMC/time/date fields remain decoded independently.
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_DIMENSION_UNKNOWN,   receiver.data.dimension);
    TEST_ASSERT_TRUE(receiver.data.rmcValid);
    TEST_ASSERT_TRUE(receiver.data.rmcModeValid);
    TEST_ASSERT_TRUE(receiver.data.timeValid);
    TEST_ASSERT_TRUE(receiver.data.dateValid);
    TEST_ASSERT_EQUAL_UINT16(2017U, receiver.data.year);
    TEST_ASSERT_EQUAL_UINT32(64378000U, receiver.data.ggaUtcMs);
    TEST_ASSERT_EQUAL_UINT32(64378000U, receiver.data.rmcUtcMs);
    TEST_ASSERT_FLOAT_WITHIN(0.00002f, 24.784784f, receiver.data.latitude);
    feedText(&receiver, "$GNGGA,175258.000,2447.0870,N,12100.5221,E,6,15,0.7,95.2,M,19.6,M,,0000*76\n");
    TEST_ASSERT_EQUAL_INT(6U,                            receiver.data.method);
    TEST_ASSERT_EQUAL_INT(LIB_YICGNSS_DIMENSION_UNKNOWN, receiver.data.dimension);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(caller_owned_instances_do_not_share_partial_frames_or_data);
    RUN_TEST(malformed_navigation_identifies_source_without_mutating_last_data);
    RUN_TEST(framing_errors_have_no_navigation_source_and_resynchronize);
    RUN_TEST(reset_stream_discards_only_assembly_and_init_clears_all_data);
    RUN_TEST(checksum_policy_is_applied_before_any_decoded_update);
    RUN_TEST(decoder_reports_fields_without_application_fix_policy);
    return UNITY_END();
}
