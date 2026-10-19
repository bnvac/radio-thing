// Multi-target tracker: greedy nearest-neighbour association with a distance gate,
// per-track alpha-beta filter, M-of-N confirmation, deletion after misses.
// Units: mm and seconds. Pure C++, deterministic.
#pragma once
#include <cstddef>
#include <cstdint>

namespace rt {

struct TrackerConfig {
    float gate_mm = 700.f;        // max distance (to predicted position) to associate
    float alpha = 0.6f;           // position gain
    float beta = 0.15f;           // velocity gain
    uint8_t confirm_m = 3;        // hits needed ...
    uint8_t confirm_n = 5;        // ... within the last N updates (N <= 16)
    uint8_t max_misses = 8;       // consecutive misses before a confirmed track is deleted
    uint8_t tentative_max_misses = 2;  // consecutive misses before a tentative track is dropped
};

struct Detection {
    float x_mm = 0;
    float y_mm = 0;
};

struct Track {
    uint8_t id = 0;  // 0 = slot unused; IDs are 1..255, never reused while the track lives
    float x_mm = 0, y_mm = 0;
    float vx_mmps = 0, vy_mmps = 0;
    bool confirmed = false;
    uint8_t misses = 0;      // consecutive
    uint16_t hist = 0;       // bit i = hit i updates ago
    uint16_t age = 0;        // updates since creation
    uint8_t confidence() const;  // 0..100 from recent hit ratio
};

class Tracker {
public:
    static constexpr size_t kMaxTracks = 8;

    explicit Tracker(const TrackerConfig& cfg = TrackerConfig()) : cfg_(cfg) {}

    // One measurement cycle. dt_s = time since previous update.
    void update(const Detection* dets, size_t n, float dt_s);
    void reset();

    const TrackerConfig& config() const { return cfg_; }
    // Slots; check Track::id != 0.
    const Track& slot(size_t i) const { return tracks_[i]; }
    size_t active_count() const;
    size_t confirmed_count() const;
    // Copy confirmed tracks into out (up to max); returns count written.
    size_t confirmed_tracks(Track* out, size_t max) const;

private:
    TrackerConfig cfg_;
    Track tracks_[kMaxTracks];
    uint8_t next_id_ = 1;
    uint8_t alloc_id();
};

}  // namespace rt
