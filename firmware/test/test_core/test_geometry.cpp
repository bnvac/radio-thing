#include <unity.h>

#include "geometry.h"
#include "tests.h"

using namespace rt;

static void test_range_settings() {
    TEST_ASSERT_EQUAL_FLOAT(3000.f, range_mm(RangeSetting::R3M));
    TEST_ASSERT_EQUAL_FLOAT(6000.f, range_mm(RangeSetting::R6M));
    TEST_ASSERT_EQUAL_FLOAT(9000.f, range_mm(RangeSetting::R9M));
    TEST_ASSERT_EQUAL(static_cast<int>(RangeSetting::R9M), static_cast<int>(next_range(RangeSetting::R6M, true)));
    TEST_ASSERT_EQUAL(static_cast<int>(RangeSetting::R9M), static_cast<int>(next_range(RangeSetting::R9M, true)));
    TEST_ASSERT_EQUAL(static_cast<int>(RangeSetting::R3M), static_cast<int>(next_range(RangeSetting::R3M, false)));
}

static void test_fan_origin_and_axes() {
    MapView v = make_fan_view(0, 20, 480, 260, 6000.f);
    ScreenPt o = map_to_screen(v, 0, 0);
    TEST_ASSERT_EQUAL(240, o.x);
    TEST_ASSERT_EQUAL(280, o.y);  // bottom of rect
    ScreenPt top = map_to_screen(v, 0, 6000.f);
    TEST_ASSERT_EQUAL(240, top.x);
    TEST_ASSERT_INT_WITHIN(1, 20, top.y);
    ScreenPt r = map_to_screen(v, 1000.f, 1000.f);
    TEST_ASSERT_TRUE(r.x > 240);   // +x is right
    TEST_ASSERT_TRUE(r.y < 280);   // +y is up
    TEST_ASSERT_TRUE(r.inside);
    ScreenPt l = map_to_screen(v, -1000.f, 1000.f);
    TEST_ASSERT_EQUAL(480 - r.x, l.x);
}

static void test_scale_changes_with_range() {
    MapView a = make_fan_view(0, 20, 480, 260, 3000.f);
    MapView b = make_fan_view(0, 20, 480, 260, 9000.f);
    TEST_ASSERT_TRUE(a.px_per_mm > b.px_per_mm * 2.9f);
    ScreenPt pa = map_to_screen(a, 0, 3000.f);
    ScreenPt pb = map_to_screen(b, 0, 3000.f);
    TEST_ASSERT_TRUE(pa.y < pb.y);
}

static void test_clipping() {
    MapView v = make_fan_view(0, 20, 480, 260, 3000.f);
    TEST_ASSERT_FALSE(map_to_screen(v, 0, 3500.f).inside);
    TEST_ASSERT_FALSE(map_to_screen(v, 0, -500.f).inside);
    TEST_ASSERT_FALSE(map_to_screen(v, 9000.f, 100.f).inside);
}

static void test_narrow_rect_fits_full_fan() {
    MapView v = make_fan_view(0, 0, 200, 300, 6000.f);  // too narrow for height-limited scale
    ScreenPt p = map_to_screen(v, 6000.f * 0.8660254f, 6000.f * 0.5f);  // +-60 deg edge at max range
    TEST_ASSERT_TRUE(p.x <= 200);
    TEST_ASSERT_TRUE(p.x >= 195);
}

static void test_pod_placement() {
    float wx, wy;
    PodPlacement p{1000.f, 2000.f, 0.f};
    pod_to_world(p, 300.f, 1500.f, wx, wy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1300.f, wx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3500.f, wy);
    // pod facing east (90 deg cw from north): forward -> +x world, right -> -y world
    PodPlacement e{0.f, 0.f, 90.f};
    pod_to_world(e, 0.f, 1000.f, wx, wy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.f, wx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.f, wy);
    pod_to_world(e, 500.f, 0.f, wx, wy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.f, wx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -500.f, wy);
    // facing south
    PodPlacement s{0.f, 0.f, 180.f};
    pod_to_world(s, 0.f, 1000.f, wx, wy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1000.f, wy);
}

static void test_world_view() {
    MapView v = make_world_view(0, 0, 400, 200, -2000.f, 0.f, 2000.f, 2000.f);
    ScreenPt c = map_to_screen(v, 0.f, 1000.f);
    TEST_ASSERT_EQUAL(200, c.x);
    TEST_ASSERT_EQUAL(100, c.y);
    TEST_ASSERT_TRUE(map_to_screen(v, -2000.f, 0.f).inside || map_to_screen(v, -1990.f, 10.f).inside);
}

void run_geometry_tests() {
    RUN_TEST(test_range_settings);
    RUN_TEST(test_fan_origin_and_axes);
    RUN_TEST(test_scale_changes_with_range);
    RUN_TEST(test_clipping);
    RUN_TEST(test_narrow_rect_fits_full_fan);
    RUN_TEST(test_pod_placement);
    RUN_TEST(test_world_view);
}
