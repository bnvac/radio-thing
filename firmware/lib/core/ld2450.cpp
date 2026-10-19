#include "ld2450.h"

namespace rt {

namespace {
const uint8_t kHeader[4] = {0xAA, 0xFF, 0x03, 0x00};
}

int16_t ld2450_decode_signed(uint8_t lo, uint8_t hi) {
    int16_t mag = static_cast<int16_t>(((hi & 0x7F) << 8) | lo);
    return (hi & 0x80) ? mag : static_cast<int16_t>(-mag);
}

void ld2450_decode_frame(const uint8_t* f, Ld2450Frame& out) {
    out.valid_count = 0;
    for (size_t i = 0; i < kLd2450MaxTargets; i++) {
        const uint8_t* p = f + 4 + i * 8;
        Ld2450Target& t = out.target[i];
        t.x_mm = ld2450_decode_signed(p[0], p[1]);
        t.y_mm = ld2450_decode_signed(p[2], p[3]);
        t.speed_cms = ld2450_decode_signed(p[4], p[5]);
        t.resolution_mm = static_cast<uint16_t>(p[6] | (p[7] << 8));
        t.valid = !(t.x_mm == 0 && t.y_mm == 0);
        if (t.valid) out.valid_count++;
    }
}

bool Ld2450Parser::feed(uint8_t byte) {
    buf_[len_++] = byte;
    bool got = false;
    // Drop leading bytes until the buffer is a prefix of a valid frame.
    for (;;) {
        size_t chk = len_ < 4 ? len_ : 4;
        size_t i = 0;
        while (i < chk && buf_[i] == kHeader[i]) i++;
        if (i < chk) {  // header mismatch: shift by one and re-check
            for (size_t k = 1; k < len_; k++) buf_[k - 1] = buf_[k];
            len_--;
            if (len_ == 0) break;
            continue;
        }
        if (len_ == kLd2450FrameLen) {
            if (buf_[28] == 0x55 && buf_[29] == 0xCC) {
                ld2450_decode_frame(buf_, frame_);
                frames_ok_++;
                len_ = 0;
                got = true;
            } else {  // bad tail: drop first byte, rescan the rest
                frames_bad_++;
                for (size_t k = 1; k < len_; k++) buf_[k - 1] = buf_[k];
                len_--;
                continue;
            }
        }
        break;
    }
    return got;
}

size_t Ld2450Parser::feed(const uint8_t* data, size_t len) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++)
        if (feed(data[i])) n++;
    return n;
}

}  // namespace rt
