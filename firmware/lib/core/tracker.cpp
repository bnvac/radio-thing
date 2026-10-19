#include "tracker.h"

#include <cmath>

namespace rt {

namespace {
uint8_t popcount16(uint16_t v) {
    uint8_t c = 0;
    while (v) {
        c += v & 1;
        v >>= 1;
    }
    return c;
}
uint16_t mask_n(uint8_t n) { return n >= 16 ? 0xFFFF : static_cast<uint16_t>((1u << n) - 1); }
}  // namespace

uint8_t Track::confidence() const {
    // Fixed 5-sample window for the reported figure.
    uint8_t hits = popcount16(hist & 0x1F);
    uint8_t span = age < 5 ? static_cast<uint8_t>(age) : 5;
    if (span == 0) return 0;
    return static_cast<uint8_t>(hits * 100 / span);
}

uint8_t Tracker::alloc_id() {
    for (int tries = 0; tries < 256; tries++) {
        uint8_t id = next_id_;
        next_id_ = next_id_ == 255 ? 1 : static_cast<uint8_t>(next_id_ + 1);
        bool used = false;
        for (const Track& t : tracks_)
            if (t.id == id) used = true;
        if (!used) return id;
    }
    return 0;
}

void Tracker::reset() {
    for (Track& t : tracks_) t = Track();
    next_id_ = 1;
}

size_t Tracker::active_count() const {
    size_t c = 0;
    for (const Track& t : tracks_)
        if (t.id) c++;
    return c;
}

size_t Tracker::confirmed_count() const {
    size_t c = 0;
    for (const Track& t : tracks_)
        if (t.id && t.confirmed) c++;
    return c;
}

size_t Tracker::confirmed_tracks(Track* out, size_t max) const {
    size_t c = 0;
    for (const Track& t : tracks_)
        if (t.id && t.confirmed && c < max) out[c++] = t;
    return c;
}

void Tracker::update(const Detection* dets, size_t n, float dt_s) {
    if (dt_s <= 0.f) dt_s = 0.1f;
    if (n > 16) n = 16;

    // Predict.
    float px[kMaxTracks], py[kMaxTracks];
    for (size_t i = 0; i < kMaxTracks; i++) {
        px[i] = tracks_[i].x_mm + tracks_[i].vx_mmps * dt_s;
        py[i] = tracks_[i].y_mm + tracks_[i].vy_mmps * dt_s;
    }

    // Greedy global nearest-neighbour: repeatedly pick the closest unassigned pair within the gate.
    int8_t det_of_track[kMaxTracks];
    bool det_used[16] = {};
    for (auto& d : det_of_track) d = -1;
    const float gate2 = cfg_.gate_mm * cfg_.gate_mm;
    for (;;) {
        float best = gate2;
        int bt = -1, bd = -1;
        for (size_t t = 0; t < kMaxTracks; t++) {
            if (!tracks_[t].id || det_of_track[t] >= 0) continue;
            for (size_t d = 0; d < n; d++) {
                if (det_used[d]) continue;
                float dx = dets[d].x_mm - px[t], dy = dets[d].y_mm - py[t];
                float d2 = dx * dx + dy * dy;
                if (d2 <= best) {
                    best = d2;
                    bt = static_cast<int>(t);
                    bd = static_cast<int>(d);
                }
            }
        }
        if (bt < 0) break;
        det_of_track[bt] = static_cast<int8_t>(bd);
        det_used[bd] = true;
    }

    // Update matched / missed tracks.
    const uint16_t nmask = mask_n(cfg_.confirm_n);
    for (size_t t = 0; t < kMaxTracks; t++) {
        Track& tr = tracks_[t];
        if (!tr.id) continue;
        tr.age++;
        tr.hist = static_cast<uint16_t>(tr.hist << 1);
        if (det_of_track[t] >= 0) {
            const Detection& z = dets[det_of_track[t]];
            float rx = z.x_mm - px[t], ry = z.y_mm - py[t];
            tr.x_mm = px[t] + cfg_.alpha * rx;
            tr.y_mm = py[t] + cfg_.alpha * ry;
            tr.vx_mmps += cfg_.beta * rx / dt_s;
            tr.vy_mmps += cfg_.beta * ry / dt_s;
            tr.hist |= 1;
            tr.misses = 0;
            if (!tr.confirmed && popcount16(tr.hist & nmask) >= cfg_.confirm_m) tr.confirmed = true;
        } else {
            tr.x_mm = px[t];
            tr.y_mm = py[t];
            tr.misses++;
            uint8_t limit = tr.confirmed ? cfg_.max_misses : cfg_.tentative_max_misses;
            bool expired = !tr.confirmed && tr.age >= cfg_.confirm_n;
            if (tr.misses >= limit || expired) tr = Track();
        }
    }

    // Spawn tracks for unmatched detections.
    for (size_t d = 0; d < n; d++) {
        if (det_used[d]) continue;
        for (Track& tr : tracks_) {
            if (tr.id) continue;
            uint8_t id = alloc_id();
            if (!id) break;
            tr = Track();
            tr.id = id;
            tr.x_mm = dets[d].x_mm;
            tr.y_mm = dets[d].y_mm;
            tr.hist = 1;
            tr.age = 1;
            if (cfg_.confirm_m <= 1) tr.confirmed = true;
            break;
        }
    }
}

}  // namespace rt
