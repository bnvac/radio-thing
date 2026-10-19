// Radio Thing main board (rev A) — ESP32-S3-WROOM-1U-N8R8 pin map.
// Single source of truth shared with hardware/PINMAP.md and hardware/gen_schematic.py.
// Same PCB for POD and DISPLAY builds; pins marked [POD]/[DISP] are only populated in that build.
// Unavailable on N8R8: GPIO26-32 (flash), GPIO35-37 (octal PSRAM). Strapping: GPIO0, 3, 45, 46 (left unused except BOOT).
#pragma once

// --- Boot / USB / debug ---
#define PIN_BOOT        0   // BOOT button (strap, 10k pull-up)
#define PIN_USB_DN     19   // native USB D-
#define PIN_USB_DP     20   // native USB D+
#define PIN_DBG_TX     43   // UART0 TXD0 -> debug header
#define PIN_DBG_RX     44   // UART0 RXD0 <- debug header

// --- I2C bus: MAX17048 fuel gauge (0x36), ISM330DHCX IMU (0x6A), LIS2MDL magnetometer (0x1E) ---
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL    18

// --- SPI bus: [DISP] ST7796 3.5" display  /  [POD] optional DWM3000 UWB ranging ---
#define PIN_SPI_SCK    12
#define PIN_SPI_MOSI   11
#define PIN_SPI_MISO   13
#define PIN_SPI_CS     10   // LCD_CS [DISP] / DWM_CS [POD]
#define PIN_LCD_DC      9   // [DISP]
#define PIN_DWM_IRQ     9   // [POD]  (shares pin with LCD_DC; never both fitted)
#define PIN_LCD_RST    14   // [DISP]
#define PIN_DWM_RST    14   // [POD]
#define PIN_LCD_BL     21   // [DISP] backlight enable, PWM-capable (high = on)

// --- Buttons (active low, use INPUT_PULLUP) ---
#define PIN_BTN_UP      4   // [DISP]
#define PIN_BTN_DOWN    5   // [DISP]
#define PIN_BTN_OK      6   // [DISP]+[POD] RTC GPIO -> deep-sleep wake; the pod's single button
#define PIN_BTN_BACK    7   // [DISP]
#define PIN_BTN_MODE   15   // [DISP]

// --- Indicators ---
#define PIN_BUZZER     16   // [DISP] low-side MOSFET, drive with LEDC PWM (~2.7 kHz)
#define PIN_LED_R      38   // active high
#define PIN_LED_G      47
#define PIN_LED_B      48

// --- Power ---
#define PIN_CHG_N      17   // BQ24074 /CHG (open drain, 10k pull-up): LOW = charging

// --- Radar head connector J4 (JST-GH 10p) [POD] ---
#define PIN_HEAD_EN    41   // HIGH = enable TPS61202 5 V head rail (100k pull-down)
#define PIN_HEADA_TX   39   // ESP TX -> head A RX  (UART1)  e.g. HLK-LD2450 @ 256000 baud
#define PIN_HEADA_RX   40   // ESP RX <- head A TX
#define PIN_HEADB_TX   42   // ESP TX -> head B RX  (UART2)  e.g. UWB head
#define PIN_HEADB_RX    2   // ESP RX <- head B TX
#define PIN_HEAD_ID     1   // ADC1_CH0, 10k pull-up to 3V3; head fits a resistor to GND

// Head ID resistor (on head board, to GND) -> expected voltage with 10k pull-up
//   none  -> ~3.3 V : no head
//   4.7k  -> ~1.05 V: HEAD_24  (LD2450 only)
//   10k   -> ~1.65 V: HEAD_UWB (UWB module only)
//   22k   -> ~2.27 V: HEAD_DUAL (LD2450 on A + UWB on B)
