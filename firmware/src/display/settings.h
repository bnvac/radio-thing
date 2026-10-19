// Persistent display settings (Preferences / NVS).
#pragma once
#include <Arduino.h>

struct Settings {
    uint8_t channel = 1;      // ESP-NOW Wi-Fi channel 1..13
    uint8_t brightness = 80;  // percent 10..100
    bool feet = false;        // units: false = metres, true = feet
    bool buzzer = true;
    bool nerf = false;        // Nerf mode: SCAN-only, big targets, short range
    uint8_t material = 0;     // wall preset index (see kMaterials in ui.cpp)
    uint8_t range = 1;        // rt::RangeSetting: 0=3 m, 1=6 m, 2=9 m
    uint8_t mode = 0;         // last requested rt::Mode

    void load();
    void save() const;
};
