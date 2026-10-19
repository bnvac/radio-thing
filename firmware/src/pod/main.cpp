// Radio Thing POD firmware: radar head -> tracker -> ESP-NOW broadcast.
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "blockage.h"
#include "board_pins.h"
#include "debounce.h"
#include "fuel.h"
#include "head_id.h"
#include "protocol.h"
#include "radar_head.h"
#include "tracker.h"

namespace {

constexpr const char* kFwVersion = "0.1.0-pod";
constexpr uint32_t kSendPeriodMs = 100;      // 10 Hz
constexpr uint32_t kWarmupMs = 3000;         // after head power-up
constexpr uint32_t kPairingMs = 30000;
constexpr uint8_t kLowBattPct = 15;
constexpr uint8_t kDefaultChannel = 1;

// ---- ISM330DHCX (I2C 0x6A) -------------------------------------------------------------
constexpr uint8_t kImuAddr = 0x6A;
constexpr uint8_t kImuRegWhoAmI = 0x0F;  // reads 0x6B
constexpr uint8_t kImuRegCtrl1Xl = 0x10; // ODR_XL[7:4]=0100 (104 Hz), FS_XL[3:2]=00 (+-2 g)
constexpr uint8_t kImuRegOutXlA = 0x28;  // OUTX_L_A .. OUTZ_H_A, 6 bytes, auto-increment
constexpr float kImuMgPerLsb = 0.061f;   // +-2 g

bool g_imu_ok = false;
bool g_fuel_ok = false;

bool i2c_write8(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, n) != n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

bool imu_begin() {
    uint8_t who = 0;
    if (!i2c_read(kImuAddr, kImuRegWhoAmI, &who, 1) || who != 0x6B) return false;
    return i2c_write8(kImuAddr, kImuRegCtrl1Xl, 0x40);
}

// High-passed acceleration magnitude (mg), smoothed. Hand tremor / platform motion raises
// it; gravity (a constant vector) is removed by the slow low-pass reference.
struct MotionEstimator {
    float ref[3] = {0, 0, 0};
    float motion_mg = 0;
    bool primed = false;

    void update(const float a[3]) {
        if (!primed) {
            for (int i = 0; i < 3; i++) ref[i] = a[i];
            primed = true;
        }
        float d2 = 0;
        for (int i = 0; i < 3; i++) {
            float d = a[i] - ref[i];
            d2 += d * d;
            ref[i] += 0.05f * d;
        }
        motion_mg += 0.2f * (sqrtf(d2) - motion_mg);
    }
} g_motion;

void imu_poll() {
    if (!g_imu_ok) return;
    uint8_t b[6];
    if (!i2c_read(kImuAddr, kImuRegOutXlA, b, 6)) return;
    float a[3];
    for (int i = 0; i < 3; i++)
        a[i] = static_cast<int16_t>(b[2 * i] | (b[2 * i + 1] << 8)) * kImuMgPerLsb;
    g_motion.update(a);
}

// ---- Battery ---------------------------------------------------------------------------
float g_batt_pct = 0;
float g_batt_v = 0;

void battery_poll() {
    uint8_t b[2];
    if (i2c_read(rt::kMax17048Addr, rt::kMax17048RegSoc, b, 2)) {
        g_batt_pct = rt::fuel_soc_percent(b[0], b[1]);
        g_fuel_ok = true;
    }
    if (i2c_read(rt::kMax17048Addr, rt::kMax17048RegVcell, b, 2))
        g_batt_v = rt::fuel_vcell_volts(b[0], b[1]);
}

bool charging() { return digitalRead(PIN_CHG_N) == LOW; }

// ---- Radio -----------------------------------------------------------------------------
const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
Preferences g_prefs;
uint8_t g_channel = kDefaultChannel;
uint8_t g_pod_id = 0;
uint16_t g_seq = 0;

portMUX_TYPE g_cmd_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_cmd_pending = false;
rt::Command g_cmd_slot;

void on_recv(const uint8_t*, const uint8_t* data, int len) {
    if (rt::packet_type(data, len) != rt::PacketType::COMMAND) return;
    rt::Command c;
    if (rt::deserialize(data, len, c) != rt::ParseResult::OK) return;
    if (c.target_pod != rt::kPodBroadcast && c.target_pod != g_pod_id) return;
    portENTER_CRITICAL(&g_cmd_mux);
    g_cmd_slot = c;
    g_cmd_pending = true;
    portEXIT_CRITICAL(&g_cmd_mux);
}

void radio_set_channel(uint8_t ch) {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

bool radio_begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    radio_set_channel(g_channel);
    if (esp_now_init() != ESP_OK) return false;
    esp_now_register_recv_cb(on_recv);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcast, 6);
    peer.channel = 0;  // current channel
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

// ---- LED -------------------------------------------------------------------------------
void led(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r);
    digitalWrite(PIN_LED_G, g);
    digitalWrite(PIN_LED_B, b);
}

