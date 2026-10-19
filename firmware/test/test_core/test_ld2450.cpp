#include <unity.h>

#include <cstring>

#include "ld2450.h"
#include "tests.h"

using namespace rt;

namespace {
// Build a frame from three raw 8-byte targets (all zeros if null).
void build(uint8_t* f, const uint8_t (*t)[8], int n) {
    const uint8_t hdr[4] = {0xAA, 0xFF, 0x03, 0x00};
    std::memcpy(f, hdr, 4);
    std::memset(f + 4, 0, 24);
    for (int i = 0; i < n; i++) std::memcpy(f + 4 + i * 8, t[i], 8);
    f[28] = 0x55;
    f[29] = 0xCC;
}
// x=+500 mm (0x81F4), y=+1500 (0x85DC), speed=-16 cm/s (0x0010), res=360
const uint8_t kT0[8] = {0xF4, 0x81, 0xDC, 0x85, 0x10, 0x00, 0x68, 0x01};
// x=-200 (0x00C8), y=+2000 (0x87D0), speed=+5 (0x8005), res=360
const uint8_t kT1[8] = {0xC8, 0x00, 0xD0, 0x87, 0x05, 0x80, 0x68, 0x01};
}  // namespace

static void test_sign_magnitude() {
    TEST_ASSERT_EQUAL_INT16(500, ld2450_decode_signed(0xF4, 0x81));
    TEST_ASSERT_EQUAL_INT16(-200, ld2450_decode_signed(0xC8, 0x00));
    TEST_ASSERT_EQUAL_INT16(0, ld2450_decode_signed(0x00, 0x00));
    TEST_ASSERT_EQUAL_INT16(0, ld2450_decode_signed(0x00, 0x80));
    TEST_ASSERT_EQUAL_INT16(32767, ld2450_decode_signed(0xFF, 0xFF));
    TEST_ASSERT_EQUAL_INT16(-32767, ld2450_decode_signed(0xFF, 0x7F));
    TEST_ASSERT_EQUAL_INT16(-1, ld2450_decode_signed(0x01, 0x00));
}

static void test_valid_frame() {
    uint8_t f[30];
    const uint8_t tg[2][8] = {{0xF4, 0x81, 0xDC, 0x85, 0x10, 0x00, 0x68, 0x01},
                              {0xC8, 0x00, 0xD0, 0x87, 0x05, 0x80, 0x68, 0x01}};
    build(f, tg, 2);
    Ld2450Parser p;
    size_t n = p.feed(f, 30);
    TEST_ASSERT_EQUAL(1, n);
    const Ld2450Frame& fr = p.frame();
    TEST_ASSERT_EQUAL_UINT8(2, fr.valid_count);
    TEST_ASSERT_TRUE(fr.target[0].valid);
    TEST_ASSERT_EQUAL_INT16(500, fr.target[0].x_mm);
    TEST_ASSERT_EQUAL_INT16(1500, fr.target[0].y_mm);
    TEST_ASSERT_EQUAL_INT16(-16, fr.target[0].speed_cms);
    TEST_ASSERT_EQUAL_UINT16(360, fr.target[0].resolution_mm);
    TEST_ASSERT_EQUAL_INT16(-200, fr.target[1].x_mm);
    TEST_ASSERT_EQUAL_INT16(2000, fr.target[1].y_mm);
    TEST_ASSERT_EQUAL_INT16(5, fr.target[1].speed_cms);
    TEST_ASSERT_FALSE(fr.target[2].valid);
}

static void test_empty_frame() {
    uint8_t f[30];
    build(f, nullptr, 0);
    Ld2450Parser p;
    TEST_ASSERT_EQUAL(1, p.feed(f, 30));
    TEST_ASSERT_EQUAL_UINT8(0, p.frame().valid_count);
}

static void test_resync_after_garbage() {
    uint8_t f[30];
    const uint8_t tg[1][8] = {{0xF4, 0x81, 0xDC, 0x85, 0x10, 0x00, 0x68, 0x01}};
    build(f, tg, 1);
    uint8_t stream[64];
    size_t n = 0;
    const uint8_t junk[] = {0x12, 0xAA, 0xAA, 0xFF, 0x00, 0x55};  // includes a false header start
    std::memcpy(stream, junk, sizeof junk);
    n += sizeof junk;
    std::memcpy(stream + n, f, 30);
    n += 30;
    Ld2450Parser p;
    TEST_ASSERT_EQUAL(1, p.feed(stream, n));
    TEST_ASSERT_EQUAL_INT16(500, p.frame().target[0].x_mm);
}

static void test_bad_tail_rejected_and_recovers() {
    uint8_t bad[30], good[30];
    const uint8_t tg[1][8] = {{0xF4, 0x81, 0xDC, 0x85, 0x10, 0x00, 0x68, 0x01}};
    build(bad, tg, 1);
    bad[29] = 0x00;  // corrupt tail
    build(good, tg, 1);
    Ld2450Parser p;
    TEST_ASSERT_EQUAL(0, p.feed(bad, 30));
    TEST_ASSERT_EQUAL_UINT32(0, p.frames_ok());
    TEST_ASSERT_TRUE(p.frames_bad() > 0);
    TEST_ASSERT_EQUAL(1, p.feed(good, 30));
}

static void test_back_to_back_and_split_feed() {
    uint8_t a[30], b[30];
    const uint8_t t1[1][8] = {{0xF4, 0x81, 0xDC, 0x85, 0x10, 0x00, 0x68, 0x01}};
    const uint8_t t2[1][8] = {{0xC8, 0x00, 0xD0, 0x87, 0x05, 0x80, 0x68, 0x01}};
    build(a, t1, 1);
    build(b, t2, 1);
    Ld2450Parser p;
    size_t n = 0;
    for (int i = 0; i < 30; i++) n += p.feed(a[i]) ? 1 : 0;
    for (int i = 0; i < 30; i++) n += p.feed(b[i]) ? 1 : 0;
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_INT16(-200, p.frame().target[0].x_mm);
}

static void test_truncated_frame_yields_nothing() {
    uint8_t f[30];
    build(f, nullptr, 0);
    Ld2450Parser p;
    TEST_ASSERT_EQUAL(0, p.feed(f, 29));
}

void run_ld2450_tests() {
    RUN_TEST(test_sign_magnitude);
    RUN_TEST(test_valid_frame);
    RUN_TEST(test_empty_frame);
    RUN_TEST(test_resync_after_garbage);
    RUN_TEST(test_bad_tail_rejected_and_recovers);
    RUN_TEST(test_back_to_back_and_split_feed);
    RUN_TEST(test_truncated_frame_yields_nothing);
}
