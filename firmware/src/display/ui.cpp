#include "ui.h"

#include <math.h>

#include "protocol.h"

namespace {

constexpr const char* kFwVersion = "0.1.0-display";
const char* const kMaterials[] = {"Drywall / wood", "Brick", "Concrete", "Unknown"};
constexpr float kMmPerFt = 304.8f;

// Layout (landscape 480x320).
constexpr int kHeaderH = 24;
constexpr int kBannerY = 24, kBannerH = 28;
constexpr int kMapY = 52, kMapH = 248;  // fan origin at (240, kMapY + kMapH)
constexpr int kFooterY = 300;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return lgfx::color565(r, g, b); }
const uint16_t cBg = 0x0000;
uint16_t cDim() { return rgb(60, 70, 80); }
uint16_t cText() { return rgb(220, 225, 230); }
uint16_t cGreen() { return rgb(60, 220, 90); }
uint16_t cAmber() { return rgb(240, 170, 30); }
uint16_t cRed() { return rgb(230, 60, 50); }
uint16_t cCyan() { return rgb(60, 200, 230); }

uint16_t batt_color(int pct) { return pct < 15 ? cRed() : (pct < 30 ? cAmber() : cGreen()); }

uint16_t shade(uint16_t r, uint16_t g, uint16_t b, uint8_t conf) {
    float k = 0.35f + 0.65f * (conf / 100.f);
    return rgb(uint8_t(r * k), uint8_t(g * k), uint8_t(b * k));
}

void people_text(char* out, size_t n, int count) {
    if (count > 3) snprintf(out, n, "3+");
    else snprintf(out, n, "%d", count);
}

struct Banner {
    char text[48];
    uint16_t color;
};

Banner make_banner(const PodState* pod, uint8_t channel) {
    Banner b;
    if (!pod) {
        snprintf(b.text, sizeof b.text, "NO POD - listening on ch %u", channel);
        b.color = lgfx::color565(70, 80, 95);
        return b;
    }
    const rt::Packet& p = pod->pkt;
    using namespace rt;
    if (p.flags & Flag::BLOCKED) { snprintf(b.text, sizeof b.text, "BLOCKED - cannot see through"); b.color = cRed(); }
    else if (p.flags & Flag::TOO_CLOSE) { snprintf(b.text, sizeof b.text, "TOO CLOSE / TOO REFLECTIVE"); b.color = cRed(); }
    else if (p.flags & Flag::NO_PENETRATION) { snprintf(b.text, sizeof b.text, "NO PENETRATION"); b.color = cRed(); }
    else if (p.head_type == HeadType::NONE) { snprintf(b.text, sizeof b.text, "NO RADAR HEAD ON POD"); b.color = cRed(); }
    else if (p.flags & Flag::HOLD_STILL) { snprintf(b.text, sizeof b.text, "HOLD STILL"); b.color = cAmber(); }
    else if (p.flags & Flag::WARMING_UP) { snprintf(b.text, sizeof b.text, "WARMING UP"); b.color = cAmber(); }
    else if (p.n_targets > 0) {
        bool solid = false;
        for (int i = 0; i < p.n_targets; i++) solid |= p.targets[i].kind != TargetKind::MAYBE;
        char c[4];
        people_text(c, sizeof c, p.n_targets);
        if (!solid) snprintf(b.text, sizeof b.text, "POSSIBLE PERSON (%s)", c);
        else if (p.n_targets == 1) snprintf(b.text, sizeof b.text, "PERSON DETECTED");
        else snprintf(b.text, sizeof b.text, "%s PEOPLE DETECTED", c);
        b.color = cGreen();
    } else if (p.flags & Flag::LOW_CONF) {
        // Deliberately never says "EMPTY": absence of detections is not proof of absence.
        snprintf(b.text, sizeof b.text, "NOTHING DETECTED (low confidence)");
        b.color = cAmber();
    } else {
        snprintf(b.text, sizeof b.text, "NOTHING DETECTED");
        b.color = rgb(60, 110, 160);
    }
    return b;
}

}  // namespace