// ---- State -----------------------------------------------------------------------------
rt::Mode g_mode = rt::Mode::SCAN;
rt::HeadType g_head_type = rt::HeadType::NONE;
RadarHead* g_head = nullptr;
Ld2450Head g_ld2450;
UwbHead g_uwb;
rt::Tracker g_tracker;
rt::Debouncer g_btn(25, 1500);

uint32_t g_head_start_ms = 0;
uint32_t g_last_frame_ms = 0;
uint32_t g_last_send_ms = 0;
uint32_t g_still_since_ms = 0;  // start of the current steady look
uint32_t g_pairing_until = 0;
uint32_t g_locate_until = 0;
uint32_t g_channel_switch_at = 0;
uint8_t g_pending_channel = 0;
rt::BlockStatus g_status = rt::BlockStatus::NOTHING_DETECTED_LOW_CONF;

rt::BlockageConfig blockage_config() {
    rt::BlockageConfig c;
    // Free-handheld SCAN tolerates motion; LISTEN/MULTI need the pod braced and still.
    c.hold_still_mg = (g_mode == rt::Mode::SCAN) ? 0.f : 60.f;
    return c;
}

void set_mode(rt::Mode m) {
    g_mode = m;
    g_still_since_ms = millis();  // restart the look-time clock
}

void pick_head() {
    int mv = 0;
    for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(PIN_HEAD_ID);
    mv /= 16;
    g_head_type = rt::head_from_mv(mv);
    Serial.printf("head id: %d mV -> %s\n", mv, rt::head_name(g_head_type));
    if (g_head_type == rt::HeadType::NONE) return;
    pinMode(PIN_HEAD_EN, OUTPUT);
    digitalWrite(PIN_HEAD_EN, HIGH);
    g_head_start_ms = millis();
    delay(300);  // let the 5 V rail and the head settle
    // UWB-only: stubbed driver. DUAL: LD2450 on A is used, UWB on B is not driven yet.
    g_head = (g_head_type == rt::HeadType::UWB) ? static_cast<RadarHead*>(&g_uwb)
                                                : static_cast<RadarHead*>(&g_ld2450);
    if (!g_head->begin()) Serial.println("head driver begin() failed / stubbed");
}

void handle_command() {
    if (!g_cmd_pending) return;
    rt::Command c;
    portENTER_CRITICAL(&g_cmd_mux);
    c = g_cmd_slot;
    g_cmd_pending = false;
    portEXIT_CRITICAL(&g_cmd_mux);
    switch (c.cmd) {
        case rt::CmdType::SET_MODE: set_mode(static_cast<rt::Mode>(c.arg)); break;
        case rt::CmdType::LOCATE: g_locate_until = c.arg ? millis() + c.arg * 1000UL : 0; break;
        case rt::CmdType::PAIR_ACK: g_pairing_until = 0; break;
        case rt::CmdType::SET_CHANNEL:
            g_pending_channel = c.arg;
            g_channel_switch_at = millis() + 300;  // let the display finish its repeats
            break;
    }
}

