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

---

## 10. Mounting anywhere: drone, vehicle, adhesive (added 2026-10-19)

### Architecture change: split into **Sensor Pod** + **Display Unit**
Once the sensor goes on a drone or a wall, the screen can't be on the same box. So:

```
 [Sensor Pod]  ──wireless──>  [Display Unit]
  radars + ESP32-S3             ESP32-S3 + 3.5" screen + battery
  + IMU + battery               (handheld, wrist, or phone app later)
  + universal mount plate
```
- **Handheld use** = the pod clips onto the back of the display unit, so it's one device again.
- **Link:** ESP-NOW (ESP32 to ESP32, no router, ~100 m+ line of sight) by default. LoRa as an option for long range or through buildings. LoRa is low data, but we only send a target list (a few bytes per person), not raw radar data.
- One display unit can show **several pods** at once (drone + 2 sticky pods) on one map.

### Universal mount plate (one pod, many mounts)
| Mount | How | Notes |
|---|---|---|
| Tripod / handheld | 1/4"-20 insert | Already planned |
| Drone / helmet / vehicle | GoPro-style 2-prong + vibration-damping balls | Keep pod **< 100 g** so small drones can carry it |
| Magnetic | Neodymium pad | Vehicles, steel doors (but it **can't see through** the steel it's stuck to) |
| **Adhesive "sticky pod"** | 3M VHB pad, or suction cup for glass | Place on a wall and leave it. Sleeps and wakes every N seconds to save battery. Wall contact makes it **the most accurate mode** (see below). |

### Moving platforms: the main problem
Breathing moves the chest ~4–12 mm. A hovering drone wobbles by **centimeters**, and a vehicle shakes more than that. Raw readings from a moving platform will be full of fake "people". What works for each platform:

| Platform | Moving people | Still people (breathing) | Best tactic |
|---|---|---|---|
| Sticky pod on wall | ✅ best | ✅ **best** | Stuck to the wall = zero relative motion |
| Handheld braced on wall | ✅ | ✅ | LISTEN mode |
| Drone **perched/landed** (ledge, roof, sill) | ✅ | ✅ | Land, rotors off, listen, take off |
| Drone hovering near wall | 🟡 with compensation | 🔴 mostly no | Use as a moving scanner (see SAR below) |
| Ground vehicle stopped | ✅ | 🟡 engine off | Stop, listen |
| Vehicle/drone moving | 🟡 | 🔴 | Synthetic aperture imaging of the scene |

Drone-specific issues:
- **Propellers** create strong radar "micro-Doppler" clutter at the blade frequency. Point the radar away from the rotors, add an absorber/shield behind the antenna, and notch-filter the blade frequencies (read from motor RPM or measured).
- **Drone radios:** check the drone's video/control bands don't overlap the UWB head's band. Common 2.4/5.8 GHz links are outside the X4's ~7–9 GHz bands, but verify for the actual module.
- **Weight/legal:** FAA rules apply (registration over 250 g total). Only fly or stick pods on buildings where you have permission.

---

## 11. How to make it more accurate (ranked by payoff per dollar)

1. **Wall-echo motion compensation (big win, free in software).** The wall is the strongest echo and it doesn't move. Any change in the wall echo's phase is *our* movement. Subtract it from every range bin and most of the hand and drone wobble cancels out. Combine this with the IMU for larger movements.
2. **Couple to the wall.** Antenna flat against the wall (foam pad or adhesive) removes the strong air-to-wall reflection and a lot of clutter. This is why sticky pods and braced LISTEN beat everything else.
3. **Look longer.** Averaging over 10–30 s instead of 3 s greatly reduces false breathing detections. Show the confidence rising on screen while it listens.
4. **More viewpoints = real 2D positions.** Combine distance arcs from 2–3 sticky pods, or from several spots along the wall, to triangulate. A drone or vehicle **moving along the wall** acts as a large virtual antenna (synthetic aperture radar). With the platform position known from IMU + optical flow, a static scene can be imaged in 2D.
5. **Calibrate for the wall.** Radio waves slow down inside brick and concrete, which makes people look farther away than they are. Measure the front and back wall echoes to estimate wall thickness and material, then correct the distance. This also feeds the BLOCKED / weak-signal logic.
6. **Better antennas.** A directional patch-array or horn antenna with an absorber behind it reduces clutter from the operator, the drone, and the side walls. This is the main hardware upgrade after the module itself.
7. **Smarter tracking.** Only confirm a person after M detections out of N frames. Use a Kalman tracker. Reject multipath "ghosts", which show up as a copy of a real person *farther away along the same line*.
8. **Add a thermal camera for line-of-sight moments.** An MLX90640 (~$50) can't see through walls, but through open doors and windows it confirms "radar target = human", and it helps a lot for Nerf.
9. **Build a test dataset.** Record labeled runs (0/1/2/3 people × still/moving × wall type × platform) and tune thresholds against them. Later, a small classifier (person vs. fan vs. curtain vs. pet) can run on the ESP32-S3.
10. **Upgrade path:** MIMO UWB or a multi-channel FMCW head for true 2D from a single position. More cost and complexity, so only once v1 numbers show it's needed.