void Ui::begin() {
    tft_.init();
    tft_.setRotation(1);  // landscape 480x320
    tft_.setBrightness(map(s_.brightness, 0, 100, 0, 255));
    tft_.fillScreen(cBg);
    cv_.setPsram(true);
    cv_.setColorDepth(16);
    have_sprite_ = cv_.createSprite(kW, kH) != nullptr;
    if (!have_sprite_) {
        cv_.setPsram(false);  // flicker-prone direct drawing fallback
    }
}

void Ui::toast(const char* msg, uint32_t now, uint32_t dur) {
    snprintf(toast_, sizeof toast_, "%s", msg);
    toast_until_ = now + dur;
}

void Ui::fmt_dist(char* out, size_t n, float mm) const {
    if (s_.feet) snprintf(out, n, "%.0f ft", mm / kMmPerFt);
    else snprintf(out, n, "%.0f m", mm / 1000.f);
}

// ---------------------------------------------------------------- navigation

void Ui::push(Screen s) {
    if (depth_ < 6) stack_[depth_++] = screen_;
    screen_ = s;
    sel_[static_cast<int>(s)] = 0;
}

void Ui::pop() {
    if (depth_ > 0) screen_ = stack_[--depth_];
    else screen_ = Screen::MAP;
}

void Ui::set_mode(rt::Mode m) {
    s_.mode = static_cast<uint8_t>(m);
    s_.save();
    rt::Command c;
    c.target_pod = rt::kPodBroadcast;
    c.seq = cmd_seq_++;
    c.cmd = rt::CmdType::SET_MODE;
    c.arg = static_cast<uint8_t>(m);
    app_send_command(c);
}

void Ui::cycle_mode(uint32_t now) {
    if (s_.nerf) {
        toast("Nerf mode: SCAN only", now);
        return;
    }
    rt::Mode m = static_cast<rt::Mode>((s_.mode + 1) % rt::kModeCount);
    set_mode(m);
    char buf[40];
    snprintf(buf, sizeof buf, "Mode: %s", rt::mode_name(m));
    toast(buf, now);
}

void Ui::on_button(Button b, uint32_t now) {
    if (b == Button::MODE) {
        cycle_mode(now);
        return;
    }
    switch (screen_) {
        case Screen::MAP:
            if (b == Button::UP)
                s_.range = static_cast<uint8_t>(rt::next_range(static_cast<rt::RangeSetting>(s_.range), false));
            else if (b == Button::DOWN)
                s_.range = static_cast<uint8_t>(rt::next_range(static_cast<rt::RangeSetting>(s_.range), true));
            else if (b == Button::BACK) {
                int n = pods_.next_active(selected_pod_);
                if (n >= 0) selected_pod_ = n;
            } else if (b == Button::OK) {
                depth_ = 0;
                push(Screen::MAIN);
            }
            if (b == Button::UP || b == Button::DOWN) s_.save();
            break;
        case Screen::EDIT_CHANNEL:
        case Screen::EDIT_BRIGHT: {
            bool ch = screen_ == Screen::EDIT_CHANNEL;
            int lo = ch ? 1 : 10, hi = ch ? 13 : 100, step = ch ? 1 : 10;
            if (b == Button::UP) edit_value_ = min(hi, edit_value_ + step);
            else if (b == Button::DOWN) edit_value_ = max(lo, edit_value_ - step);
            if (!ch && (b == Button::UP || b == Button::DOWN)) app_apply_brightness(edit_value_);  // live preview
            else if (b == Button::BACK) {
                if (!ch) app_apply_brightness(s_.brightness);
                pop();
            } else if (b == Button::OK) {
                if (ch) {
                    s_.channel = edit_value_;
                    app_set_channel(edit_value_);
                    toast("Channel set (pods follow)", now);
                } else {
                    s_.brightness = edit_value_;
                    app_apply_brightness(edit_value_);
                }
                s_.save();
                pop();
            }
            break;
        }
        default:
            if (b == Button::UP) sel_[static_cast<int>(screen_)] = max(0, sel_[static_cast<int>(screen_)] - 1);
            else if (b == Button::DOWN) {
                build_menu();
                sel_[static_cast<int>(screen_)] = min(n_items_ - 1, sel_[static_cast<int>(screen_)] + 1);
            } else if (b == Button::BACK) pop();
            else if (b == Button::OK) {
                build_menu();
                select_item();
                if (screen_ == Screen::MAP) depth_ = 0;
            }
            break;
    }
    (void)now;
}

