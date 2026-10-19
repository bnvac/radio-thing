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