void handle_serial() {
    // "chan N" over USB serial sets the ESP-NOW channel (pods have no screen).
    static String line;
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch != '\n' && ch != '\r') {
            line += ch;
            continue;
        }
        int n = 0;
        if (sscanf(line.c_str(), "chan %d", &n) == 1 && n >= 1 && n <= 13) {
            g_pending_channel = n;
            g_channel_switch_at = millis();
        }
        line = "";
    }
}

void apply_channel_switch() {
    if (!g_pending_channel || static_cast<int32_t>(millis() - g_channel_switch_at) < 0) return;
    g_channel = g_pending_channel;
    g_pending_channel = 0;
    radio_set_channel(g_channel);
    g_prefs.putUChar("chan", g_channel);
    Serial.printf("channel -> %u\n", g_channel);
}

void process_radar(uint32_t now) {
    if (!g_head) return;
    HeadFrame f;
    if (!g_head->poll(f)) return;
    float dt = (now - g_last_frame_ms) / 1000.f;
    g_last_frame_ms = now;
    if (dt > 1.f) dt = 0.1f;
    rt::Detection d[rt::kMaxTargets];
    for (uint8_t i = 0; i < f.n; i++) d[i] = {float(f.t[i].x_mm), float(f.t[i].y_mm)};
    g_tracker.update(d, f.n, dt);
}

void build_and_send(uint32_t now) {
    rt::Packet p;
    p.pod_id = g_pod_id;
    p.seq = g_seq++;
    p.uptime_ms = now;
    p.mode = g_mode;
    p.battery_pct = rt::fuel_percent_u8(g_batt_pct);
    p.head_type = g_head_type;

    rt::Track tr[rt::Tracker::kMaxTracks];
    size_t n = g_tracker.confirmed_tracks(tr, rt::Tracker::kMaxTracks);
    if (n > rt::kMaxTargets) n = rt::kMaxTargets;
    for (size_t i = 0; i < n; i++) {
        rt::WireTarget& w = p.targets[i];
        w.x_mm = static_cast<int16_t>(tr[i].x_mm);
        w.y_mm = static_cast<int16_t>(tr[i].y_mm);
        w.speed_cms = static_cast<int16_t>(tr[i].vy_mmps / 10.f);
        float v = sqrtf(tr[i].vx_mmps * tr[i].vx_mmps + tr[i].vy_mmps * tr[i].vy_mmps);
        // LD2450 also reports stationary targets (could be clutter): moving -> MOVING, else MAYBE.
        bool moving = v > 100.f;
        w.kind = moving ? rt::TargetKind::MOVING : rt::TargetKind::MAYBE;
        uint8_t c = tr[i].confidence();
        w.confidence = moving ? c : (c > 50 ? 50 : c);
    }
    p.n_targets = static_cast<uint8_t>(n);

    // Blockage: the LD2450 gives no range profile, so only IMU + detection feed the logic.
    rt::BlockageFeatures bf;
    bf.imu_motion_mg = g_motion.motion_mg;
    bf.any_detection = n > 0;
    bf.has_profile = g_head && g_head->has_range_profile();
    bf.look_time_ms = now - g_still_since_ms;
    if (g_mode != rt::Mode::SCAN && bf.imu_motion_mg > blockage_config().hold_still_mg)
        g_still_since_ms = now;  // moving: the steady look starts over
    g_status = rt::evaluate_blockage(bf, blockage_config());

    uint16_t fl = 0;
    switch (g_status) {
        case rt::BlockStatus::BLOCKED: fl |= rt::Flag::BLOCKED; break;
        case rt::BlockStatus::NO_PENETRATION: fl |= rt::Flag::NO_PENETRATION; break;
        case rt::BlockStatus::HOLD_STILL: fl |= rt::Flag::HOLD_STILL; break;
        case rt::BlockStatus::TOO_CLOSE: fl |= rt::Flag::TOO_CLOSE; break;
        case rt::BlockStatus::NOTHING_DETECTED_LOW_CONF: fl |= rt::Flag::LOW_CONF; break;
        default: break;
    }
    if (g_batt_pct < kLowBattPct && g_fuel_ok) fl |= rt::Flag::LOW_BATT;
    if (charging()) fl |= rt::Flag::CHARGING;
    if (now - g_head_start_ms < kWarmupMs || g_head_type == rt::HeadType::NONE)
        fl |= rt::Flag::WARMING_UP;
    if (g_pairing_until) fl |= rt::Flag::PAIRING;
    if (g_head_type == rt::HeadType::NONE || (g_head_type == rt::HeadType::UWB))
        fl |= rt::Flag::LOW_CONF;  // no working radar: nothing seen means nothing
    p.flags = fl;

    uint8_t buf[rt::kMaxPacketLen];
    size_t len = rt::serialize(p, buf, sizeof buf);
    if (len) esp_now_send(kBroadcast, buf, len);
}

