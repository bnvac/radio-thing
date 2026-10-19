// LovyanGFX configuration for the 3.5" ST7796 320x480 SPI TFT on the Radio Thing main board.
// Pins come from board_pins.h (fixed contract).
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "board_pins.h"

// Many 3.5" ST7796 IPS modules need colour inversion; flip if colours look negative.
// UNVERIFIED on real hardware.
#ifndef RT_LCD_INVERT
#define RT_LCD_INVERT true
#endif
#ifndef RT_LCD_RGB_ORDER_BGR
#define RT_LCD_RGB_ORDER_BGR false
#endif

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 panel_;
    lgfx::Bus_SPI bus_;
    lgfx::Light_PWM light_;

public:
    LGFX() {
        {
            auto cfg = bus_.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_SPI_SCK;
            cfg.pin_mosi = PIN_SPI_MOSI;
            cfg.pin_miso = PIN_SPI_MISO;
            cfg.pin_dc = PIN_LCD_DC;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {
            auto cfg = panel_.config();
            cfg.pin_cs = PIN_SPI_CS;
            cfg.pin_rst = PIN_LCD_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = true;
            cfg.invert = RT_LCD_INVERT;
            cfg.rgb_order = RT_LCD_RGB_ORDER_BGR;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            panel_.config(cfg);
        }
        {
            auto cfg = light_.config();
            cfg.pin_bl = PIN_LCD_BL;
            cfg.invert = false;  // high = on
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            light_.config(cfg);
            panel_.setLight(&light_);
        }
        setPanel(&panel_);
    }
};
