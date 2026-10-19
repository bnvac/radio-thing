// Radar-head identification from the PIN_HEAD_ID ADC voltage (10k pull-up to 3V3,
// head fits a resistor to GND). Nominal voltages mirror include/board_pins.h.
#pragma once
#include "protocol.h"

namespace rt {

constexpr int kHeadNominalNoneMv = 3300;  // no resistor
constexpr int kHeadNominal24Mv = 1050;    // 4.7k
constexpr int kHeadNominalUwbMv = 1650;   // 10k
constexpr int kHeadNominalDualMv = 2270;  // 22k
// Decision thresholds: midpoints between adjacent nominal values.
constexpr int kHeadThr24UwbMv = (kHeadNominal24Mv + kHeadNominalUwbMv) / 2;    // 1350
constexpr int kHeadThrUwbDualMv = (kHeadNominalUwbMv + kHeadNominalDualMv) / 2;  // 1960
constexpr int kHeadThrDualNoneMv = (kHeadNominalDualMv + kHeadNominalNoneMv) / 2;  // 2785
// Below this the ID line is shorted/faulty (a 4.7k head reads ~1.05 V): report NONE.
constexpr int kHeadFaultBelowMv = 300;

HeadType head_from_mv(int millivolts);
const char* head_name(HeadType h);

}  // namespace rt
