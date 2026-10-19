#include "blockage.h"

namespace rt {

BlockStatus evaluate_blockage(const BlockageFeatures& f, const BlockageConfig& cfg) {
    if (f.saturated) return BlockStatus::TOO_CLOSE;

    float far_ratio = 0.f;
    bool profile_ok = f.has_profile && f.noise_floor > 0.f;
    if (profile_ok) {
        float near_ratio = f.near_energy / f.noise_floor;
        far_ratio = f.far_energy / f.noise_floor;
        if (near_ratio >= cfg.blocked_near_ratio && far_ratio < cfg.blocked_far_ratio)
            return BlockStatus::BLOCKED;
        if (far_ratio < cfg.min_far_ratio) return BlockStatus::NO_PENETRATION;
    }

    if (cfg.hold_still_mg > 0.f && f.imu_motion_mg > cfg.hold_still_mg)
        return BlockStatus::HOLD_STILL;

    if (f.any_detection) return BlockStatus::OK;

    bool long_enough = f.look_time_ms >= cfg.min_look_ms;
    if (long_enough && profile_ok && far_ratio >= cfg.good_far_ratio)
        return BlockStatus::NOTHING_DETECTED;
    return BlockStatus::NOTHING_DETECTED_LOW_CONF;
}

const char* block_status_text(BlockStatus s) {
    switch (s) {
        case BlockStatus::OK: return "OK";
        case BlockStatus::BLOCKED: return "BLOCKED";
        case BlockStatus::NO_PENETRATION: return "NO PENETRATION";
        case BlockStatus::HOLD_STILL: return "HOLD STILL";
        case BlockStatus::TOO_CLOSE: return "TOO CLOSE";
        case BlockStatus::NOTHING_DETECTED_LOW_CONF: return "NOTHING DETECTED (low confidence)";
        case BlockStatus::NOTHING_DETECTED: return "NOTHING DETECTED";
    }
    return "?";
}

}  // namespace rt