### Updated milestones
1. **M1:** Pod (ESP32-S3 + LD2450) + Display unit over ESP-NOW. Live dots. Handheld clip.
2. **M2:** IMU + blockage/confidence + **wall-echo motion compensation**.
3. **M3:** UWB head → LISTEN mode. Sticky pod with sleep/wake.
4. **M4:** Multi-pod map, triangulation, wall-material calibration.
5. **M5:** Drone mount: vibration isolation, rotor notch filter, perch-and-listen workflow.
6. **M6 (stretch):** synthetic aperture scan from a moving drone or vehicle.

---

## 12. Decisions (2026-10-19, round 3)

- **Link: ESP-NOW** between pod and display unit. LoRa dropped from v1.
- **Drone: platform-agnostic** (FPV quads and DJI-style camera drones).

What these mean for the design:
- **Self-contained pod.** It has its own battery and takes no power or telemetry from the drone. DJI doesn't expose motor or flight data to payloads, so rotor-noise filtering has to **measure blade frequencies from the radar data itself** rather than read RPM.
- **Weight budget:** aim for **≤ 60 g** so a DJI Mini-class drone can still fly safely. FPV quads can carry the full ≤ 100 g pod. Plan a "lite" pod (LD2450 + ESP32 + IMU + small LiPo) and a "full" pod (adds the UWB head).
- **Mount:** GoPro-style 2-prong + damping balls is the common standard on both drone types. Velcro/zip-tie straps as a fallback.
- **2.4 GHz coexistence:** ESP-NOW runs on 2.4 GHz, the same band as many FPV control links (ExpressLRS 2.4, TBS Tracer) and DJI's links. To avoid hurting control range:
  - Keep packets small: target lists only, at ≤ 10 Hz.
  - Fix the Wi-Fi channel, configurable from the display unit.
  - Mount the pod antenna away from the drone's receiver antennas.
  - **Range test with the drone's control link active before any real flight.**
  - ESP32 long-range Wi-Fi mode can extend ESP-NOW range if needed.
- **Safety:** pod failure must never affect the drone. It's electrically separate and only mechanically attached.

---

## 13. Power: LiPo charging circuit (pod + display unit)

### Requirements
- 1-cell LiPo (3.0–4.2 V), USB-C charging.
- **Runs while charging** (power-path). A plain TP4056 doesn't do this cleanly.
- Battery protection (over-discharge, over-current, short).
- Battery % on the screen.
- **Clean power for the radars.** Switching noise on the radar supply shows up as false targets.

