#include <unity.h>

#include "tests.h"
#include "tracker.h"

using namespace rt;

static const float DT = 0.1f;

static void test_confirmation_3_of_5() {
    Tracker t;
    Detection d{1000, 2000};
    t.update(&d, 1, DT);
    TEST_ASSERT_EQUAL(1, t.active_count());
    TEST_ASSERT_EQUAL(0, t.confirmed_count());
    t.update(&d, 1, DT);
    TEST_ASSERT_EQUAL(0, t.confirmed_count());
    t.update(&d, 1, DT);
    TEST_ASSERT_EQUAL(1, t.confirmed_count());
}

static void test_confirmation_with_gaps() {
    Tracker t;
    Detection d{1000, 2000};
    t.update(&d, 1, DT);   // hit
    t.update(nullptr, 0, DT);  // miss (1 consecutive, tentative allows 2)
    t.update(&d, 1, DT);   // hit
    TEST_ASSERT_EQUAL(0, t.confirmed_count());
    t.update(&d, 1, DT);   // 3 hits in last 4
    TEST_ASSERT_EQUAL(1, t.confirmed_count());
}

static void test_single_blip_never_confirmed_and_dropped() {
    Tracker t;
    Detection d{1000, 2000};
    t.update(&d, 1, DT);
    t.update(nullptr, 0, DT);
    t.update(nullptr, 0, DT);
    TEST_ASSERT_EQUAL(0, t.confirmed_count());
    TEST_ASSERT_EQUAL(0, t.active_count());
}

static void test_deletion_after_misses() {
    TrackerConfig c;
    c.max_misses = 4;
    Tracker t(c);
    Detection d{0, 3000};
    for (int i = 0; i < 4; i++) t.update(&d, 1, DT);
    TEST_ASSERT_EQUAL(1, t.confirmed_count());
    for (int i = 0; i < 3; i++) t.update(nullptr, 0, DT);
    TEST_ASSERT_EQUAL(1, t.confirmed_count());  // still alive (coasting)
    t.update(nullptr, 0, DT);
    TEST_ASSERT_EQUAL(0, t.active_count());
}

static void test_id_stability_moving_target() {
    Tracker t;
    float y = 1000;
    uint8_t id = 0;
    for (int i = 0; i < 30; i++) {
        Detection d{500, y};
        t.update(&d, 1, DT);
        y += 50;  // 0.5 m/s
        if (t.confirmed_count() == 1) {
            Track out[2];
            t.confirmed_tracks(out, 2);
            if (!id) id = out[0].id;
            TEST_ASSERT_EQUAL_UINT8(id, out[0].id);
        }
    }
    TEST_ASSERT_TRUE(id != 0);
    Track out[2];
    t.confirmed_tracks(out, 2);
    TEST_ASSERT_FLOAT_WITHIN(150.f, y - 50, out[0].y_mm);
    TEST_ASSERT_FLOAT_WITHIN(150.f, 500.f, out[0].vy_mmps);  // 500 mm/s learned
}

static void test_two_targets_keep_ids_when_order_swaps() {
    Tracker t;
    for (int i = 0; i < 5; i++) {
        Detection d[2] = {{-1000, 2000}, {1000, 2000}};
        t.update(d, 2, DT);
    }
    TEST_ASSERT_EQUAL(2, t.confirmed_count());
    Track a[2];
    t.confirmed_tracks(a, 2);
    uint8_t left = a[0].x_mm < 0 ? a[0].id : a[1].id;
    uint8_t right = a[0].x_mm < 0 ? a[1].id : a[0].id;
    // swap detection order
    Detection d2[2] = {{1010, 2000}, {-990, 2000}};
    t.update(d2, 2, DT);
    Track b[2];
    t.confirmed_tracks(b, 2);
    for (auto& tr : b) {
        if (tr.x_mm < 0) TEST_ASSERT_EQUAL_UINT8(left, tr.id);
        else TEST_ASSERT_EQUAL_UINT8(right, tr.id);
    }
}

static void test_gating_spawns_new_track() {
    Tracker t;
    Detection d{0, 1000};
    for (int i = 0; i < 4; i++) t.update(&d, 1, DT);
    Detection far{3000, 5000};  // way outside the gate
    t.update(&far, 1, DT);
    TEST_ASSERT_EQUAL(2, t.active_count());
}

static void test_ids_nonzero_and_wrap() {
    Tracker t;
    bool seen[256] = {};
    for (int k = 0; k < 600; k++) {  // many short-lived blips: ids wrap past 255, never 0
        Detection d{static_cast<float>((k % 7) * 1500), 1000};
        t.update(&d, 1, DT);
        for (size_t i = 0; i < Tracker::kMaxTracks; i++) {
            if (t.slot(i).id) seen[t.slot(i).id] = true;
        }
        t.update(nullptr, 0, DT);
        t.update(nullptr, 0, DT);
    }
    TEST_ASSERT_FALSE(seen[0]);
    int distinct = 0;
    for (int i = 1; i < 256; i++) distinct += seen[i];
    TEST_ASSERT_EQUAL(255, distinct);  // wrapped through the whole 1..255 range
    TEST_ASSERT_EQUAL(0, t.active_count());
}

static void test_capacity_limit_and_reset() {
    Tracker t;
    Detection d[12];
    for (int i = 0; i < 12; i++) d[i] = Detection{static_cast<float>(i * 1000 - 5000), 2000};
    t.update(d, 12, DT);
    TEST_ASSERT_EQUAL(Tracker::kMaxTracks, t.active_count());
    t.reset();
    TEST_ASSERT_EQUAL(0, t.active_count());
}

void run_tracker_tests() {
    RUN_TEST(test_confirmation_3_of_5);
    RUN_TEST(test_confirmation_with_gaps);
    RUN_TEST(test_single_blip_never_confirmed_and_dropped);
    RUN_TEST(test_deletion_after_misses);
    RUN_TEST(test_id_stability_moving_target);
    RUN_TEST(test_two_targets_keep_ids_when_order_swaps);
    RUN_TEST(test_gating_spawns_new_track);
    RUN_TEST(test_ids_nonzero_and_wrap);
    RUN_TEST(test_capacity_limit_and_reset);
}
