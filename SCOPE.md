# Radio Thing — Through-Wall Person Detector: Feasibility & Scope

Status: **Draft for decision** · 2026-10-19
Use cases: (1) search & rescue (SAR) "is anyone alive behind this wall/rubble", (2) Nerf wars.

> Prices and part availability are approximate as of my knowledge. Verify before ordering.

---

## 1. Bottom line

| Goal | Low-cost feasible? | Notes |
|---|---|---|
| Detect *someone* behind drywall / wood / interior doors | **Yes** | Main build target. |
| Detect a **still** person (breathing) through a non-metal wall | **Yes, with limits** | Device has to be still (tripod or braced on the wall). Takes 5–20 s per look. |
| Detect **heartbeat** through a wall | **Mostly no** | Chest movement from a heartbeat is ~0.2–0.5 mm. It works at short range with line of sight and nobody moving. Treat it as a stretch goal. |
| Count people precisely + show their 2D positions through a wall | **Partly** | 1–3 people who are moving and spread out: OK. Crowds, people close together, or people standing still: unreliable. |
| See through reinforced concrete, brick-and-rebar, stucco with wire mesh | **Weak to no** | Signal loss is too high for hobby power levels. |
| See through metal (steel door, foil-backed insulation, filing cabinet, car body) | **No (physics)** | **But we can reliably *detect* that we're blocked** and say so on screen. |
| Tell you when it can't see | **Yes** | See §5. This is cheap and should be in v1. |

**Recommendation:** Build in two stages. First, a cheap line-of-sight / thin-wall tracker (Nerf-grade, about $50). Then a UWB "breathing detector" module for SAR-style use on the same screen and microcontroller platform. Don't try to make one $100 device that does everything. It will do every job badly.

---

## 2. How it works (short version)

The device is a radar. It sends radio pulses or chirps and measures the echoes:

- **Range**: how long the echo took to come back.
- **Motion / Doppler**: how the echo's phase changes over time. Walking makes large changes. Breathing makes a slow change of about 0.1–0.5 Hz from 4–12 mm of chest movement. A heartbeat makes a tiny change of about 0.8–2 Hz.
- **Angle (where in the room)**: needs several receive antennas (MIMO) or several sensors spread apart. One antenna only gives you distance, not direction.

**Frequency choice decides whether it can see through walls at all:**

| Band | Through drywall/wood | Through brick/concrete | Resolution / size | Cheap modules |
|---|---|---|---|---|
| 2.4 / 5.8 GHz Doppler/FMCW | Good | Fair | Coarse, large antennas | Few complete ones; DIY RF is hard |
| **3–10 GHz UWB impulse** | **Good** | Fair (brick), poor (rebar concrete) | ~cm range, small | Novelda X4-based kits, Vayyar Walabot-class |
| 24 GHz | Fair (thin drywall, plastic, doors) | Poor | Good | HLK-LD2450 / LD2410 (~$5–20) |
| 60 / 77 GHz mmWave | Poor (thin drywall only, high loss) | No | Excellent, 2D/3D tracking | TI IWR6843, Infineon BGT60, Acconeer A121 |

Lower frequency gets through walls better. Higher frequency gives sharper positions. That trade-off drives the whole project.

---

## 3. Build tiers

### Tier A — "Nerf Scout" (~$40–70, a weekend)
- **Radar:** HLK-LD2450 (24 GHz). It tracks up to **3 targets** with X/Y position out to roughly 6 m and sends them over UART.
- **MCU + screen:** ESP32-S3 with a 2.8–3.5" SPI TFT (ILI9341/ST7796), or an all-in-one like a LilyGO T-Display-S3 / ESP32-2432S028 "CYD".
- **Power:** 18650 cell + charger board.
- **What you get:** a top-down "radar sweep" screen with dots. Works well with line of sight, through a closed door, or through thin drywall at short range. It mainly sees **moving** people. Stationary detection is weak.
- **Limits:** max 3 targets, no real wall penetration beyond thin interior walls, and ghost targets from reflections.

