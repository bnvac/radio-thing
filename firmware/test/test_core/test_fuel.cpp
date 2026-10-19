#include <unity.h>

#include "fuel.h"
#include "tests.h"

using namespace rt;

static void test_soc() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.f, fuel_soc_percent(0, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.5f, fuel_soc_percent(50, 128));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 87.f + 255.f / 256.f, fuel_soc_percent(87, 255));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.f, fuel_soc_percent(100, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.f, fuel_soc_percent(200, 77));  // clamped
}

static void test_vcell() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.f, fuel_vcell_volts(0, 0));
    // 0xC000 = 49152 LSB * 78.125 uV = 3.84 V
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.84f, fuel_vcell_volts(0xC0, 0x00));
    // 4.2 V -> 53760 = 0xD200
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.2f, fuel_vcell_volts(0xD2, 0x00));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 78.125e-6f, fuel_vcell_volts(0x00, 0x01));
}

static void test_percent_u8() {
    TEST_ASSERT_EQUAL_UINT8(0, fuel_percent_u8(-3.f));
    TEST_ASSERT_EQUAL_UINT8(51, fuel_percent_u8(50.5f));
    TEST_ASSERT_EQUAL_UINT8(100, fuel_percent_u8(99.6f));
    TEST_ASSERT_EQUAL_UINT8(100, fuel_percent_u8(140.f));
}

void run_fuel_tests() {
    RUN_TEST(test_soc);
    RUN_TEST(test_vcell);
    RUN_TEST(test_percent_u8);
}
