// Blockage / confidence logic ("know when it can't see"). Pure function.
//
// Rule: never output a confident "room empty". NOTHING_DETECTED is only returned
// after a minimum look time AND with a verified good penetration measurement
// (a range profile). Everything else that finds nothing is NOTHING_DETECTED_LOW_CONF.
//
// HONEST LIMITATION: the HLK-LD2450 exposes only target lists, no raw range/energy
// profile. A pod with only that head can supply imu_motion_mg and any_detection
// (has_profile = false). Energy-based checks (BLOCKED, NO_PENETRATION, TOO_CLOSE
// via saturation) cannot run, and "nothing" can never be upgraded beyond
// NOTHING_DETECTED_LOW_CONF. The full set only becomes reachable with a head that
// provides a range profile (UWB head) - not implemented yet.
#pragma once
#include <cstdint>

namespace rt {

enum class BlockStatus : uint8_t {
    OK = 0,                      // at least one detection, nothing wrong with the measurement
    BLOCKED,                     // strong near-field echo, little beyond: metal/foil at the wall
    NO_PENETRATION,              // far-field energy ~ noise floor: signal not getting through
    HOLD_STILL,                  // platform moving too much for the current mode
    TOO_CLOSE,                   // receiver saturated / too reflective
    NOTHING_DETECTED_LOW_CONF,   // nothing seen, but look too short or penetration unproven
    NOTHING_DETECTED,            // nothing seen after long look with proven penetration
};

struct BlockageFeatures {
    float near_energy = 0;        // mean echo energy in the first ~0.3 m (linear, same unit as noise)
    float far_energy = 0;         // mean echo energy beyond the near zone
    float noise_floor = 0;        // radar internal noise energy (same unit); > 0 when has_profile
    float imu_motion_mg = 0;      // IMU |a| deviation from 1 g, in milli-g (smoothed)
    bool saturated = false;       // receiver clipped
    uint32_t look_time_ms = 0;    // time spent observing in a steady state
    bool any_detection = false;   // any target/breathing detected in the look window
    bool has_profile = false;     // extension: false for heads with no range profile (LD2450)
};

struct BlockageConfig {
    // Ratios are energy / noise_floor.
    float blocked_near_ratio = 20.f;   // near echo at least this strong ...
    float blocked_far_ratio = 2.f;     // ... while far zone is below this -> BLOCKED
    float min_far_ratio = 3.f;         // far/noise below this -> NO_PENETRATION
    float good_far_ratio = 6.f;        // far/noise at/above this counts as proven penetration
    float hold_still_mg = 60.f;        // IMU motion above this -> HOLD_STILL (0 = disabled, e.g. SCAN)
    uint32_t min_look_ms = 10000;      // minimum look time before NOTHING_DETECTED
};

BlockStatus evaluate_blockage(const BlockageFeatures& f, const BlockageConfig& cfg = BlockageConfig());

const char* block_status_text(BlockStatus s);

}  // namespace rt
