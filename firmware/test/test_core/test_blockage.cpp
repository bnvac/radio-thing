#include <unity.h>

#include <cstring>

#include "blockage.h"
#include "tests.h"

using namespace rt;

static BlockageFeatures good_profile() {
    BlockageFeatures f;
    f.has_profile = true;
    f.noise_floor = 1.f;
    f.near_energy = 5.f;
    f.far_energy = 10.f;
    f.look_time_ms = 20000;
    return f;
}
#define EQ_STATUS(exp, act) TEST_ASSERT_EQUAL(static_cast<int>(exp), static_cast<int>(act))

static void test_nothing_after_long_look_with_good_penetration() {
    EQ_STATUS(BlockStatus::NOTHING_DETECTED, evaluate_blockage(good_profile()));
}

static void test_short_look_is_low_conf() {
    auto f = good_profile();
    f.look_time_ms = 9999;
    EQ_STATUS(BlockStatus::NOTHING_DETECTED_LOW_CONF, evaluate_blockage(f));
    f.look_time_ms = 10000;
    EQ_STATUS(BlockStatus::NOTHING_DETECTED, evaluate_blockage(f));
}

static void test_marginal_penetration_is_low_conf() {
    auto f = good_profile();
    f.far_energy = 4.f;  // above NO_PENETRATION (3) but below proven (6)
    EQ_STATUS(BlockStatus::NOTHING_DETECTED_LOW_CONF, evaluate_blockage(f));
}

static void test_detection_is_ok() {
    auto f = good_profile();
    f.any_detection = true;
    f.look_time_ms = 0;
    EQ_STATUS(BlockStatus::OK, evaluate_blockage(f));
}

static void test_blocked() {
    auto f = good_profile();
    f.near_energy = 50.f;
    f.far_energy = 1.5f;
    f.any_detection = true;  // blockage wins over a (spurious) detection
    EQ_STATUS(BlockStatus::BLOCKED, evaluate_blockage(f));
}

static void test_no_penetration() {
    auto f = good_profile();
    f.far_energy = 1.2f;
    EQ_STATUS(BlockStatus::NO_PENETRATION, evaluate_blockage(f));
}

static void test_too_close_saturated_has_priority() {
    auto f = good_profile();
    f.saturated = true;
    f.near_energy = 50.f;
    f.far_energy = 0.5f;
    EQ_STATUS(BlockStatus::TOO_CLOSE, evaluate_blockage(f));
}

static void test_hold_still() {
    auto f = good_profile();
    f.imu_motion_mg = 200.f;
    EQ_STATUS(BlockStatus::HOLD_STILL, evaluate_blockage(f));
    BlockageConfig c;
    c.hold_still_mg = 0;  // disabled (SCAN)
    EQ_STATUS(BlockStatus::NOTHING_DETECTED, evaluate_blockage(f, c));
}

static void test_ld2450_never_confident_empty() {
    BlockageFeatures f;  // no profile: only imu + detection
    f.look_time_ms = 600000;  // ten minutes of nothing
    EQ_STATUS(BlockStatus::NOTHING_DETECTED_LOW_CONF, evaluate_blockage(f));
    f.any_detection = true;
    EQ_STATUS(BlockStatus::OK, evaluate_blockage(f));
    f.imu_motion_mg = 500;
    EQ_STATUS(BlockStatus::HOLD_STILL, evaluate_blockage(f));
}

static void test_zero_noise_floor_does_not_divide() {
    auto f = good_profile();
    f.noise_floor = 0.f;  // invalid profile: treated as no profile
    EQ_STATUS(BlockStatus::NOTHING_DETECTED_LOW_CONF, evaluate_blockage(f));
}

static void test_text_never_says_empty() {
    for (int i = 0; i <= static_cast<int>(BlockStatus::NOTHING_DETECTED); i++) {
        const char* s = block_status_text(static_cast<BlockStatus>(i));
        TEST_ASSERT_NULL(strstr(s, "EMPTY"));
        TEST_ASSERT_NULL(strstr(s, "empty"));
    }
}

void run_blockage_tests() {
    RUN_TEST(test_nothing_after_long_look_with_good_penetration);
    RUN_TEST(test_short_look_is_low_conf);
    RUN_TEST(test_marginal_penetration_is_low_conf);
    RUN_TEST(test_detection_is_ok);
    RUN_TEST(test_blocked);
    RUN_TEST(test_no_penetration);
    RUN_TEST(test_too_close_saturated_has_priority);
    RUN_TEST(test_hold_still);
    RUN_TEST(test_ld2450_never_confident_empty);
    RUN_TEST(test_zero_noise_floor_does_not_divide);
    RUN_TEST(test_text_never_says_empty);
}