### Tier B — "Room Counter" (~$200–400)
- **Radar:** TI IWR6843AOP EVM (60 GHz, 3 TX / 4 RX, on-chip DSP, ships with people-counting/tracking firmware) **or** Infineon BGT60TR13C kit.
- **Host:** ESP32-S3 (just draws the targets) or a Raspberry Pi 5 / Zero 2 W if we do our own processing.
- **What you get:** solid counting and 2D/3D tracking of roughly 5–10+ people **in the same room or through glass/fabric**. Through-wall ability is poor.
- **Why bother:** best accuracy for "how many and where" when there's a line of sight or only a thin barrier. Also the best platform for learning the signal processing.

### Tier C — "SAR Breathing Finder" (~$400–900)
- **Radar:** UWB impulse radar module (Novelda X4-based dev kit such as X4M03/X4M200, or a Walabot-class 3–10 GHz kit if one is still sold).
- **Host:** Raspberry Pi 5 (Python/NumPy for the signal processing) + 5" DSI or HDMI screen, or Pi → ESP32 display.
- **Mount:** tripod or a foam pad held against the wall. **Holding it by hand ruins breathing detection.**
- **What you get:** through drywall, wood, plaster and single-layer brick, it finds **range bins with breathing-rate motion**. That means "1 person, ~3.2 m, breathing 14/min".
- **Position:** a single module gives distance only. To get 2D you need to (a) take two or three readings from different spots along the wall and combine them, or (b) use a multi-channel UWB array, which costs more and is much harder.
- **Counting:** reliable for 1–2 people at different distances. People at the same distance merge into one.

### Not feasible at low cost (reference points)
- NASA JPL FINDER (heartbeat through ~9 m of rubble), Camero Xaver, L3Harris Range-R. These cost from several thousand dollars to tens of thousands, use custom RF and antennas, and have years of algorithm work behind them. Range-R, the cheapest of them, mostly reports **presence + distance**, not a map. That's a good benchmark for what Tier C can realistically hit.
- MIT's WiTrack / RF-Capture research showed 2D/3D through-wall tracking with custom FMCW hardware around 5.5–7.25 GHz and a large antenna array. That's possible, but it's a grad-school project, not a low-cost build.

---

## 4. System architecture (common to all tiers)

```
 [Radar module] --UART/SPI/USB--> [Processor] --SPI/DSI--> [Screen]
       |                              |
   antenna(s)                 [Battery + buttons + buzzer]
```

Software pipeline:
1. **Acquire** frames (range profile or point cloud).
2. **Clutter removal**: subtract the static background (walls, furniture). Learn the background in the first few seconds.
3. **Detection**: CFAR threshold per range/angle bin.
4. **Vital-sign stage (Tier C)**: phase over 5–20 s per bin, band-pass 0.1–0.6 Hz (breathing) and 0.8–2.5 Hz (heartbeat), confidence score.
5. **Tracking**: cluster detections, then a Kalman/Hungarian tracker so the count stays stable from frame to frame.
6. **Display**: top-down plot, target count, a confidence for each dot, and a "BLOCKED / LOW CONFIDENCE" banner.

Modular firmware: keep each stage a pure function (`frame -> detections -> tracks -> render`). That lets us swap radars between tiers without rewriting the UI.

---

## 5. "Know when it can't see" (blockage detection)

These checks are cheap and dependable, and should be in v1:

| Check | Meaning |
|---|---|
| Very strong echo in the first ~0.3 m **and** almost nothing beyond it | Metal or foil right at the wall → **BLOCKED** |
| Noise floor behind the wall ≈ the radar's internal noise | Signal isn't getting through → **NO PENETRATION** |
| Sensor moving (IMU on the board, or the whole scene shifting together) | **HOLD STILL**; vital-sign readings are invalid |
| Receiver saturated / clipped samples | **TOO CLOSE / TOO REFLECTIVE**; back off or change the angle |
| Long period with no motion and no breathing peaks | Show "**No one detected (confidence: X)**", never a confident "empty" |

