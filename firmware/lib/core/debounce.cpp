#include "debounce.h"

namespace rt {

ButtonEvent Debouncer::update(bool raw_pressed, uint32_t now) {
    if (raw_pressed != raw_) {
        raw_ = raw_pressed;
        raw_since_ = now;
    }
    ButtonEvent ev = ButtonEvent::NONE;
    if (raw_ != stable_ && static_cast<uint32_t>(now - raw_since_) >= debounce_ms_) {
        stable_ = raw_;
        if (stable_) {
            press_start_ = raw_since_;
            long_fired_ = false;
        } else {
            last_hold_ms_ = static_cast<uint32_t>(raw_since_ - press_start_);
            if (!long_fired_) ev = ButtonEvent::SHORT_PRESS;
        }
    }
    if (stable_ && !long_fired_ && static_cast<uint32_t>(now - press_start_) >= long_ms_) {
        long_fired_ = true;
        ev = ButtonEvent::LONG_PRESS;
    }
    return ev;
}

}  // namespace rt