void Ui::build_menu() {
    n_items_ = 0;
    auto add = [&](const char* fmt, auto... args) {
        if (n_items_ < kMaxItems) snprintf(items_[n_items_++], sizeof items_[0], fmt, args...);
    };
    switch (screen_) {
        case Screen::MAIN:
            title_ = "MENU";
            add("Mode"); add("Pods"); add("Wall"); add("Settings"); add("Status");
            break;
        case Screen::MODE:
            title_ = "MODE";
            for (uint8_t m = 0; m < rt::kModeCount; m++)
                add("%s%s", s_.mode == m ? "* " : "  ", m == 2 ? "MULTI-SPOT" : rt::mode_name(static_cast<rt::Mode>(m)));
            break;
        case Screen::PODS:
            title_ = "PODS";
            for (int i = 0; i < kMaxPods; i++) {
                const PodState& p = pods_.at(i);
                if (p.active) add("%sPod %u  batt %u%%  link %u%%", i == selected_pod_ ? "* " : "  ", p.id, p.pkt.battery_pct, p.link_pct);
            }
            add("Pair new pods");
            break;
        case Screen::POD_ACT:
            title_ = "POD";
            add("Show on map"); add("Locate (blink LED 10 s)"); add("Stop locating");
            break;
        case Screen::WALL:
            title_ = "WALL";
            add("Calibrate (not implemented)"); add("Material: %s", kMaterials[s_.material]);
            break;
        case Screen::MATERIAL:
            title_ = "WALL MATERIAL";
            for (uint8_t i = 0; i < 4; i++) add("%s%s", s_.material == i ? "* " : "  ", kMaterials[i]);
            break;
        case Screen::SETTINGS:
            title_ = "SETTINGS";
            add("Channel: %u", s_.channel); add("Brightness: %u%%", s_.brightness);
            add("Units: %s", s_.feet ? "feet" : "metres"); add("Buzzer: %s", s_.buzzer ? "on" : "off");
            add("Nerf mode: %s", s_.nerf ? "on" : "off");
            break;
        case Screen::STATUS: {
            title_ = "STATUS";
            add("Firmware %s", kFwVersion);
            add("Display batt %.0f%%  %.2f V", app_battery_pct(), app_battery_volts());
            add("Channel %u   pods %d/%d", s_.channel, pods_.active_count(), kMaxPods);
            for (int i = 0; i < kMaxPods; i++) {
                const PodState& p = pods_.at(i);
                if (p.active)
                    add("Pod %u: %u%%  link %u%%  %s%s", p.id, p.pkt.battery_pct, p.link_pct,
                        p.pkt.head_type == rt::HeadType::H24 ? "24G" : p.pkt.head_type == rt::HeadType::UWB ? "UWB" : p.pkt.head_type == rt::HeadType::DUAL ? "dual" : "nohead",
                        (p.pkt.flags & rt::Flag::LOW_BATT) ? " LOW" : (p.pkt.flags & rt::Flag::CHARGING) ? " chg" : "");
            }
            break;
        }
        default: break;
    }
}

