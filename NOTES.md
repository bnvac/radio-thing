# NOTES

<!-- MEMORY_UPDATE_START -->
### Update: 2026-10-19
* **Decisions/Changes**: Wrote SCOPE.md (feasibility + tiered plan). Proposed path: Tier A (ESP32-S3 + HLK-LD2450 24 GHz + TFT, ~$50) first, then Tier C (UWB impulse radar + Raspberry Pi) for through-wall breathing detection. Blockage/"can't see" detection is a v1 requirement. Firmware as pure pipeline stages: acquire -> clutter removal -> CFAR -> vitals -> tracking -> render.
* **Known Bugs/Gaps**: Open decisions in SCOPE.md §7 (goal, budget, MCU vs Pi, handheld vs tripod, wall types). Part prices/availability not verified. Through-wall heartbeat and 2D through-wall location with a single module are not expected to work.
<!-- MEMORY_UPDATE_END -->
