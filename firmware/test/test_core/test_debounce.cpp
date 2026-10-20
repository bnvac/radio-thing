#include <unity.h>

#include "debounce.h"
#include "tests.h"

using namespace rt;
#define EQ_EV(exp, act) TEST_ASSERT_EQUAL(static_cast<int>(exp), static_cast<int>(act))

// Drive `pressed` for [from, to) ms in 1 ms steps; returns the first non-NONE event (and its time).
static ButtonEvent run(Debouncer& d, bool pressed, uint32_t from, uint32_t to, uint32_t* at = nullptr) {
    ButtonEvent first = ButtonEvent::NONE;
    for (uint32_t t = from; t < to; t++) {
        ButtonEvent e = d.update(pressed, t);
        if (e != ButtonEvent::NONE && first == ButtonEvent::NONE) {
            first = e;
            if (at) *at = t;
        }
    }
    return first;
}

static void test_short_press() {
    Debouncer d(25, 1500);
    EQ_EV(ButtonEvent::NONE, run(d, true, 0, 300));
    TEST_ASSERT_TRUE(d.pressed());
    uint32_t at = 0;
    EQ_EV(ButtonEvent::SHORT_PRESS, run(d, false, 300, 400, &at));
    TEST_ASSERT_TRUE(at >= 325);  // needs 25 ms of stable release
    TEST_ASSERT_FALSE(d.pressed());
}

static void test_bounce_is_ignored() {
    Debouncer d(25, 1500);
    uint32_t t = 0;
    // 10 ms of chatter then stable low
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(0, static_cast<int>(d.update(i % 2 == 0, t++)));
    }
    TEST_ASSERT_FALSE(d.pressed());
    // glitch shorter than debounce never registers
    EQ_EV(ButtonEvent::NONE, run(d, true, 100, 110));
    EQ_EV(ButtonEvent::NONE, run(d, false, 110, 300));
    TEST_ASSERT_FALSE(d.pressed());
}

static void test_debounce_boundary() {
    Debouncer d(25, 1500);
    d.update(true, 1000);
    d.update(true, 1024);
    TEST_ASSERT_FALSE(d.pressed());
    d.update(true, 1025);
    TEST_ASSERT_TRUE(d.pressed());
}

static void test_long_press_fires_once_while_held() {
    Debouncer d(25, 1500);
    uint32_t at = 0;
    EQ_EV(ButtonEvent::LONG_PRESS, run(d, true, 0, 3000, &at));
    TEST_ASSERT_TRUE(at >= 1500 - 25 && at <= 1530);
    // no more events while held, and release after long emits nothing
    EQ_EV(ButtonEvent::NONE, run(d, true, 3000, 4000));
    EQ_EV(ButtonEvent::NONE, run(d, false, 4000, 4200));
}

static void test_just_under_long_is_short() {
    Debouncer d(25, 1500);
    EQ_EV(ButtonEvent::NONE, run(d, true, 0, 1400));
    EQ_EV(ButtonEvent::SHORT_PRESS, run(d, false, 1400, 1500));
}

static void test_millis_wraparound() {
    Debouncer d(25, 1500);
    uint32_t start = 0xFFFFFF00u;
    ButtonEvent e = ButtonEvent::NONE;
    for (uint32_t i = 0; i < 2000; i++) {
        ButtonEvent x = d.update(true, start + i);
        if (x != ButtonEvent::NONE) e = x;
    }
    EQ_EV(ButtonEvent::LONG_PRESS, e);
}

static void test_two_presses() {
    Debouncer d(25, 1500);
    run(d, true, 0, 100);
    EQ_EV(ButtonEvent::SHORT_PRESS, run(d, false, 100, 200));
    run(d, true, 200, 300);
    EQ_EV(ButtonEvent::SHORT_PRESS, run(d, false, 300, 400));
}

// OK button: release between 1.5 s and 3 s is still a SHORT_PRESS, and the hold time
// tells the caller to treat it as BACK.
static void test_last_hold_ms_reports_press_length() {
    Debouncer d(25, 3000);
    run(d, true, 0, 2000);
    EQ_EV(ButtonEvent::SHORT_PRESS, run(d, false, 2000, 2100));
    TEST_ASSERT_UINT32_WITHIN(2, 2000, d.last_hold_ms());
    run(d, true, 3000, 3200);
    EQ_EV(ButtonEvent::SHORT_PRESS, run(d, false, 3200, 3300));
    TEST_ASSERT_UINT32_WITHIN(2, 200, d.last_hold_ms());
}

void run_debounce_tests() {
    RUN_TEST(test_last_hold_ms_reports_press_length);
    RUN_TEST(test_short_press);
    RUN_TEST(test_bounce_is_ignored);
    RUN_TEST(test_debounce_boundary);
    RUN_TEST(test_long_press_fires_once_while_held);
    RUN_TEST(test_just_under_long_is_short);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_two_presses);
}
