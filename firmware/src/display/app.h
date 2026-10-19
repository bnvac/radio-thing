// Hooks the UI calls back into main.cpp for hardware side effects.
#pragma once
#include "protocol.h"

void app_send_command(const rt::Command& c);
void app_apply_brightness(uint8_t pct);
void app_set_channel(uint8_t ch);  // broadcast to pods, then switch own radio
void app_chirp(bool force);        // short buzzer beep (respects mute unless forced)
float app_battery_pct();
float app_battery_volts();