Key rule for SAR: **never show "room empty" as a certainty.** Show "nothing detected" plus a confidence level and the reason (blocked, low signal, too short a look).

A calibration step against a known wall type (drywall, brick, and so on) at startup improves all of these thresholds.

---

## 6. Legal / safety notes

- **Use FCC-certified radar modules and don't build your own transmitter.** 24 GHz and 57–71 GHz modules operate under Part 15 rules. In the US, purpose-built **UWB through-wall *imaging* systems (47 CFR 15.509/15.510) are limited to law enforcement, fire and rescue**. Buying a certified UWB dev kit and using it as intended is the safe route. Modifying power or antennas is not.
- **Export controls (ITAR/EAR)** only matter if this is sold, shared abroad, or marketed for defense. A personal hobby build is fine. Mention it before any "defense product" talk.
- Power levels are microwatts to milliwatts, so there's no RF exposure concern.
- Treat SAR output as a **hint, not a decision**. A false "empty" can get someone killed. The UI should be built around that.

---

## 7. Decisions (resolved 2026-10-19)

1. **One device** for both SAR and Nerf. → See §9 for the unified design.
2. **Walls:** ordinary building walls (drywall, wood, plaster). Brick/concrete are nice to have. No rubble. → Needs a lower-frequency **UWB** head, not mmWave.
3. **Cost:** as low as possible.
4. **Form factor:** handheld preferred, and it must also mount (tripod or wall). → Two modes, see §9.

Still open:
- **2D position through walls:** accept "distance + multi-spot readings" in v1? (Recommended.)
- **Budget ceiling:** the unified build lands around $150–400 depending on which UWB module we can buy (§9).

---

## 8. Suggested milestones

1. **M1 (Tier A):** ESP32-S3 + LD2450 + TFT. Live dots, count, battery. Test through an open door, a closed door, and drywall.
2. **M2:** add the blockage/confidence banner (test with a baking sheet or foil behind drywall).
3. **M3 (Tier C):** UWB module on a Pi. Detect breathing of a seated person through an interior wall at 1–5 m, and log detection rate vs. false alarms.
4. **M4:** combine readings from several spots for 2D estimates. Unified display.
5. **M5 (optional):** Tier B mmWave head for high-accuracy counting with line of sight.

Test protocol: run every milestone against a fixed scenario matrix (0/1/2/3 people × moving/still × wall type × distance) and record hit rate, false alarms, and count error. Without numbers, "works well" can't be checked.

---

## 9. Unified v1 design: "one device, two radars, two modes"

### Why two radars in one box
No single cheap radar does both jobs well:
- **24 GHz HLK-LD2450 (~$15):** gives 2D dots for up to 3 movers. Weak through walls.
- **UWB impulse radar (3–10 GHz):** gets through drywall and brick and can detect breathing. Distance only (1D).

The firmware **fuses** the two. The UWB head says *someone is 3.2 m away*. The LD2450 gives angles when the signal gets through, and multi-spot readings fill in when it doesn't. One screen, one UI, one battery.

### Hardware (v1)

| Block | Part | Approx cost | Notes |
|---|---|---|---|
| MCU + display | ESP32-S3 + 3.5" IPS SPI TFT (ST7796), or an all-in-one ESP32-S3 display board | $20–35 | Breathing DSP (FFT over ~100 range bins at ~20 fps) fits on an S3. |
| 2D radar | HLK-LD2450 (24 GHz) | $10–20 | UART. Nerf mode and line-of-sight tracking. |
| Through-wall radar | **Option 1:** Novelda X4-based module (X4M200 has respiration processing built in and outputs over UART; it launched at ~$80/1k). **Option 2:** X4 dev kit (X4M02/X4M03), raw frames over USB/UART. **Option 3 (fallback):** 24 GHz FMCW transceiver (e.g. Silicon Radar TRX_024) with our own DSP. | $80–300 | **Availability unverified.** X4 modules are 2017-era parts. Check stock at Mouser/Digi-Key/Arrow before committing. The firmware treats this as a swappable "radar head" driver. |
| Motion sensor | IMU (e.g. BMI270/MPU-6050) | $3–8 | Detects hand shake, gates breathing mode, shows HOLD STILL. |
| Power | 2× 18650 or 1× LiPo + USB-C charge/boost | $10–20 | |
| Mount | 1/4"-20 tripod insert + foam wall pad on the antenna face | $5 | One body, mounts either way. |
| Enclosure | 3D-printed, **plastic window over the antennas (no metal or paint)** | $5–10 | |
| **Total** | | **~$140–400** | Most of the spread is the UWB head. |

