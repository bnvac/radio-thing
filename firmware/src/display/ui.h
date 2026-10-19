// Display UI: radar map, banner, hand-written menu system. Renders into a PSRAM sprite.
#pragma once
#include "app.h"
#include "geometry.h"
#include "lgfx_config.h"
#include "pods.h"
#include "settings.h"

enum class Button : uint8_t { UP, DOWN, OK, BACK, MODE };

class Ui {
public:
    Ui(LGFX& tft, Settings& settings, PodTable& pods)
        : tft_(tft), cv_(&tft), s_(settings), pods_(pods) {}

    void begin();
    void on_button(Button b, uint32_t now_ms);   // short presses
    void render(uint32_t now_ms);
    void toast(const char* msg, uint32_t now_ms, uint32_t dur_ms = 2000);
    bool on_map() const { return screen_ == Screen::MAP; }

private:
    enum class Screen : uint8_t {
        MAP, MAIN, MODE, PODS, POD_ACT, WALL, MATERIAL, SETTINGS, STATUS, EDIT_CHANNEL, EDIT_BRIGHT,
        COUNT
    };

    static constexpr int kW = 480, kH = 320;
    static constexpr int kMaxItems = 10;

    // navigation
    void push(Screen s);
    void pop();
    void select_item();
    void build_menu();
    void cycle_mode(uint32_t now);
    void set_mode(rt::Mode m);

    // drawing
    lgfx::LovyanGFX& g() { return have_sprite_ ? static_cast<lgfx::LovyanGFX&>(cv_) : static_cast<lgfx::LovyanGFX&>(tft_); }
    void draw_map(uint32_t now);
    void draw_fan(const PodState& pod, uint32_t now);
    void draw_world(uint32_t now);
    void draw_header(const PodState* pod);
    void draw_banner(const PodState* pod);
    void draw_footer(const PodState* pod);
    void draw_menu();
    void draw_edit();
    void draw_target(int x, int y, const rt::WireTarget& t);
    void draw_ring_label(int x, int y, float mm);
    void fmt_dist(char* out, size_t n, float mm) const;

    LGFX& tft_;
    lgfx::LGFX_Sprite cv_;
    bool have_sprite_ = false;
    Settings& s_;
    PodTable& pods_;

    Screen screen_ = Screen::MAP;
    Screen stack_[6] = {};
    int depth_ = 0;
    int sel_[static_cast<int>(Screen::COUNT)] = {};
    char items_[kMaxItems][44] = {};
    int n_items_ = 0;
    const char* title_ = "";
    int selected_pod_ = -1;   // table slot shown on the map
    int act_pod_ = -1;        // table slot targeted by the pod action menu
    int edit_value_ = 0;
    uint8_t cmd_seq_ = 0;
    char toast_[64] = {};
    uint32_t toast_until_ = 0;
};
