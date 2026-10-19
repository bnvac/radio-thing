#include "fuel.h"

namespace rt {

float fuel_soc_percent(uint8_t msb, uint8_t lsb) {
    float pct = static_cast<float>(msb) + static_cast<float>(lsb) / 256.0f;
    return pct > 100.f ? 100.f : pct;
}

float fuel_vcell_volts(uint8_t msb, uint8_t lsb) {
    uint16_t raw = static_cast<uint16_t>((msb << 8) | lsb);
    return static_cast<float>(raw) * 78.125e-6f;
}

uint8_t fuel_percent_u8(float pct) {
    if (pct <= 0.f) return 0;
    if (pct >= 100.f) return 100;
    return static_cast<uint8_t>(pct + 0.5f);
}

}  // namespace rt
