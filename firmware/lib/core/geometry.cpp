#include "geometry.h"

#include <cmath>

namespace rt {

float range_mm(RangeSetting r) {
    switch (r) {
        case RangeSetting::R3M: return 3000.f;
        case RangeSetting::R6M: return 6000.f;
        case RangeSetting::R9M: return 9000.f;
    }
    return 6000.f;
}

RangeSetting next_range(RangeSetting r, bool zoom_out) {
    int v = static_cast<int>(r) + (zoom_out ? 1 : -1);
    if (v < 0) v = 0;
    if (v > 2) v = 2;
    return static_cast<RangeSetting>(v);
}

MapView make_fan_view(int rx, int ry, int rw, int rh, float max_range_mm) {
    MapView v;
    // Height-limited scale; if the rectangle is too narrow for the full fan, shrink so the
    // lateral extent at max range (LD2450 FOV +-60 deg -> R*sin60) fits in half the width.
    constexpr float kSin60 = 0.8660254f;
    float s = static_cast<float>(rh) / max_range_mm;
    float s_lat = (static_cast<float>(rw) * 0.5f) / (max_range_mm * kSin60);
    if (s_lat < s) s = s_lat;
    v.px_per_mm = s;
    v.anchor_x = rx + rw / 2;
    v.anchor_y = ry + rh;
    v.clip_x = rx;
    v.clip_y = ry;
    v.clip_w = rw;
    v.clip_h = rh;
    return v;
}

MapView make_world_view(int rx, int ry, int rw, int rh, float min_x, float min_y, float max_x,
                        float max_y) {
    MapView v;
    float w = max_x - min_x, h = max_y - min_y;
    if (w < 1.f) w = 1.f;
    if (h < 1.f) h = 1.f;
    float sx = static_cast<float>(rw) / w, sy = static_cast<float>(rh) / h;
    v.px_per_mm = sx < sy ? sx : sy;
    v.origin_x_mm = (min_x + max_x) * 0.5f;
    v.origin_y_mm = (min_y + max_y) * 0.5f;
    v.anchor_x = rx + rw / 2;
    v.anchor_y = ry + rh / 2;
    v.clip_x = rx;
    v.clip_y = ry;
    v.clip_w = rw;
    v.clip_h = rh;
    return v;
}

ScreenPt map_to_screen(const MapView& v, float x_mm, float y_mm) {
    ScreenPt p;
    p.x = v.anchor_x + static_cast<int>(std::lround((x_mm - v.origin_x_mm) * v.px_per_mm));
    p.y = v.anchor_y - static_cast<int>(std::lround((y_mm - v.origin_y_mm) * v.px_per_mm));
    p.inside = p.x >= v.clip_x && p.x < v.clip_x + v.clip_w && p.y >= v.clip_y &&
               p.y < v.clip_y + v.clip_h;
    return p;
}

void pod_to_world(const PodPlacement& p, float px, float py, float& wx, float& wy) {
    const float h = p.heading_deg * 3.14159265358979f / 180.f;
    const float s = std::sin(h), c = std::cos(h);
    // forward = (sin h, cos h), right = (cos h, -sin h)
    wx = p.x_mm + px * c + py * s;
    wy = p.y_mm - px * s + py * c;
}

}  // namespace rt
