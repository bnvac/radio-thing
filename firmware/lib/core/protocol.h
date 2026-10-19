// ESP-NOW wire protocol, version 1. Explicit little-endian, no struct memcpy.
//
// Data packet (pod -> display), magic 'R','T':
//   0  'R' 'T'
//   2  version (=1)
//   3  pod_id
//   4  seq (u16)
//   6  uptime_ms (u32)
//   10 mode (Mode)
//   11 battery_pct (0..100)
//   12 flags (u16, Flag bits)
//   14 head_type (HeadType)
//   15 n_targets (0..8)
//   16 n_targets x { x_mm i16, y_mm i16, speed_cms i16, confidence u8, kind u8 }
//   .. CRC-16/CCITT-FALSE (u16 LE) over every preceding byte
// Max size 16 + 8*8 + 2 = 82 bytes (ESP-NOW limit 250).
//
// Command packet (display -> pods), magic 'R','C':
//   0 'R' 'C' | 2 version | 3 target_pod_id (0xFF = all) | 4 seq (u8)
//   5 cmd | 6 arg | 7 CRC-16 (u16 LE)   => 9 bytes
#pragma once
#include <cstddef>
#include <cstdint>

namespace rt {

constexpr uint8_t kProtoVersion = 1;
constexpr size_t kMaxTargets = 8;
constexpr size_t kMaxPacketLen = 16 + kMaxTargets * 8 + 2;
constexpr size_t kCommandLen = 9;
constexpr uint8_t kPodBroadcast = 0xFF;

enum class Mode : uint8_t { SCAN = 0, LISTEN = 1, MULTI = 2 };
constexpr uint8_t kModeCount = 3;

enum class HeadType : uint8_t { NONE = 0, H24 = 1, UWB = 2, DUAL = 3 };

enum class TargetKind : uint8_t { MOVING = 0, BREATHING = 1, MAYBE = 2 };

namespace Flag {
constexpr uint16_t BLOCKED = 1u << 0;
constexpr uint16_t NO_PENETRATION = 1u << 1;
constexpr uint16_t HOLD_STILL = 1u << 2;
constexpr uint16_t LOW_BATT = 1u << 3;
constexpr uint16_t CHARGING = 1u << 4;
constexpr uint16_t WARMING_UP = 1u << 5;
constexpr uint16_t PAIRING = 1u << 6;       // addition: pod is in pairing mode
constexpr uint16_t LOW_CONF = 1u << 7;      // addition: "nothing detected" is low confidence
constexpr uint16_t TOO_CLOSE = 1u << 8;     // addition: saturated / too reflective
constexpr uint16_t KNOWN_MASK = 0x01FF;
}  // namespace Flag

struct WireTarget {
    int16_t x_mm = 0;
    int16_t y_mm = 0;
    int16_t speed_cms = 0;
    uint8_t confidence = 0;  // 0..100
    TargetKind kind = TargetKind::MOVING;
};

struct Packet {
    uint8_t pod_id = 0;
    uint16_t seq = 0;
    uint32_t uptime_ms = 0;
    Mode mode = Mode::SCAN;
    uint8_t battery_pct = 0;
    uint16_t flags = 0;
    HeadType head_type = HeadType::NONE;
    uint8_t n_targets = 0;
    WireTarget targets[kMaxTargets];
};

enum class CmdType : uint8_t {
    SET_MODE = 1,  // arg = Mode
    LOCATE = 2,    // arg = seconds to blink the LED (0 = stop)
    PAIR_ACK = 3,  // arg unused; ends a pod's pairing state
    SET_CHANNEL = 4,  // arg = Wi-Fi channel 1..13; pod switches and persists it
};

struct Command {
    uint8_t target_pod = kPodBroadcast;
    uint8_t seq = 0;
    CmdType cmd = CmdType::SET_MODE;
    uint8_t arg = 0;
};

enum class ParseResult : uint8_t {
    OK = 0,
    TOO_SHORT,
    BAD_MAGIC,
    BAD_VERSION,
    BAD_LENGTH,
    BAD_CRC,
    BAD_FIELD,  // out-of-range enum / n_targets
};

enum class PacketType : uint8_t { UNKNOWN = 0, DATA, COMMAND };

uint16_t crc16_ccitt_false(const uint8_t* data, size_t len);

// Cheap magic sniff so a receiver can dispatch to the right deserializer.
PacketType packet_type(const uint8_t* data, size_t len);

// Returns bytes written, or 0 if cap too small / packet invalid (n_targets > 8).
size_t serialize(const Packet& p, uint8_t* out, size_t cap);
ParseResult deserialize(const uint8_t* data, size_t len, Packet& out);

size_t serialize(const Command& c, uint8_t* out, size_t cap);
ParseResult deserialize(const uint8_t* data, size_t len, Command& out);

const char* mode_name(Mode m);

}  // namespace rt
