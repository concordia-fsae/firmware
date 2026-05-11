#include "LIB_app.h"
#include "LIB_app_config.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void validates_application_descriptor_bounds(void)
{
    const lib_app_appDesc_S valid = {
        .appStart = LIB_APP_FLASH_START,
        .appEnd = LIB_APP_FLASH_END - sizeof(lib_app_crc_t),
        .appCrcLocation = LIB_APP_FLASH_END - sizeof(lib_app_crc_t),
        .appComponentId = 10U,
        .appVariantId = 20U,
        .appNodeId = 3U,
    };
    const lib_app_appDesc_S bad_start = {
        .appStart = LIB_APP_FLASH_START - 1U,
        .appEnd = valid.appEnd,
        .appCrcLocation = valid.appCrcLocation,
        .appComponentId = valid.appComponentId,
        .appVariantId = valid.appVariantId,
        .appNodeId = valid.appNodeId,
    };
    const lib_app_appDesc_S bad_end = {
        .appStart = valid.appStart,
        .appEnd = LIB_APP_FLASH_END,
        .appCrcLocation = valid.appCrcLocation,
        .appComponentId = valid.appComponentId,
        .appVariantId = valid.appVariantId,
        .appNodeId = valid.appNodeId,
    };
    const lib_app_appDesc_S bad_crc = {
        .appStart = valid.appStart,
        .appEnd = valid.appEnd,
        .appCrcLocation = LIB_APP_FLASH_START - 1U,
        .appComponentId = valid.appComponentId,
        .appVariantId = valid.appVariantId,
        .appNodeId = valid.appNodeId,
    };

    TEST_ASSERT_TRUE(lib_app_validateAppDesc(&valid, APPDESC_VALID_START));
    TEST_ASSERT_TRUE(lib_app_validateAppDesc(&valid, APPDESC_VALID_END));
    TEST_ASSERT_TRUE(lib_app_validateAppDesc(&valid, APPDESC_VALID_CRCLOCATION));
    TEST_ASSERT_FALSE(lib_app_validateAppDesc(&bad_start, APPDESC_VALID_START));
    TEST_ASSERT_FALSE(lib_app_validateAppDesc(&bad_end, APPDESC_VALID_END));
    TEST_ASSERT_FALSE(lib_app_validateAppDesc(&bad_crc, APPDESC_VALID_CRCLOCATION));
    TEST_ASSERT_FALSE(lib_app_validateAppDesc(&valid, APPDESC_VALID_COUNT));
}

static void validates_application_identity_fields(void)
{
    const lib_app_appDesc_S hw = {
        .appStart = LIB_APP_FLASH_START,
        .appEnd = LIB_APP_FLASH_END - sizeof(lib_app_crc_t),
        .appCrcLocation = LIB_APP_FLASH_END - sizeof(lib_app_crc_t),
        .appComponentId = 10U,
        .appVariantId = 20U,
        .appNodeId = 3U,
    };
    const lib_app_appDesc_S app = {
        .appStart = LIB_APP_FLASH_START,
        .appEnd = LIB_APP_FLASH_END - sizeof(lib_app_crc_t),
        .appCrcLocation = LIB_APP_FLASH_END - sizeof(lib_app_crc_t),
        .appComponentId = 10U,
        .appVariantId = 20U,
        .appNodeId = 3U,
    };
    const lib_app_appDesc_S mismatch = {
        .appStart = app.appStart,
        .appEnd = app.appEnd,
        .appCrcLocation = app.appCrcLocation,
        .appComponentId = 11U,
        .appVariantId = 21U,
        .appNodeId = 4U,
    };

    TEST_ASSERT_EQUAL_UINT32(8U, FDEF_TO_DID_RESPONSE(10U));
    TEST_ASSERT_TRUE(lib_app_validateApp(&hw, &app, APP_VALID_VARIANT_ID));
    TEST_ASSERT_TRUE(lib_app_validateApp(&hw, &app, APP_VALID_COMPONENT_ID));
    TEST_ASSERT_TRUE(lib_app_validateApp(&hw, &app, APP_VALID_NODE_ID));
    TEST_ASSERT_FALSE(lib_app_validateApp(&hw, &mismatch, APP_VALID_VARIANT_ID));
    TEST_ASSERT_FALSE(lib_app_validateApp(&hw, &mismatch, APP_VALID_COMPONENT_ID));
    TEST_ASSERT_FALSE(lib_app_validateApp(&hw, &mismatch, APP_VALID_NODE_ID));
    TEST_ASSERT_FALSE(lib_app_validateApp(&hw, &app, APP_VALID_COUNT));
}

static void exposes_newlib_init_hook(void)
{
    _init();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(validates_application_descriptor_bounds);
    RUN_TEST(validates_application_identity_fields);
    RUN_TEST(exposes_newlib_init_hook);
    return UNITY_END();
}
