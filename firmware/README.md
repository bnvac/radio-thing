# Radio Thing firmware

PlatformIO project for the two builds of the one PCB: **pod** (radar head + ESP-NOW broadcaster)
and **display** (ST7796 radar map + menus). Pin map: `include/board_pins.h` (fixed contract).

## Build / flash / test

```
cd firmware
pio run -e pod                 # build pod
pio run -e display             # build display
pio test -e native             # host unit tests for lib/core (needs gcc/g++)
pio run -e pod -t upload       # flash (native USB; hold BOOT + tap reset if the port is absent)
pio device monitor
```

Pinned: `espressif32@6.10.0` (Arduino-ESP32 2.0.17, ESP-IDF 4.4), `lovyan03/LovyanGFX@1.2.33`.
Board is `esp32-s3-devkitc-1` with `qio_opi` memory type (N8R8), 8 MB flash, USB-CDC on boot.
Pods: `chan N` over the USB serial console sets the ESP-NOW channel (pods have no screen).

## Layout

```
include/board_pins.h     pin contract
lib/core/                pure C++17, no Arduino; unit-tested on the host
  ld2450   streaming parser for HLK-LD2450 frames
  protocol ESP-NOW packet v1 + command packet, CRC-16/CCITT-FALSE
  tracker  NN association + alpha-beta + M-of-N confirmation
  blockage BLOCKED / NO PENETRATION / HOLD STILL / ... decision function
  head_id  head-ID ADC millivolts -> head type
  debounce button debouncer, short / long press
  fuel     MAX17048 register decoding
  geometry fan-view pixel mapping, range presets, multi-pod placement
src/pod/                 main.cpp, radar_head.h (RadarHead, Ld2450Head, UwbHead stub)
src/display/             main.cpp (radio, buttons, sleep, buzzer), ui.* (map + menus),
                         pods.* (pod table), settings.* (Preferences), lgfx_config.h
test/test_core/          Unity tests (one binary, one file per module)
```

Data flow (pod): LD2450 UART1 -> `Ld2450Parser` -> `Tracker` (confirmed tracks only) ->
`Packet` (10 Hz ESP-NOW broadcast). IMU motion + detection state -> `evaluate_blockage` -> flags.
Display: ESP-NOW rx callback -> queue -> validate (`deserialize`) -> `PodTable` (4 pods, 3 s timeout)
-> `Ui`. Commands (display -> pods, magic `RC`): SET_MODE, LOCATE, PAIR_ACK, SET_CHANNEL.

## Behaviour notes

- Display never shows "EMPTY". No detections is `NOTHING DETECTED (low confidence)` unless a head
  supplies a range profile, which no current head does (see below).
- Target kinds from the LD2450 pod: moving (>100 mm/s track speed) = MOVING, stationary = MAYBE
  (hollow dot, confidence capped at 50) because stationary returns may be clutter.
- People count is the number of confirmed tracks of the selected pod, shown capped as `3+`.
- Controls: UP/DOWN zoom (3/6/9 m) on the map and move in menus; OK opens the menu (deviation from
  the SCOPE table "OK = start LISTEN": use MODE for that); BACK switches pod on the map / goes back;
  MODE cycles SCAN/LISTEN/MULTI on all pods; hold OK 2 s = deep sleep, wake on OK (ext0, GPIO6).
- Pod button: short = cycle mode, long (1.5 s) = pairing beacon for 30 s (blue blink,
  PAIRING flag). Display Menu > Pods > Pair new pods sends PAIR_ACK. Pairing is only a handshake
  of the LED/flag state; any valid pod is accepted by the display regardless.
- LED: green running, blue blink pairing, red blocked / low battery / no head, white blink locate.
- Nerf mode: forces SCAN and 3 m range, bigger dots, big nearest-target distance.

## Stubbed / unverified (read before trusting)

- **No hardware testing at all.** Everything compiles; nothing has run on a board.
- LD2450 sign-magnitude encoding (bit15 set = positive) and frame layout were checked against the
  ESPHome `ld2450` component source (see comment in `lib/core/ld2450.h`). Speed unit cm/s follows
  from ESPHome's `*10` to mm/s. Not checked against the HLK PDF or a real sensor.
- ISM330DHCX registers (WHO_AM_I 0x0F = 0x6B, CTRL1_XL 0x10 = 0x40, OUTX_L_A 0x28, 0.061 mg/LSB at
  +-2 g) are from memory of the ST datasheet, not re-verified here. LIS2MDL is not read at all.
- MAX17048 register map (VCELL 0x02 at 78.125 uV/LSB, SOC 0x04) from the Maxim datasheet, unverified.
- UWB head: `UwbHead` is an empty stub. UWB-only pods run but detect nothing and say so
  (LOW_CONF). DUAL heads use the LD2450 only. Blockage energy checks (BLOCKED, NO_PENETRATION,
  TOO_CLOSE, NOTHING_DETECTED) are implemented and tested but unreachable on hardware until a head
  provides a range profile; the LD2450 supplies only IMU motion + detection.
- Fusion of LD2450 and UWB, breathing detection, MULTI-SPOT intersection: not implemented. MULTI
  draws all pods in one world frame with a fixed default layout (1.5 m apart, all facing forward);
  the geometry transform exists and is tested but there is no UI to enter real positions.
- Wall > Calibrate is a placeholder; the material preset is stored but changes nothing.
- Signal strength: ESP-NOW rx callback in Arduino-ESP32 2.0.17 gives no RSSI, so "link %" (packet
  delivery from sequence numbers) is shown instead.
- Pod ID is the last MAC byte (collision chance 1/256 per pair). Radio is unencrypted, broadcast.
- ST7796 colour inversion / RGB order (`RT_LCD_INVERT`, `RT_LCD_RGB_ORDER_BGR` in `lgfx_config.h`)
  and SPI speed are guesses for the 3.5" module; flip if colours look wrong.
- Deep-sleep wake uses ext0 on GPIO6; backlight pin is held low in sleep. Untested.
- Magnetometer / heading for multi-pod placement not used yet.