### Recommended circuit (custom build)
```
USB-C ─> [BQ24074 charger + power-path] ─> SYS rail (3.5–4.4 V)
                 │                              ├─> [3.3 V buck-boost TPS63001/TPS63020] ─> ESP32-S3, IMU, screen
            [1S LiPo + protection]              └─> [5 V boost TPS61023] ─> [LC filter / ferrite + 47–100 µF] ─> LD2450 (5 V)
                 │                                                            (UWB head on its own filtered rail)
            [MAX17048 fuel gauge] ─I2C─> ESP32-S3 (battery %)
```
| Part | Job | Why this one |
|---|---|---|
| **BQ24074** (Adafruit #4755 breakout exists) | Charger, 1.5 A max, power-path, thermistor input | Device runs from USB while charging. Safe charge termination. |
| **TPS63001/TPS63020** buck-boost | Steady 3.3 V from 3.0–4.2 V | Uses the whole battery. An LDO drops out around 3.5 V. |
| **TPS61023** (or similar low-ripple boost) | 5 V for the LD2450 | Avoid the cheap MT3608, which is noisy. Add an LC/ferrite filter. |
| **MAX17048** | Fuel gauge | Accurate %, not just a voltage guess. |
| LiPo with built-in protection PCB, JST-PH 2.0 | Battery | Check connector polarity. Brands don't agree. |
| Slide switch on battery output, or button soft-latch | Power on/off | Pod: hard switch. Display: soft power on the OK button. |

### Cheaper / faster option
- **Pod (lite):** **Seeed XIAO ESP32S3** has a LiPo charger built in and is tiny and light. Add a 5 V boost for the LD2450.
- **Display:** a TP4056 USB-C module **with protection** (DW01) + a load-sharing MOSFET/diode circuit + a buck-boost. Fine for prototypes. Move to BQ24074 for v1.
- Some ESP32-S3 display boards (e.g. LilyGO T-Display-S3) have a charger and 2 buttons built in, but the screen is small (1.9"). **Check the charger and pinout for the specific board before buying.**

### Battery sizing (estimates; measure in M5)
| Unit | Typical draw | Cell | Approx runtime | Cell weight |
|---|---|---|---|---|
| Lite pod | ~150–200 mA | 500 mAh | ~2–3 h | ~10–12 g |
| Full pod | ~250–400 mA | 800–1000 mAh | ~2–3 h | ~18–22 g |
| Display unit | ~200–300 mA (backlight dominates) | 1500–2000 mAh | ~5–8 h | ~30–40 g |
| Sticky pod (sleep/wake duty cycle) | avg ~20–40 mA | 1000 mAh | ~1 day | |

Rule: **never let the drone's battery power the pod** (see §12).

---

## 14. Controls: tactile buttons + menus

### Display unit (glove-friendly)
| Button | Function |
|---|---|
| **UP / DOWN** | Move through menus. On the map: zoom. |
| **OK** (hold 2 s = power on/off) | Select. On the map: start LISTEN. |
| **BACK** | Back. On the map: switch between pods. |
| **MODE** (side) | Cycle SCAN → LISTEN → MULTI-SPOT |

- Parts: 12×12 mm tactile switches with caps (big enough for gloves), **or** one EC11 rotary encoder with push + BACK button (fewer holes, easy with gloves). LVGL supports both natively (keypad / encoder input).
- Wiring: each button to its own GPIO with the internal pull-up, to GND. 20–30 ms software debounce. **OK on an RTC-capable GPIO** so it can wake the ESP32-S3 from deep sleep.
- Optional buzzer/vibration motor: a buzz when a person is detected, so you don't have to stare at the screen.

### Pod
- **One button:** short press = cycle mode, long press = pair with display unit, very long = power off (if soft power is used).
- **RGB status LED:** green = running, blue blink = pairing, red = BLOCKED / low battery.

### Menu tree (v1)
```
Main map
├─ Mode: SCAN / LISTEN / MULTI-SPOT
├─ Pods: list · pair new · rename · locate (blink LED)
├─ Wall: auto-calibrate · material preset (drywall / brick / concrete)
├─ Settings: ESP-NOW channel · brightness · units (m/ft) · buzzer · Nerf mode
└─ Status: battery (display + each pod) · signal strength · firmware
```

---

## 15. More accuracy upgrades (on top of §11)

**Hardware**
1. **Clean radar power** (§13): separate filtered rail, star ground. This is a cheap and real fix for phantom targets.
2. **Pod-to-pod UWB ranging (DWM3000 module, ~$20–30 each):** pods measure their distance to each other to ~10 cm. Triangulation from multiple pods then uses *known* pod positions instead of guesses, which is the biggest accuracy gain for multi-pod maps. ⚠ It shares the UWB band with the X4 radar head, so **time-share them** (range pods, then radar, never both at once).
3. **Heading for every pod:** IMU with a magnetometer, plus a "point the pod at the wall and press OK" alignment step. Compass readings are unreliable near steel, so the manual alignment is the fallback.
4. **Directional antenna + absorber backing** (§11), and keep metal (battery, screws) behind the antenna, never in front.

**Calibration**
5. **Corner-reflector check:** a ~$5 aluminum corner reflector at a measured distance (e.g. 2.00 m) checks range offset and angle offset each time the hardware changes. Store the offsets in flash.
6. **Mounting-angle offset:** LD2450 angle readings shift if the module is tilted in the case, so calibrate once after assembly.
7. **Warm-up:** radar readings drift for the first minute or two. Show "warming up" and hold off on LISTEN until it's stable.

**Software**
8. **Fuse both radars:** only show a confident dot when the LD2450 angle and the UWB distance agree. Show a single-sensor hit as a fainter "maybe".
9. **Timestamps on everything:** sync pod clocks over ESP-NOW so multi-pod data lines up in time.
10. **Per-wall auto-calibration** (§11.5) from the menu.
11. **Measure it:** keep the labeled test matrix (§8). Every change has to improve hit rate, false alarms, or count error against it, or it gets rolled back.

### Updated BOM additions
| Item | Qty | Approx |
|---|---|---|
| BQ24074 charger board | 2 | $8 ea |
| TPS63020 3.3 V buck-boost | 2 | $5 ea |
| 5 V low-ripple boost | 2 | $4 ea |
| MAX17048 fuel gauge | 2 | $5 ea |
| LiPo 500 mAh (pod) / 2000 mAh (display) | 1 + 1 | $8 / $12 |
| 12 mm tactile switches + caps (or EC11 encoder) | 5 | $3 |
| Buzzer / vibration motor | 1 | $2 |
| DWM3000 ranging module (optional) | 1 per pod | $20–30 |
| Aluminum corner reflector | 1 | $5 |

---

## 16. Single integrated PCB with detachable antenna/radar head

### Key constraint (read first)
Cheap radar modules (HLK-LD2450, X4M200) have their **antennas printed on the module**. There's no connector to attach an external antenna. To get a detachable radar antenna you either:

| Option | What | Cost / risk | Recommendation |
|---|---|---|---|
| **A. Detachable radar *head*** | Main board has a keyed connector; the radar module + its antenna sit on a small "head" board that plugs in | Low. Keeps the module's FCC certification. Heads are swappable (24 GHz head, UWB head, directional head). | ✅ **v1** |
| B. Radar chip on our PCB + SMA antenna port | Put the radar IC (e.g. Infineon BGT24 24 GHz, or a UWB radar IC) directly on the main board, with an SMA/2.92 mm connector | High. 24 GHz needs RF substrate (Rogers), controlled impedance, and lossy connectors. **We lose module certification and need our own FCC compliance.** UWB imaging also has the 15.509 restriction. | ❌ Not for v1. Revisit only if A's antennas limit accuracy. |

**The ESP-NOW link antenna *can* be truly detachable now:** use the **ESP32-S3-WROOM-1U** (U.FL connector) instead of the -1 version (PCB antenna). Add a U.FL → SMA bulkhead on the case and screw on a 2.4 GHz whip or a small directional antenna. This also helps link range from drones.

### One board design, two builds
Same PCB, with different parts fitted. That's cheaper to fab and only one design to debug.

| Block | Pod build | Display build |
|---|---|---|
| ESP32-S3-WROOM-1U (N8R8) + U.FL→SMA | ✅ | ✅ |
| USB-C (charge + programming via native USB) | ✅ | ✅ |
| BQ24074 charger + power-path, protected LiPo JST-PH | ✅ | ✅ |
| TPS63020 3.3 V + filtered 5 V boost rail for radar | ✅ | 5 V rail unpopulated |
| MAX17048 fuel gauge | ✅ | ✅ |
| IMU w/ magnetometer (e.g. ICM-20948 or BMI270 + BMM150) | ✅ | optional |
| **Radar head connector** (keyed, latching, e.g. JST-GH 6-pin: 5 V, 3.3 V, GND, UART TX/RX, EN) | ✅ | — |
| Display FPC/header (SPI ST7796 3.5") | — | ✅ |
| 5 tactile buttons / EC11 footprint | 1 button | ✅ all 5 |
| RGB LED, buzzer/vibe footprint | LED | ✅ |
| Optional DWM3000 footprint (pod ranging) | ✅ optional | — |
| GoPro / 1/4"-20 mounting holes, board outline fits ≤ 60 g lite pod | ✅ | — |

### Radar heads (plug into the head connector)
- **Head-24:** LD2450 on a small carrier with an absorber backing. Nerf/SCAN.
- **Head-UWB:** X4-based module (or the FMCW fallback). LISTEN/through-wall.
- **Head-Dual:** both on one carrier, for the full pod.
- The firmware reads a head ID (resistor on an ADC pin, or a tiny I2C EEPROM) and loads the right driver automatically.

### PCB notes
- 4-layer, 1.6 mm, standard FR-4 (fine, since no RF on the main board except the U.FL feed in the module).
- Keep the buck-boost and boost converters far from the head connector. Ground stitching, LC filter at the head connector.
- Fab + assembly: JLCPCB/PCBWay with mostly basic/extended parts. Expect ~$15–30/board in qty 5–10 before modules and battery.
- Tools: KiCad 8+, schematic + layout kept in `/hardware` in this repo.

### Decisions (2026-10-19, round 4)
- **Detachable:** both. SMA (via U.FL) for the ESP-NOW link antenna, plus plug-in radar heads.
- **PCB:** Claude drafts in KiCad (schematic, BOM, pin map in `/hardware`). Final layout review and JLCPCB order with assembly afterwards.
- **Screen:** 3.5" (320×480, ST7796 SPI).
- **Ruggedness:** splash/drop resistant. Gasketed 3D-printed case, TPU bumpers, sealed button caps, SMA bulkhead with O-ring, USB-C with rubber flap.

---

## 17. Build status (2026-10-19)

- **Hardware** (`hardware/`): KiCad 9 schematic + placed 4-layer PCB, generated from `hardware/design.py`.
  - ERC clean (2 expected warnings). Netlist verified pin-by-pin against the design and the firmware pin map.
  - PCB has 0 electrical DRC errors and 0 schematic-parity issues. **Not routed yet.**
  - See `hardware/README.md` for the datasheet-verify list and the layout steps.
- **Firmware** (`firmware/`): PlatformIO, ESP32-S3.
  - `pod` and `display` builds compile; 55/55 host unit tests pass.
  - LD2450 SCAN path, ESP-NOW protocol, tracker, menus and buttons are done. The UWB head driver, breathing detection and wall calibration are stubbed.
  - Not yet run on real hardware. See `firmware/README.md`.

### Update (2026-10-20): 3 buttons, under the screen
Replaces the 5-button layout in §14. Three 12 mm buttons sit on the tab directly below the screen:

| Button | Short press | Hold |
|---|---|---|
| **UP** | up / zoom in | 1.5 s: cycle SCAN → LISTEN → MULTI |
| **OK** | select / open menu | 1.5 s: back (on map: next pod) · 3 s: sleep |
| **DOWN** | down / zoom out | — |

GPIO7 and GPIO15 are now spare (test pads TP1/TP2).

### Update (2026-10-24): board routed
- Rev A board fully routed (4 layers, 0 unconnected, 0 electrical DRC errors, matches schematic). Manufacturing files are in `hardware/fab/`.
- The button-tab break line is now **routed slots + bridges**, not a V-score. A V-cut would sever the button traces.
- The optional DWM3000 pod-to-pod ranging footprint moved to rev B (its antenna keep-out blocked routing).

### Update (2026-10-24 b): display enclosure
3D-printable display enclosure + button caps in `enclosure/` (OpenSCAD, STLs included). The screen connects to the board with a short ribbon so it can sit centred; the buttons on the PCB were moved to match. Next steps for the builder: `BUILD-GUIDE.md`.
