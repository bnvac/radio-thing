// Map geometry: pod-relative mm -> screen pixels, and multi-pod placement.
// Pod frame: +y = out of the sensor face, +x = right. Screen: y grows downward.
// World frame (multi-pod): +x east, +y north; heading is degrees clockwise from north.
#pragma once

namespace rt {

enum class RangeSetting : unsigned char { R3M = 0, R6M = 1, R9M = 2 };

float range_mm(RangeSetting r);
RangeSetting next_range(RangeSetting r, bool zoom_out);  // clamps at the ends

struct MapView {
    int anchor_x = 0;          // pixel of the world point (origin_x_mm, origin_y_mm)
    int anchor_y = 0;
    float px_per_mm = 0.1f;
    float origin_x_mm = 0;
    float origin_y_mm = 0;
    // Clip rectangle in pixels, used for `inside`.
    int clip_x = 0, clip_y = 0, clip_w = 0, clip_h = 0;
};

struct ScreenPt {
    int x = 0;
    int y = 0;
    bool inside = false;
};

// Top-down fan view: sensor sits at the bottom centre of the rectangle, range maps to the
// full rectangle height (and is limited laterally so nothing scales outside half the width).
MapView make_fan_view(int rect_x, int rect_y, int rect_w, int rect_h, float max_range_mm);

// View centred on a world rectangle (multi-pod): fits [min..max] in the rectangle.
MapView make_world_view(int rect_x, int rect_y, int rect_w, int rect_h, float min_x_mm,
                        float min_y_mm, float max_x_mm, float max_y_mm);

ScreenPt map_to_screen(const MapView& v, float x_mm, float y_mm);

struct PodPlacement {
    float x_mm = 0;
    float y_mm = 0;
    float heading_deg = 0;  // clockwise from north (+y world)
};

void pod_to_world(const PodPlacement& p, float px_mm, float py_mm, float& wx_mm, float& wy_mm);

}  // namespace rt
