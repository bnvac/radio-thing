# Radio Thing — what to do next (rev A)

Everything that can be designed without the real parts in hand is done: scope, schematic, routed 4-layer board, manufacturing files, firmware, and the display enclosure. What's left needs a person: buying parts, measuring them, ordering, building and testing.

## 1. Buy the parts you need to measure (≈ $40, do this first)

| Part | Why first |
|---|---|
| **lcdwiki MSP3525** 3.5" ST7796 SPI module (not MSP3520/3521, which is a different chip) | Its thickness and visible-area position set the case dimensions |
| **HLK-LD2450** 24 GHz radar | Check its connector pin order before the radar head board |
| 12 × 12 mm tactile switches (e.g. Omron B3F-4055) | Height sets the button cap length |
| 2000 mAh 1S LiPo with JST-PH 2.0 lead (~60 × 50 × 9 mm) | **Check the lead polarity**: pin 1 must be + on our board. Many hobby packs are reversed |
| 3 V, ~3 kHz, ~6.5 mm magnetic buzzer (external drive) | Confirm it matches the board footprint |

## 2. Measure and update (≈ 30 min)

- **Screen:** thickness, glass thickness, and the visible-area offset → `enclosure/radio_thing_display.scad` (`[MEASURE]` values).
- **Switch height:** → `SW_H` in the same file.
- **Battery thickness:** → `BATT_T`.
- **Buzzer and LD2450 connector:** tell me what you find and I'll update the board/firmware if anything differs.

## 3. Review the board (≈ 15 min)

Open `hardware/radio-thing.kicad_pcb` in KiCad 9. Run DRC: you should see only 6 silkscreen-at-edge warnings. Glance at the three power-supply areas (U2 charger, U4 3.3 V, U5 5 V): parts should be tight around each chip.

## 4. Order boards (JLCPCB)

1. Upload `hardware/fab/radio-thing-revA-gerbers.zip`: 4 layers, 1.6 mm, 1 oz, ENIG.
2. For assembly, upload `bom-POD.csv` + `cpl-POD.csv` (pod build) or `bom-DISPLAY.csv` + `cpl-DISPLAY.csv` (screen build). Pick LCSC parts for each line and check rotations in their 3D preview.
3. Through-hole parts (12 mm buttons, display socket, debug header): hand-solder them or pay for THT assembly.
4. Order **at least 2 boards**: one built as POD, one as DISPLAY.

## 5. Print the case

Print `enclosure/stl/` (front, lid, caps) in PETG/ASA. Settings and screws are in `enclosure/README.md`. Print one button cap first to check the fit.

## 6. Bring-up (when boards arrive)

1. **Before the battery:** power from USB only. Check 3.3 V on the debug header (pin 1) before connecting anything else.
2. **Flash:** `cd firmware && pio run -e display -t upload` (and `-e pod` for the pod). The USB port shows up as a serial device.
3. **Check sensors over the USB console:** IMU and battery-gauge readings. Their register settings were checked against the datasheets but have never run on hardware.
4. **Pod:** plug in the LD2450 head and walk in front of it. Dots should appear on the screen unit within a few seconds.
5. Then the battery, then the case.

## 7. Decisions still open (tell me when you're ready)

- **Through-wall (UWB) radar head:** which module, since the Novelda X4 parts may no longer be sold. This decides the radar head board and the firmware driver.
- **Pod enclosure:** drone/GoPro mount, magnet, VHB adhesive. I'll design it once the radar head is picked.
- **Rev B:** pod-to-pod UWB ranging (DWM3000), and anything that comes out of testing.
