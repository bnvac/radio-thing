// HLK-LD2450 target-frame stream parser (pure C++, no Arduino).
//
// Frame (30 bytes): AA FF 03 00 | 3 x 8-byte targets | 55 CC
// Target: x:int16 y:int16 speed:int16 resolution:uint16, all little-endian.
// x, y, speed use a sign-magnitude form: bit15 SET = positive, CLEAR = negative,
// bits 0..14 = magnitude. x/y in mm, speed in cm/s. An unused slot is all zeros.
//
// Encoding verified against the ESPHome ld2450 component
// (esphome/components/ld2450/ld2450.cpp: decode_coordinate/decode_speed, where
// "(high_byte & 0x80) == 0 -> negate" and speed*10 converts cm/s to mm/s) and
// DATA_FRAME_HEADER {AA FF 03 00} / DATA_FRAME_FOOTER {55 CC}.
// Axes: +y = straight out of the sensor face, +x = to the sensor's right.
#pragma once
#include <cstddef>
#include <cstdint>

namespace rt {

constexpr size_t kLd2450FrameLen = 30;
constexpr size_t kLd2450MaxTargets = 3;

struct Ld2450Target {
    int16_t x_mm = 0;
    int16_t y_mm = 0;
    int16_t speed_cms = 0;
    uint16_t resolution_mm = 0;
    bool valid = false;  // false for an empty slot (x == 0 && y == 0)
};

struct Ld2450Frame {
    Ld2450Target target[kLd2450MaxTargets];
    uint8_t valid_count = 0;
};

// Decode one sign-magnitude int16 (bit15 set = positive).
int16_t ld2450_decode_signed(uint8_t lo, uint8_t hi);

// Decode a complete, already-validated 30-byte frame.
void ld2450_decode_frame(const uint8_t* frame30, Ld2450Frame& out);

class Ld2450Parser {
public:
    // Feed one byte; returns true when a complete valid frame is available via frame().
    bool feed(uint8_t byte);
    // Feed a buffer; returns number of frames completed (frame() holds the last one).
    size_t feed(const uint8_t* data, size_t len);

    const Ld2450Frame& frame() const { return frame_; }
    uint32_t frames_ok() const { return frames_ok_; }
    uint32_t frames_bad() const { return frames_bad_; }  // bad tail / resyncs

private:
    uint8_t buf_[kLd2450FrameLen] = {};
    size_t len_ = 0;
    Ld2450Frame frame_;
    uint32_t frames_ok_ = 0;
    uint32_t frames_bad_ = 0;
};

}  // namespace rt
