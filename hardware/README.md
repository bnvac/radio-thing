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
| Datasheet checks | ⚠ See "Verify before ordering". Parts of the design come from memory, not checked datasheets. |

**Don't order boards yet.** Routing and the checks below come first.

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

## Verify before ordering ⚠

These values are reasonable but come from memory, not a datasheet read in this session. Check each one:

1. **BQ24074**
   - EN1/EN2 truth table: EN2=1, EN1=0 should give the ILIM-resistor mode.
   - TMR resistor (46.4k), ITERM resistor (15k).
   - K_ISET = 890 and K_ILIM = 1550 constants.
2. **TPS61202**: polarity of the PS pin, and whether UVLO can be tied to VIN.
3. **TPS63001**: PS/SYNC high = forced PWM.
4. **MAX17048**: the custom symbol pinout (1 CTG, 2 CELL, 3 VDD, 4 GND, 5 ALRT, 6 QSTRT, 7 SCL, 8 SDA, EP) and the TDFN-8 2×2 land pattern.
5. **ISM330DHCX**: how unused SDx/SCx/OCS_AUX/SDO_AUX should be handled.
6. **LIS2MDL**: C1 capacitor value (100 nF used).
7. **DWM3000**: that the DWM1000 footprint is pin-compatible, and the VDDAON/WAKEUP wiring. It's optional and not fitted by default.
8. **Display module**: this assumes the common red 3.5" ST7796 SPI module, 14-pin header with VCC, GND, CS, RST, DC, MOSI, SCK, LED, MISO, then touch.
   - **Measure your module.** The header position decides where J3 goes so the screen sits over the board.
   - Check the backlight current (Q1 is rated for it).
9. **LiPo connector polarity**: JST-PH pin 1 = +. Many hobby packs are reversed.
10. **Buzzer**: KCG0601 voltage rating vs VSYS (3.5–4.4 V).
11. **Radar heads**: HLK-LD2450 runs from 5 V with 3.3 V UART, and so does the UWB module. Confirm both.

## Next: layout (what's left to make it orderable)

1. **Power stages first, by hand.**
   - Keep the U4/L1/C6/C8/C9 and U5/L2/C10/C11 current loops tiny.
   - Stitch the thermal pads to the In1/In2 GND planes with vias.
   - Keep both converters away from J4 (radar head) and the U6 U.FL connector.
2. **USB D+/D-** as a 90 Ω differential pair, J1 → U1 → U6, short.
3. **Radar head:** route the +5V_HEAD rail through FB1 and C13 right at J4. Keep the UART pairs away from the switch nodes (BB_L1/BB_L2/HB_L).
4. **Stackup:** L1 parts/signals, L2 GND, L3 GND (or short power pours), L4 signals.
5. Route everything else (Freerouting can do the low-speed nets), then clean up the silkscreen.
6. **Fab:** JLCPCB 4-layer, 1.6 mm, ENIG. Ask for a **V-cut at y = 50 mm** (marked on User.1). Assembly with the BOM variant you're building.
7. **Mechanical:** M3 holes at the corners, an SMA bulkhead pigtail from the U6 U.FL, and display standoffs.
