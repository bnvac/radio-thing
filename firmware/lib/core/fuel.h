// MAX17048 fuel-gauge register decoding (I2C 0x36).
//  VCELL 0x02: 16-bit, 78.125 uV/LSB.   SOC 0x04: MSB = 1 %, LSB = 1/256 %.
#pragma once
#include <cstdint>

namespace rt {

constexpr uint8_t kMax17048Addr = 0x36;
constexpr uint8_t kMax17048RegVcell = 0x02;
constexpr uint8_t kMax17048RegSoc = 0x04;

float fuel_soc_percent(uint8_t msb, uint8_t lsb);  // clamped 0..100
float fuel_vcell_volts(uint8_t msb, uint8_t lsb);
uint8_t fuel_percent_u8(float pct);                // rounded, clamped 0..100

}  // namespace rt
