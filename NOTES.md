# NOTES

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19
* **Decisions/Changes**: Wrote SCOPE.md (feasibility + tiered plan). Proposed path: Tier A (ESP32-S3 + HLK-LD2450 24 GHz + TFT, ~$50) first, then Tier C (UWB impulse radar + Raspberry Pi) for through-wall breathing detection. Blockage/"can't see" detection is a v1 requirement. Firmware as pure pipeline stages: acquire -> clutter removal -> CFAR -> vitals -> tracking -> render.
* **Known Bugs/Gaps**: Open decisions in SCOPE.md §7 (goal, budget, MCU vs Pi, handheld vs tripod, wall types). Part prices/availability not verified. Through-wall heartbeat and 2D through-wall location with a single module are not expected to work.
<!-- MEMORY_UPDATE_END -->

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19 (b)
* **Decisions/Changes**: One unified device. ESP32-S3 + TFT + HLK-LD2450 (2D, Nerf/SCAN) + swappable UWB head (X4M200 preferred) + IMU. Modes: SCAN (free handheld), LISTEN (braced on wall/tripod, breathing), MULTI-SPOT (2D from arcs). Targets: ordinary building walls; brick/concrete best-effort; no rubble. See SCOPE.md §9.
* **Known Bugs/Gaps**: UWB module availability unverified (X4 parts are 2017-era) -> confirm stock before M3; 24 GHz FMCW fallback. Counting capped at "3+".
<!-- MEMORY_UPDATE_END -->

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19 (c)
* **Decisions/Changes**: Split into Sensor Pod (radars + ESP32-S3 + IMU + battery, <100 g, universal mount: tripod/GoPro/magnet/VHB adhesive) and Display Unit (ESP32-S3 + screen), linked by ESP-NOW (LoRa optional). Multi-pod map. Accuracy plan in SCOPE.md §11: wall-echo motion compensation, wall coupling, longer dwell, multi-view triangulation/SAR, wall calibration, directional antennas, M-of-N + Kalman tracking, optional MLX90640 thermal.
* **Known Bugs/Gaps**: Breathing detection from a hovering/moving drone not expected to work; perch-and-listen instead. Propeller micro-Doppler needs notch filtering. Check drone RF band overlap with UWB head. FAA/permission constraints.
<!-- MEMORY_UPDATE_END -->

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19 (d)
* **Decisions/Changes**: ESP-NOW confirmed (LoRa out of v1). Drone-agnostic (FPV + DJI): self-powered pod, GoPro mount, lite pod ≤60 g / full pod ≤100 g, rotor clutter filter estimates blade frequency from radar data (no RPM telemetry).
* **Known Bugs/Gaps**: 2.4 GHz coexistence with ELRS/DJI control links untested; must range test before flight.
<!-- MEMORY_UPDATE_END -->

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19 (e)
* **Decisions/Changes**: Power design: BQ24074 (power-path charger) + TPS63020 3.3 V buck-boost + low-ripple 5 V boost with LC filter for LD2450 + MAX17048 fuel gauge; XIAO ESP32S3 for lite pod. Controls: 5 glove-friendly buttons (UP/DOWN/OK/BACK/MODE) or EC11 encoder, LVGL input, OK on RTC GPIO for wake; pod has 1 button + RGB LED. Extra accuracy (§15): clean radar rail, DWM3000 pod-to-pod ranging (time-shared with X4), magnetometer + manual heading, corner-reflector calibration, warm-up, dual-radar fusion, timestamp sync.
* **Known Bugs/Gaps**: Current-draw figures are estimates; verify LD2450 supply voltage/current and display-board charger before buying. DWM3000/X4 band overlap requires time multiplexing.
<!-- MEMORY_UPDATE_END -->

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19 (f)
* **Decisions/Changes**: One custom PCB design for both pod and display (different population). ESP32-S3-WROOM-1U with U.FL→SMA for a detachable ESP-NOW antenna. Radar antenna detachability via plug-in "radar heads" on a keyed JST-GH connector with head-ID; on-board radar RF (SMA) rejected for v1 due to RF layout + FCC certification.
* **Known Bugs/Gaps**: Resolved: both antennas detachable; Claude drafts KiCad; 3.5" ST7796; splash/drop-resistant case. Next: pin map + BOM, then schematic.
<!-- MEMORY_UPDATE_END -->

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19 (g)
* **Decisions/Changes**: Built rev A. hardware/design.py is the circuit source -> generated KiCad 9 schematic/PCB (82x50 main + 22 mm snap-off button tab, 4-layer, GND In1/In2). Substitutes: TPS61202 (5 V), ISM330DHCX+LIS2MDL, custom MAX17048 symbol, Alps SKRK reset/boot, KCG0601 buzzer. Firmware: PlatformIO, espressif32@6.10.0, LovyanGFX 1.2.33, core libs pure C++ with 55 Unity tests; ESP-NOW data packet 'RT' + command packet 'RC' with CRC16.
* **Known Bugs/Gaps**: PCB unrouted. Datasheet-verify list in hardware/README.md (BQ24074 TMR/ITERM, TPS61202 PS, MAX17048 pinout, DWM3000 wiring, display header position). IMU/MAX17048 registers unverified; UWB driver/breathing/wall calibration stubbed; no hardware test yet. All GPIOs used.
<!-- MEMORY_UPDATE_END -->