void Ui::select_item() {
    const int sel = sel_[static_cast<int>(screen_)];
    switch (screen_) {
        case Screen::MAIN: {
            static const Screen to[] = {Screen::MODE, Screen::PODS, Screen::WALL, Screen::SETTINGS, Screen::STATUS};
            push(to[sel]);
            break;
        }
        case Screen::MODE:
            set_mode(static_cast<rt::Mode>(sel));
            pop();
            pop();  // back to the map
            break;
        case Screen::PODS: {
            int slot = -1, k = 0;
            for (int i = 0; i < kMaxPods; i++)
                if (pods_.at(i).active && k++ == sel) slot = i;
            if (slot >= 0) {
                act_pod_ = slot;
                push(Screen::POD_ACT);
            } else {  // "Pair new pods": acknowledge every pod that is in pairing mode
                rt::Command c;
                c.cmd = rt::CmdType::PAIR_ACK;
                c.seq = cmd_seq_++;
                app_send_command(c);
                toast("Pairing: accepted pods in pairing mode", millis());
            }
            break;
        }
        case Screen::POD_ACT:
            if (act_pod_ >= 0 && pods_.at(act_pod_).active) {
                if (sel == 0) { selected_pod_ = act_pod_; pop(); pop(); pop(); }
                else {
                    rt::Command c;
                    c.target_pod = pods_.at(act_pod_).id;
                    c.cmd = rt::CmdType::LOCATE;
                    c.arg = sel == 1 ? 10 : 0;
                    c.seq = cmd_seq_++;
                    app_send_command(c);
                    toast(sel == 1 ? "Locating: pod LED blinking" : "Locate stopped", millis());
                }
            }
            break;
        case Screen::WALL:
            if (sel == 0) toast("Calibration needs a range-profile head", millis(), 2500);
            else push(Screen::MATERIAL);
            break;
        case Screen::MATERIAL:
            s_.material = sel;
            s_.save();
            pop();
            break;
        case Screen::SETTINGS:
            if (sel == 0) { edit_value_ = s_.channel; push(Screen::EDIT_CHANNEL); }
            else if (sel == 1) { edit_value_ = s_.brightness; push(Screen::EDIT_BRIGHT); }
            else if (sel == 2) s_.feet = !s_.feet;
            else if (sel == 3) s_.buzzer = !s_.buzzer;
            else if (sel == 4) {
                s_.nerf = !s_.nerf;
                if (s_.nerf) {
                    s_.range = 0;
                    set_mode(rt::Mode::SCAN);
                }
            }
            s_.save();
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- drawing

void Ui::draw_target(int x, int y, const rt::WireTarget& t) {
    auto& d = g();
    const int r = s_.nerf ? 11 : 6;
    uint16_t base_r = 60, base_g = 220, base_b = 90;                     // moving
    if (t.kind == rt::TargetKind::BREATHING) { base_r = 60; base_g = 200; base_b = 230; }
    uint16_t col = shade(base_r, base_g, base_b, t.confidence);
    if (t.kind == rt::TargetKind::MAYBE) {  // hollow
        d.drawCircle(x, y, r, col);
        d.drawCircle(x, y, r - 1, col);
    } else {
        d.fillCircle(x, y, r, col);
        d.drawCircle(x, y, r + 2, col);
    }
}

void Ui::draw_ring_label(int x, int y, float mm) {
    char b[12];
    fmt_dist(b, sizeof b, mm);
    auto& d = g();
    d.setFont(&fonts::Font0);
    d.setTextColor(cDim());
    d.setTextDatum(lgfx::top_left);
    d.drawString(b, x, y);
}

void Ui::draw_fan(const PodState& pod, uint32_t) {
    auto& d = g();
    const float R = rt::range_mm(static_cast<rt::RangeSetting>(s_.range));
    const rt::MapView v = rt::make_fan_view(0, kMapY, kW, kMapH, R);
    d.setClipRect(0, kMapY, kW, kMapH);
    const uint16_t ring = cDim();
    // Range rings (3 per view) as polylines over the +-60 degree FOV.
    for (int k = 1; k <= 3; k++) {
        float rr = R * k / 3.f * v.px_per_mm;
        int px = -1, py = -1;
        for (int a = -60; a <= 60; a += 4) {
            float th = a * 3.14159265f / 180.f;
            int x = v.anchor_x + int(rr * sinf(th)), y = v.anchor_y - int(rr * cosf(th));
            if (px >= 0) d.drawLine(px, py, x, y, ring);
            px = x; py = y;
        }
        draw_ring_label(v.anchor_x + 3, v.anchor_y - int(rr) + 2, R * k / 3.f);
    }
    float edge = R * v.px_per_mm;
    d.drawLine(v.anchor_x, v.anchor_y, v.anchor_x + int(edge * 0.8660254f), v.anchor_y - int(edge * 0.5f), ring);
    d.drawLine(v.anchor_x, v.anchor_y, v.anchor_x - int(edge * 0.8660254f), v.anchor_y - int(edge * 0.5f), ring);
    d.fillTriangle(v.anchor_x - 6, v.anchor_y, v.anchor_x + 6, v.anchor_y, v.anchor_x, v.anchor_y - 10, cCyan());

    for (int i = 0; i < pod.pkt.n_targets; i++) {
        const rt::WireTarget& t = pod.pkt.targets[i];
        rt::ScreenPt p = rt::map_to_screen(v, t.x_mm, t.y_mm);
        if (p.inside) draw_target(p.x, p.y, t);
    }
    d.clearClipRect();

    // Nearest-target readout (big, for Nerf mode).
    if (s_.nerf && pod.pkt.n_targets > 0) {
        float best = 1e9f;
        for (int i = 0; i < pod.pkt.n_targets; i++) {
            const auto& t = pod.pkt.targets[i];
            float dist = sqrtf(float(t.x_mm) * t.x_mm + float(t.y_mm) * t.y_mm);
            if (dist < best) best = dist;
        }
        char b[16];
        if (s_.feet) snprintf(b, sizeof b, "%.1f ft", best / kMmPerFt);
        else snprintf(b, sizeof b, "%.1f m", best / 1000.f);
        d.setFont(&fonts::Font4);
        d.setTextSize(2);
        d.setTextColor(cText());
        d.setTextDatum(lgfx::top_right);
        d.drawString(b, kW - 6, kMapY + 4);
        d.setTextSize(1);
    }
}

// MULTI: all live pods in one world frame. Placements are a fixed default layout
// (pods side by side along the wall, facing +y); an editor for real positions is not built yet.
void Ui::draw_world(uint32_t) {
    auto& d = g();
    const float R = rt::range_mm(static_cast<rt::RangeSetting>(s_.range));
    int n = pods_.active_count(), k = 0;
    float half = ((n - 1) / 2.f) * 1500.f + R * 0.6f;
    rt::MapView v = rt::make_world_view(0, kMapY, kW, kMapH, -half, -400.f, half, R);
    d.setClipRect(0, kMapY, kW, kMapH);
    k = 0;
    for (int i = 0; i < kMaxPods; i++) {
        const PodState& p = pods_.at(i);
        if (!p.active) continue;
        rt::PodPlacement q = {(k - (n - 1) / 2.f) * 1500.f, 0.f, 0.f};
        k++;
        rt::ScreenPt o = rt::map_to_screen(v, q.x_mm, q.y_mm);
        d.fillTriangle(o.x - 6, o.y, o.x + 6, o.y, o.x, o.y - 10, i == selected_pod_ ? cCyan() : cDim());
        for (int j = 0; j < p.pkt.n_targets; j++) {
            float wx, wy;
            rt::pod_to_world(q, p.pkt.targets[j].x_mm, p.pkt.targets[j].y_mm, wx, wy);
            rt::ScreenPt s = rt::map_to_screen(v, wx, wy);
            if (s.inside) draw_target(s.x, s.y, p.pkt.targets[j]);
        }
    }
    d.clearClipRect();
}

void Ui::draw_header(const PodState* pod) {
    auto& d = g();
    d.fillRect(0, 0, kW, kHeaderH, rgb(20, 26, 34));
    d.setFont(&fonts::Font2);
    d.setTextDatum(lgfx::middle_left);
    d.setTextColor(cText());
    char b[48];
    if (pod) {
        snprintf(b, sizeof b, "POD %u  %s%s", pod->id, rt::mode_name(pod->pkt.mode), s_.nerf ? "  NERF" : "");
    } else {
        snprintf(b, sizeof b, "RADIO THING  %s", s_.nerf ? "NERF" : "");
    }
    d.drawString(b, 6, kHeaderH / 2);
    d.setTextDatum(lgfx::middle_right);
    int x = kW - 6;
    int dp = int(app_battery_pct() + 0.5f);
    snprintf(b, sizeof b, "DISP %d%%", dp);
    d.setTextColor(batt_color(dp));
    d.drawString(b, x, kHeaderH / 2);
    if (pod) {
        int pp = pod->pkt.battery_pct;
        snprintf(b, sizeof b, "POD %d%%%s", pp, (pod->pkt.flags & rt::Flag::CHARGING) ? "+" : "");
        d.setTextColor(batt_color(pp));
        d.drawString(b, x - 105, kHeaderH / 2);
    }
}

void Ui::draw_banner(const PodState* pod) {
    Banner b = make_banner(pod, s_.channel);
    auto& d = g();
    d.fillRect(0, kBannerY, kW, kBannerH, b.color);
    d.setFont(&fonts::Font4);
    d.setTextDatum(lgfx::middle_center);
    d.setTextColor(b.color == cAmber() ? 0x0000 : 0xFFFF);
    d.drawString(b.text, kW / 2, kBannerY + kBannerH / 2);
}

void Ui::draw_footer(const PodState* pod) {
    auto& d = g();
    d.fillRect(0, kFooterY, kW, kH - kFooterY, rgb(20, 26, 34));
    d.setFont(&fonts::Font2);
    d.setTextDatum(lgfx::middle_left);
    d.setTextColor(cText());
    char b[64];
    fmt_dist(b, sizeof b, rt::range_mm(static_cast<rt::RangeSetting>(s_.range)));
    char r[72];
    snprintf(r, sizeof r, "range %s", b);
    d.drawString(r, 6, kFooterY + 10);
    d.setTextDatum(lgfx::middle_right);
    if (pod) {
        snprintf(b, sizeof b, "link %u%%  pods %d", pod->link_pct, pods_.active_count());
        d.setTextColor(pod->link_pct < 60 ? cAmber() : cText());
        d.drawString(b, kW - 6, kFooterY + 10);
    }
    d.setTextDatum(lgfx::middle_center);
    for (int i = 0; i < kMaxPods; i++) {
        const PodState& p = pods_.at(i);
        if (p.active && (p.pkt.flags & rt::Flag::PAIRING)) {
            snprintf(b, sizeof b, "pod %u pairing: Menu>Pods>Pair", p.id);
            d.setTextColor(cCyan());
            d.drawString(b, kW / 2, kFooterY + 10);
            break;
        }
    }
}

void Ui::draw_map(uint32_t now) {
    auto& d = g();
    const PodState* pod = nullptr;
    if (selected_pod_ >= 0 && pods_.at(selected_pod_).active) pod = &pods_.at(selected_pod_);
    else {
        selected_pod_ = pods_.next_active(-1);
        if (selected_pod_ >= 0) pod = &pods_.at(selected_pod_);
    }
    d.fillScreen(cBg);
    if (pod) {
        if (pod->pkt.mode == rt::Mode::MULTI && pods_.active_count() > 1) draw_world(now);
        else draw_fan(*pod, now);
        // People count, capped at "3+".
        char c[4];
        people_text(c, sizeof c, pod->pkt.n_targets);
        d.setFont(&fonts::Font4);
        d.setTextSize(2);
        d.setTextDatum(lgfx::top_left);
        d.setTextColor(pod->pkt.n_targets ? cGreen() : cDim());
        d.drawString(c, 8, kMapY + 2);
        d.setTextSize(1);
        d.setFont(&fonts::Font2);
        d.setTextColor(cDim());
        d.drawString("detected", 8, kMapY + 56);
    }
    draw_header(pod);
    draw_banner(pod);
    draw_footer(pod);
}

void Ui::draw_menu() {
    build_menu();
    auto& d = g();
    d.fillScreen(cBg);
    d.fillRect(0, 0, kW, kHeaderH + 4, rgb(20, 26, 34));
    d.setFont(&fonts::Font4);
    d.setTextDatum(lgfx::middle_left);
    d.setTextColor(cCyan());
    d.drawString(title_, 8, (kHeaderH + 4) / 2);
    const bool selectable = screen_ != Screen::STATUS;
    int& sel = sel_[static_cast<int>(screen_)];
    if (sel >= n_items_) sel = max(0, n_items_ - 1);
    const int rowh = 32, y0 = 40;
    d.setFont(&fonts::Font4);
    for (int i = 0; i < n_items_; i++) {
        int y = y0 + i * rowh;
        bool hi = selectable && i == sel;
        if (hi) d.fillRoundRect(4, y, kW - 8, rowh - 4, 4, rgb(30, 80, 120));
        d.setTextColor(hi ? 0xFFFF : cText());
        d.setTextDatum(lgfx::middle_left);
        d.drawString(items_[i], 14, y + rowh / 2 - 2);
    }
    d.setFont(&fonts::Font2);
    d.setTextColor(cDim());
    d.setTextDatum(lgfx::bottom_left);
    d.drawString(selectable ? "UP/DOWN move   OK select   BACK back" : "BACK back", 8, kH - 4);
}

void Ui::draw_edit() {
    auto& d = g();
    d.fillScreen(cBg);
    bool ch = screen_ == Screen::EDIT_CHANNEL;
    d.setFont(&fonts::Font4);
    d.setTextDatum(lgfx::top_center);
    d.setTextColor(cCyan());
    d.drawString(ch ? "ESP-NOW CHANNEL" : "BRIGHTNESS", kW / 2, 40);
    char b[16];
    snprintf(b, sizeof b, ch ? "%d" : "%d%%", edit_value_);
    d.setTextSize(3);
    d.setTextColor(0xFFFF);
    d.setTextDatum(lgfx::middle_center);
    d.drawString(b, kW / 2, 150);
    d.setTextSize(1);
    d.setFont(&fonts::Font2);
    d.setTextColor(cDim());
    d.setTextDatum(lgfx::bottom_center);
    d.drawString(ch ? "UP/DOWN change  OK apply (pods follow)  BACK cancel" : "UP/DOWN change  OK save  BACK cancel", kW / 2, kH - 6);
}

void Ui::render(uint32_t now) {
    if (screen_ == Screen::MAP) draw_map(now);
    else if (screen_ == Screen::EDIT_CHANNEL || screen_ == Screen::EDIT_BRIGHT) draw_edit();
    else draw_menu();

    if (static_cast<int32_t>(toast_until_ - now) > 0) {
        auto& d = g();
        d.setFont(&fonts::Font2);
        int w = d.textWidth(toast_) + 24;
        d.fillRoundRect((kW - w) / 2, kH - 60, w, 28, 6, rgb(40, 50, 65));
        d.drawRoundRect((kW - w) / 2, kH - 60, w, 28, 6, cCyan());
        d.setTextColor(0xFFFF);
        d.setTextDatum(lgfx::middle_center);
        d.drawString(toast_, kW / 2, kH - 46);
    }
    if (have_sprite_) cv_.pushSprite(0, 0);
}
