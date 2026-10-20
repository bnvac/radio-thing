# Radio Thing — main board, rev A (KiCad 9)

One PCB for both builds:

- **POD:** 82 × 50 mm. Snap off the button tab along the V-score.
- **DISPLAY:** 82 × 72 mm. Keeps the tab with 3 buttons (UP · OK · DOWN) centred right under the screen.

![board front](board-front.png)

## Status

| Step | State |
|---|---|
| Schematic | ✅ Generated, KiCad ERC: 2 warnings (unused bidirectional pins tied to GND, both expected) |
| Netlist | ✅ 328 pin assignments verified against `design.py`, 32 MCU pins verified against `firmware/include/board_pins.h` |
| PCB placement | ✅ All 84 footprints placed by block, every pad on its net. DRC: **0 electrical errors, 0 schematic-parity issues**. Only silkscreen overlap warnings remain. |
| PCB routing | ❌ **Not done.** 246 connections unrouted. See "Next: layout" below. |
| Datasheet checks | ✅ Done 2026-10-20. 13 fixes applied; 6 items need the real parts in hand (see below). |

**Don't order boards yet.** Routing and the 6 open items below come first.

## Files

| File | What |
|---|---|
| `design.py` | **Source of truth.** Every part, value, footprint, and pin→net connection, plus the build variant. Edit this, not the KiCad files. |
| `tools/gen_schematic.py` | Generates `radio-thing.kicad_sch` + `radio-thing.kicad_sym` (custom MAX17048 symbol). Pins come from KiCad's own symbol library, so pin numbers aren't hand-typed. |
| `tools/check_netlist.py` | Checks KiCad's exported netlist against `design.py` and the firmware pin map |
| `tools/gen_pcb.py` | Generates `radio-thing.kicad_pcb`: outline, 4 layers, GND planes on In1/In2, placement, nets from the netlist |
| `build.sh` | Runs everything above + ERC + DRC + BOM + PDF |
| `bom.csv` | BOM grouped by value/footprint/**Variant** (BOTH / POD / DISP / OPT) |
| `radio-thing-schematic.pdf` | Printable schematic |
| `reports/` | Latest ERC/DRC reports |

Rebuild: `KICAD_SYMBOL_DIR=… KICAD_FOOTPRINT_DIR=… ./build.sh` (KiCad 9 libraries; defaults to the system install).

> Once you start hand-routing in KiCad, **stop regenerating the PCB**, because `gen_pcb.py` would overwrite your routing. From then on, edit the schematic in KiCad and use *Tools → Update PCB from Schematic*. `design.py` is then documentation only.

## Block diagram

```
USB-C ─ F1 ─ U1 ESD ─┬─ VBUS ─ U2 BQ24074 (power path, ~1 A in, 445 mA charge)
                     │            ├─ VBAT ─ J2 LiPo 1S ── U3 MAX17048 fuel gauge (I2C)
                     │            └─ VSYS ─┬─ SW1 power ─ U4 TPS63001 ─ +3V3 (ESP32, sensors, display)
                     │                     ├─ U5 TPS61202 ─ FB1/C13 LC filter ─ +5V_HEAD ─ J4 radar head [POD]
                     │                     └─ BZ1 buzzer [DISP]
                     └─ USB D+/D- ─ U6 ESP32-S3-WROOM-1U (U.FL → SMA)
U6 ── I2C ── U7 ISM330DHCX (IMU) + U8 LIS2MDL (compass) + U3
U6 ── SPI ── J3 ST7796 3.5" display [DISP]  |  U9 DWM3000 UWB ranging [OPT, POD]
U6 ── UART1/UART2 + HEAD_ID + HEAD_EN ── J4 JST-GH 10p radar head [POD]
U6 ── 3 buttons UP/OK/DOWN [DISP] (OK on both builds), RGB LED, buzzer
```

## Build variants

| Variant | Fitted on | Parts |
|---|---|---|
| BOTH | every board | USB-C, charger, fuel gauge, 3V3, ESP32, sensors, LED, OK button |
| POD | pod | 5 V boost + filter, radar head connector J4, head ID |
| DISP | display | display socket + backlight switch, UP and DOWN buttons, buzzer. Change **R7 to 1k** for ~890 mA charging of a 2000 mAh cell. |
| OPT | not fitted by default | DWM3000 pod-to-pod ranging (shares SPI with the display, so never fit both) |

## Pin map (ESP32-S3-WROOM-1U-N8R8)

Defined in `firmware/include/board_pins.h`, the single source of truth, and checked automatically.

| GPIO | Net | GPIO | Net |
|---|---|---|---|
| 0 | BOOT | 15 | spare (TP2) |
| 1 | HEAD_ID (ADC) | 16 | BUZZER |
| 2 | HEADB_RX | 17 | CHG_N |
| 4 | BTN_UP | 18 | I2C_SCL |
| 5 | BTN_DOWN | 19/20 | USB D-/D+ |
| 6 | BTN_OK (wake) | 21 | LCD_BL |
| 7 | spare (TP1) | 38/47/48 | LED R/G/B |
| 8 | I2C_SDA | 39/40 | HEADA TX/RX |
| 9 | LCD_DC / DWM_IRQ | 41 | HEAD_EN |
| 10 | SPI_CS | 42 | HEADB_TX |
| 11/12/13 | MOSI/SCK/MISO | 43/44 | debug UART |
| 14 | LCD_RST / DWM_RST | 3, 45, 46, 35–37 | unused (strapping / PSRAM) |

GPIO7 and GPIO15 are spare (test pads TP1/TP2), e.g. for an IMU interrupt or touch later.

**Buttons (3, under the screen):** UP short = up/zoom in, hold = cycle mode · OK short = select/menu, hold 1.5 s = back (map: next pod), hold 3 s = sleep · DOWN short = down/zoom out. Button positions assume the display module is centred over the main area; check against your module.

## Datasheet check (2026-10-20)

Every value that came from memory was checked against the manufacturer datasheets: TI BQ2407x SLUS810N, TPS6300x SLVS520C, TPS6120x SLVS577E, Maxim MAX17048 19-6171 r7, ST ISM330DHCX r4, LIS2MDL r5, Qorvo DWM3000 rev B, Espressif WROOM-1/1U v1.8, Hi-Link LD2450 protocol v1.03, and lcdwiki MSP3525/3526.

**Fixed after the check**

| Part | Was | Now | Why |
|---|---|---|---|
| R6 BQ24074 ITERM | 15k | **3.0k** | 15k ends the charge at 50% of ICHG (the max allowed). 3.0k gives the normal 10%. |
| R4 BQ24074 ILIM | 1.5k (~1.07 A) | **1.78k (~0.9 A)** | Stay within a default USB-C source's 900 mA |
| L1 TPS63001 | 1.5 µH | **2.2 µH** | The value TI characterises. Loop compensation assumes it. |
| TPS63001 VINA | direct | **100 Ω + 100 nF RC** | TI reference circuit |
| C11 TPS61202 VOUT | 10 µF | **22 µF** | Datasheet minimum ≈ 5 × L = 11 µF, before DC-bias derating |
| C20 LIS2MDL C1 | 100 nF | **220 nF** | Required value (Table 2). Plus **10 µF** on Vdd (C25). |
| C5 MAX17048 VDD | 1 µF | 100 nF | Datasheet value |
| Display power | +3V3 | **switched VSYS (PWR_EN)** | The module has its own 3.3 V LDO, and powering it from 3V3 would starve the backlight. Using the switched rail keeps it fully off when SW1 is off. |
| Backlight drive | P-FET high-side switch | **GPIO → 100 Ω → LED pin** | On the MSP3525 the LED pin is a logic input with a pull-up, so the old switch could never turn the backlight off |
| J3 pin 14 | NC | **SD_CS pulled high** | The module's SD card shares MISO |
| Buzzer | from VSYS (4.4 V, ~8.5 Ω coil) | **+3V3 via 15 Ω, flyback on coil** | The transducer is rated 2–4 V and ≤150 mA mean |
| DWM3000 IRQ | none | **100k pull-down** | IRQ floats in sleep (option part only) |
| Radar head UART | direct | **100 Ω series ×4** | Detachable connector: limits back-powering and ESD |
| Button tab | no holes | **2 × M3 holes** | Button presses would otherwise bend the board at the V-score |

**Confirmed correct:**
- BQ24074 pinout, EN1/EN2 mode, ISET (445 mA / 890 mA), TMR 46.4k (6.2 h), TS 10k, /CE, caps.
- TPS63001 pinout, PS/SYNC, FB, current margin.
- TPS61202 pinout, PS, UVLO-to-VIN, load disconnect, 2.2 µH, 600 mA at 5 V.
- MAX17048 pinout, wiring and registers.
- ISM330DHCX pinout, unused-pin handling and registers.
- LIS2MDL pinout, CS and address.
- DWM3000 is pin-compatible with DWM1000.
- ESP32-S3 strapping, EN RC, PSRAM pins, module pin numbers.
- LD2450 needs 5 V / >200 mA with a 3.3 V UART at 256000 baud.

**Still open (check with the real parts):**
1. **Display:** buy the **lcdwiki MSP3525 (ST7796)** module; the board is wired for its pinout. MSP3520/3521 is an ILI9488 board with different touch pins. The module is 98 × 55.5 mm with the header centred on its short edge, so check J3 and the button positions against the module before layout.
2. **Buzzer:** KCG0601 has no datasheet online. Buy any 3 V, ~3 kHz, 6.5 mm external-drive magnetic transducer and confirm its footprint.
3. **MAX17048:** exposed-pad size (0.8 × 1.2 mm in the footprint) vs Maxim land pattern 90-0065. Also check stock/lifecycle.
4. **USBLC6-2SC6:** pinout confirmed from secondary sources only.
5. **LD2450 head board:** connector pin order and pitch (only shown as an image in Hi-Link's manual).
6. **DWM3000 (optional):** the RSTn pin is open-drain, so firmware must never drive it high.

## Next: layout (what's left to make it orderable)

1. **Power stages first, by hand.**
   - Keep the U4/L1/C6/C8/C9 and U5/L2/C10/C11 current loops tiny.
   - Stitch the thermal pads to the In1/In2 GND planes with vias.
   - Keep both converters away from J4 (radar head) and the U6 U.FL connector.
2. **USB D+/D-** as a 90 Ω differential pair, J1 → U1 → U6, short.
3. **Radar head:** route the +5V_HEAD rail through FB1 and C13 right at J4. Keep the UART pairs away from the switch nodes (BB_L1/BB_L2/HB_L).
4. **Stackup:** L1 parts/signals, L2 solid GND, L3 power pours + signals, L4 signals.
5. Route everything else (Freerouting can do the low-speed nets), then clean up the silkscreen.
6. **Fab:** JLCPCB 4-layer, 1.6 mm, ENIG. Ask for a **V-cut at y = 50 mm** (marked on User.1). Assembly with the BOM variant you're building.
7. **Mechanical:** M3 holes at the corners, an SMA bulkhead pigtail from the U6 U.FL, and display standoffs.
