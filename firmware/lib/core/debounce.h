// Button debouncer with short/long press events. Wrap-safe millisecond arithmetic.
//  - state changes only after the raw input is stable for debounce_ms
//  - SHORT_PRESS is emitted on release if the hold was shorter than long_ms
//  - LONG_PRESS is emitted once, while still held, when the hold reaches long_ms;
//    the following release emits nothing
#pragma once
#include <cstdint>

namespace rt {

enum class ButtonEvent : uint8_t { NONE = 0, SHORT_PRESS, LONG_PRESS };

class Debouncer {
public:
    explicit Debouncer(uint32_t debounce_ms = 25, uint32_t long_ms = 1500)
        : debounce_ms_(debounce_ms), long_ms_(long_ms) {}

    // Call regularly with the raw (already polarity-corrected) pressed state.
    ButtonEvent update(bool raw_pressed, uint32_t now_ms);

    bool pressed() const { return stable_; }

    // Duration of the most recent completed press (valid after SHORT_PRESS).
    uint32_t last_hold_ms() const { return last_hold_ms_; }

private:
    uint32_t debounce_ms_, long_ms_;
    bool raw_ = false, stable_ = false, long_fired_ = false;
    uint32_t raw_since_ = 0, press_start_ = 0, last_hold_ms_ = 0;
};

}  // namespace rt