void update_led(uint32_t now) {
    bool blink = (now / 300) % 2;
    if (g_locate_until) {
        if (static_cast<int32_t>(now - g_locate_until) >= 0) g_locate_until = 0;
        else { led(blink, blink, blink); return; }  // white blink
    }
    if (g_pairing_until) {
        if (static_cast<int32_t>(now - g_pairing_until) >= 0) g_pairing_until = 0;
        else { led(false, false, blink); return; }  // blue blink
    }
    bool low_batt = g_fuel_ok && g_batt_pct < kLowBattPct;
    if (g_head_type == rt::HeadType::NONE || low_batt || g_status == rt::BlockStatus::BLOCKED ||
        g_status == rt::BlockStatus::NO_PENETRATION || g_status == rt::BlockStatus::TOO_CLOSE) {
        led(true, false, false);
    } else {
        led(false, true, false);  // running
    }
}

void handle_button(uint32_t now) {
    rt::ButtonEvent e = g_btn.update(digitalRead(PIN_BTN_OK) == LOW, now);
    if (e == rt::ButtonEvent::SHORT_PRESS) {
        set_mode(static_cast<rt::Mode>((static_cast<uint8_t>(g_mode) + 1) % rt::kModeCount));
        Serial.printf("mode -> %s\n", rt::mode_name(g_mode));
    } else if (e == rt::ButtonEvent::LONG_PRESS) {
        g_pairing_until = now + kPairingMs;
        Serial.println("pairing beacon");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    pinMode(PIN_BTN_OK, INPUT_PULLUP);
    pinMode(PIN_CHG_N, INPUT);
    led(true, false, false);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    g_imu_ok = imu_begin();
    battery_poll();
    Serial.printf("%s imu=%d fuel=%d batt=%.0f%% (%.2f V)\n", kFwVersion, g_imu_ok, g_fuel_ok,
                  g_batt_pct, g_batt_v);

    g_prefs.begin("rt-pod", false);
    g_channel = g_prefs.getUChar("chan", kDefaultChannel);
    if (g_channel < 1 || g_channel > 13) g_channel = kDefaultChannel;
    uint64_t mac = ESP.getEfuseMac();
    g_pod_id = static_cast<uint8_t>(mac >> 40);  // last MAC byte (little-endian efuse); 1/256 collision risk
    if (!radio_begin()) Serial.println("ESP-NOW init failed");

    pick_head();
    g_still_since_ms = millis();
}

void loop() {
    const uint32_t now = millis();
    static uint32_t last_imu = 0, last_batt = 0;

    process_radar(now);
    if (now - last_imu >= 10) {
        last_imu = now;
        imu_poll();
    }
    if (now - last_batt >= 5000) {
        last_batt = now;
        battery_poll();
    }
    handle_button(now);
    handle_command();
    handle_serial();
    apply_channel_switch();
    if (now - g_last_send_ms >= kSendPeriodMs) {
        g_last_send_ms = now;
        build_and_send(now);
    }
    update_led(now);
    delay(1);
}