### Operating modes
1. **SCAN (handheld, free):** LD2450 + UWB motion. Shows moving people as dots and distance arcs, updated live. This is Nerf mode, and also a quick SAR sweep.
2. **LISTEN (handheld braced on the wall, or on a tripod):** you press the foam pad flat on the wall and keep it still. This is how commercial handheld through-wall radars are used. After 10–20 s it shows breathing detections: "1 person · 3.2 m · 14 breaths/min · confidence 78%". The IMU cancels the reading if you move.
3. **MULTI-SPOT (optional):** take a LISTEN reading at 2–3 spots along the wall. Intersecting the distance arcs gives a rough 2D position (about ±0.5–1 m).

### Expected performance (v1 targets; to be measured, not promised)

| Wall | Moving person | Still person (breathing) |
|---|---|---|
| Drywall / wood / interior door | ✅ ~6–8 m | ✅ ~4–5 m (LISTEN only) |
| Plaster/lath (wood lath) | ✅ | 🟡 shorter range |
| Single brick | 🟡 ~3–5 m | 🟡 ~2–3 m |
| Concrete block / poured concrete (no rebar) | 🟡 short range | 🔴 unreliable |
| Rebar concrete, metal lath stucco, foil insulation, metal doors | 🔴 → **BLOCKED** shown | 🔴 → **BLOCKED** shown |

Counting: v1 should be reliable for **0–3 people**. Show "3+" beyond that instead of guessing a number.

### Handheld reality check
Hand tremor is millimeters, about the same size as chest movement from breathing. That's why handheld LISTEN mode **has to be braced on the wall**, with the IMU checking it. Moving targets work fine in free-handheld SCAN mode. Heartbeat detection stays out of v1.

### Firmware layout (proposed)
```
/firmware
  radar_ld2450.*    UART parser -> targets[x,y,v]
  radar_uwb.*       driver interface (X4M200 | X4 raw | FMCW) -> range frames / vitals
  clutter.*         background subtraction (pure)
  detect.*          CFAR, breathing band-pass + confidence (pure)
  blockage.*        BLOCKED / NO PENETRATION / HOLD STILL logic (pure)
  fuse_track.*      merge LD2450 + UWB, Kalman tracker, count
  ui.*              LVGL radar view, banners, mode switching
```

### Revised milestones
1. **M1:** ESP32-S3 + TFT + LD2450 → SCAN mode with live dots (Nerf-ready, ~$50).
2. **M2:** IMU + blockage/confidence banners. Test with a baking sheet behind drywall.
3. **M3:** buy/confirm the UWB head, then LISTEN mode for breathing through drywall at 1–5 m. Log hit rate and false alarms.
4. **M4:** test brick/concrete, then MULTI-SPOT 2D, then the fused display.
5. **M5:** enclosure, tripod mount, battery life testing.

### Sources consulted
- Novelda X4M200 launch (respiration zone 0.4–5 m, ~$80/1k in 2017): https://www.digitalengineering247.com/article/novelda-offers-two-new-improved-modules-sensing-monitoring
- Breathing detection behind walls with monostatic UWB radar (EURASIP 2011): https://dx.doi.org/10.1186/1687-1499-2011-20
- Low-cost 24 GHz FMCW breathing detection with a Raspberry Pi (DTU thesis): https://upcommons.upc.edu/bitstreams/eaf25fce-867d-4f06-9fb0-4cd379fc1420/download
- Commercial reference, RETIA ReTWis 5 (breathing-based localization): https://www.plantautomation-technology.com/products/retia/retwis
