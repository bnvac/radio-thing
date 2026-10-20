// Radio Thing DISPLAY firmware: ESP-NOW receiver, radar map UI, buttons, buzzer, battery.
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "app.h"
#include "board_pins.h"
#include "debounce.h"
#include "fuel.h"
#include "pods.h"
#include "protocol.h"
#include "settings.h"
#include "ui.h"

namespace {

constexpr uint32_t kFrameMs = 100;       // UI refresh ~10 Hz
constexpr uint32_t kSleepHoldMs = 3000;  // OK held this long = deep sleep
constexpr uint32_t kBackHoldMs = 1500;   // OK released after this long = BACK
constexpr int kBuzzChannel = 4;
constexpr uint32_t kChirpCooldownMs = 1500;

LGFX g_tft;
Settings g_settings;
PodTable g_pods;
Ui g_ui(g_tft, g_settings, g_pods);

struct RxItem {
    uint8_t len;
    uint8_t data[rt::kMaxPacketLen];
};
QueueHandle_t g_rx_queue;

const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 3 physical buttons mapped onto the UI's 5 logical actions:
//   UP   short -> UP,   hold 1.5 s -> MODE
//   OK   short -> OK,   released after >= 1.5 s -> BACK,   hold 3 s -> deep sleep
//   DOWN short -> DOWN
struct ButtonDef {
    uint8_t pin;
    Button id;
    rt::Debouncer deb;
};
ButtonDef g_buttons[] = {
    {PIN_BTN_UP, Button::UP, rt::Debouncer(25, 1500)},
    {PIN_BTN_OK, Button::OK, rt::Debouncer(25, kSleepHoldMs)},
    {PIN_BTN_DOWN, Button::DOWN, rt::Debouncer(25, 1500)},
};

float g_batt_pct = 0, g_batt_v = 0;
uint32_t g_chirp_off_at = 0, g_last_chirp = 0;

void on_recv(const uint8_t*, const uint8_t* data, int len) {
    if (len <= 0 || len > static_cast<int>(rt::kMaxPacketLen)) return;
    if (rt::packet_type(data, len) != rt::PacketType::DATA) return;
    RxItem it;
    it.len = static_cast<uint8_t>(len);
    memcpy(it.data, data, len);
    xQueueSend(g_rx_queue, &it, 0);  // drop if full; validation happens in loop()
}

void radio_set_channel(uint8_t ch) {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

bool radio_begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    radio_set_channel(g_settings.channel);
    if (esp_now_init() != ESP_OK) return false;
    esp_now_register_recv_cb(on_recv);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcast, 6);
    peer.channel = 0;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, n) != n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

void battery_poll() {
    uint8_t b[2];
    if (i2c_read(rt::kMax17048Addr, rt::kMax17048RegSoc, b, 2)) g_batt_pct = rt::fuel_soc_percent(b[0], b[1]);
    if (i2c_read(rt::kMax17048Addr, rt::kMax17048RegVcell, b, 2)) g_batt_v = rt::fuel_vcell_volts(b[0], b[1]);
}

void deep_sleep() {
    g_ui.toast("Sleeping...", millis(), 1000);
    g_ui.render(millis());
    delay(600);
    app_apply_brightness(0);
    g_tft.sleep();
    ledcWrite(kBuzzChannel, 0);
    // Wait for OK release, otherwise the wake level is already active.
    while (digitalRead(PIN_BTN_OK) == LOW) delay(10);
    delay(50);
    pinMode(PIN_LCD_BL, OUTPUT);  // keep the backlight off while asleep
    digitalWrite(PIN_LCD_BL, LOW);
    gpio_hold_en(static_cast<gpio_num_t>(PIN_LCD_BL));
    gpio_deep_sleep_hold_en();
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(PIN_BTN_OK));  // keep the pull-up powered in sleep
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_BTN_OK));
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_BTN_OK), 0);
    esp_deep_sleep_start();
}

void buzzer_update(uint32_t now) {
    if (g_chirp_off_at && static_cast<int32_t>(now - g_chirp_off_at) >= 0) {
        ledcWrite(kBuzzChannel, 0);
        g_chirp_off_at = 0;
    }
}

void process_rx(uint32_t now) {
    RxItem it;
    while (xQueueReceive(g_rx_queue, &it, 0) == pdTRUE) {
        rt::Packet p;
        if (rt::deserialize(it.data, it.len, p) != rt::ParseResult::OK) continue;
        if (g_pods.on_packet(p, now)) {
            if (static_cast<int32_t>(now - g_last_chirp) > static_cast<int32_t>(kChirpCooldownMs)) {
                app_chirp(false);
                g_last_chirp = now;
            }
        }
    }
    g_pods.expire(now);
}

}  // namespace

// ---- app.h hooks ------------------------------------------------------------------------
void app_send_command(const rt::Command& c) {
    uint8_t buf[rt::kCommandLen];
    size_t n = rt::serialize(c, buf, sizeof buf);
    for (int i = 0; n && i < 3; i++) {  // broadcast has no ACK: repeat a few times
        esp_now_send(kBroadcast, buf, n);
        delay(15);
    }
}

void app_apply_brightness(uint8_t pct) { g_tft.setBrightness(map(pct, 0, 100, 0, 255)); }

void app_set_channel(uint8_t ch) {
    rt::Command c;
    c.cmd = rt::CmdType::SET_CHANNEL;
    c.arg = ch;
    app_send_command(c);  // sent on the old channel so pods hear it
    delay(350);
    radio_set_channel(ch);
}

void app_chirp(bool force) {
    if (!g_settings.buzzer && !force) return;
    ledcWriteTone(kBuzzChannel, 2700);
    g_chirp_off_at = millis() + 80;
}

float app_battery_pct() { return g_batt_pct; }
float app_battery_volts() { return g_batt_v; }

void setup() {
    Serial.begin(115200);
    for (auto& b : g_buttons) pinMode(b.pin, INPUT_PULLUP);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    digitalWrite(PIN_LED_G, HIGH);
    ledcSetup(kBuzzChannel, 2700, 8);
    ledcAttachPin(PIN_BUZZER, kBuzzChannel);
    ledcWrite(kBuzzChannel, 0);

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        gpio_hold_dis(static_cast<gpio_num_t>(PIN_LCD_BL));
        rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_OK));
    }

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    battery_poll();

    g_settings.load();
    g_rx_queue = xQueueCreate(8, sizeof(RxItem));
    if (!radio_begin()) Serial.println("ESP-NOW init failed");
    g_ui.begin();
    app_chirp(true);
}

void loop() {
    const uint32_t now = millis();
    static uint32_t last_frame = 0, last_batt = 0;

    for (auto& b : g_buttons) {
        rt::ButtonEvent e = b.deb.update(digitalRead(b.pin) == LOW, now);
        if (e == rt::ButtonEvent::SHORT_PRESS) {
            Button act = b.id;
            if (b.id == Button::OK && b.deb.last_hold_ms() >= kBackHoldMs) act = Button::BACK;
            g_ui.on_button(act, now);
        } else if (e == rt::ButtonEvent::LONG_PRESS) {
            if (b.id == Button::OK) deep_sleep();
            else if (b.id == Button::UP) g_ui.on_button(Button::MODE, now);
        }
    }
    process_rx(now);
    buzzer_update(now);
    if (now - last_batt >= 5000 || last_batt == 0) {
        last_batt = now ? now : 1;
        battery_poll();
    }
    if (now - last_frame >= kFrameMs) {
        last_frame = now;
        g_ui.render(now);
    }
    delay(2);
}
