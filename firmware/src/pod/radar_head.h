// Radar head abstraction. A new head type = a new RadarHead subclass; main.cpp only
// sees HeadFrame (targets in the pod frame, mm and cm/s) and a few capability queries.
#pragma once
#include <Arduino.h>

#include "board_pins.h"
#include "ld2450.h"
#include "protocol.h"

struct HeadTarget {
    int16_t x_mm = 0;
    int16_t y_mm = 0;
    int16_t speed_cms = 0;
};

struct HeadFrame {
    HeadTarget t[rt::kMaxTargets];
    uint8_t n = 0;
};

class RadarHead {
public:
    virtual ~RadarHead() = default;
    virtual bool begin() = 0;
    // Drain the transport; returns true if a new frame was produced into `out`.
    virtual bool poll(HeadFrame& out) = 0;
    virtual rt::HeadType type() const = 0;
    // True if the head provides a range/energy profile for the blockage checks.
    virtual bool has_range_profile() const = 0;
};

// HLK-LD2450 on UART1 (head connector A), 256000 baud, 3 targets @ ~10 Hz.
class Ld2450Head : public RadarHead {
public:
    bool begin() override {
        Serial1.setRxBufferSize(512);
        Serial1.begin(256000, SERIAL_8N1, PIN_HEADA_RX, PIN_HEADA_TX);
        return true;
    }
    bool poll(HeadFrame& out) override {
        bool got = false;
        while (Serial1.available()) {
            if (parser_.feed(static_cast<uint8_t>(Serial1.read()))) {
                const rt::Ld2450Frame& f = parser_.frame();
                out.n = 0;
                for (const auto& t : f.target) {
                    if (!t.valid) continue;
                    out.t[out.n++] = {t.x_mm, t.y_mm, t.speed_cms};
                }
                got = true;
            }
        }
        return got;
    }
    rt::HeadType type() const override { return rt::HeadType::H24; }
    bool has_range_profile() const override { return false; }  // LD2450 gives targets only
    uint32_t frames_ok() const { return parser_.frames_ok(); }

private:
    rt::Ld2450Parser parser_;
};

// TODO(UWB): driver for the UWB head on UART2 (PIN_HEADB_TX/RX, X4M200 or raw X4/FMCW).
// Needs: begin() (open Serial2, configure respiration profile), poll() decoding breathing
// detections into HeadFrame with TargetKind::BREATHING (range only -> x=0, y=range),
// has_range_profile() = true plus a way to deliver near/far/noise energy to blockage.
// Until then it reports no frames, so a UWB-only pod runs but never detects anything.
class UwbHead : public RadarHead {
public:
    bool begin() override { return false; }
    bool poll(HeadFrame&) override { return false; }
    rt::HeadType type() const override { return rt::HeadType::UWB; }
    bool has_range_profile() const override { return false; }
};
