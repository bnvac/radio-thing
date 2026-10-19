#include <unity.h>

#include "head_id.h"
#include "tests.h"

using namespace rt;
#define EQ_HEAD(exp, act) TEST_ASSERT_EQUAL(static_cast<int>(exp), static_cast<int>(act))

static void test_nominal_voltages() {
    EQ_HEAD(HeadType::NONE, head_from_mv(3300));
    EQ_HEAD(HeadType::H24, head_from_mv(1050));
    EQ_HEAD(HeadType::UWB, head_from_mv(1650));
    EQ_HEAD(HeadType::DUAL, head_from_mv(2270));
}

static void test_thresholds_are_midpoints() {
    TEST_ASSERT_EQUAL(1350, kHeadThr24UwbMv);
    TEST_ASSERT_EQUAL(1960, kHeadThrUwbDualMv);
    TEST_ASSERT_EQUAL(2785, kHeadThrDualNoneMv);
    EQ_HEAD(HeadType::H24, head_from_mv(1349));
    EQ_HEAD(HeadType::UWB, head_from_mv(1350));
    EQ_HEAD(HeadType::UWB, head_from_mv(1959));
    EQ_HEAD(HeadType::DUAL, head_from_mv(1960));
    EQ_HEAD(HeadType::DUAL, head_from_mv(2784));
    EQ_HEAD(HeadType::NONE, head_from_mv(2785));
}

static void test_out_of_range() {
    EQ_HEAD(HeadType::NONE, head_from_mv(0));
    EQ_HEAD(HeadType::NONE, head_from_mv(-50));
    EQ_HEAD(HeadType::NONE, head_from_mv(299));
    EQ_HEAD(HeadType::H24, head_from_mv(300));
    EQ_HEAD(HeadType::NONE, head_from_mv(5000));
}

static void test_names() {
    TEST_ASSERT_EQUAL_STRING("24GHz", head_name(HeadType::H24));
    TEST_ASSERT_EQUAL_STRING("UWB", head_name(HeadType::UWB));
}

void run_head_id_tests() {
    RUN_TEST(test_nominal_voltages);
    RUN_TEST(test_thresholds_are_midpoints);
    RUN_TEST(test_out_of_range);
    RUN_TEST(test_names);
}
