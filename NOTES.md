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
