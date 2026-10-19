#include "protocol.h"

namespace rt {

namespace {
void put16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

PacketType packet_type(const uint8_t* d, size_t len) {
    if (len < 2 || d[0] != 'R') return PacketType::UNKNOWN;
    if (d[1] == 'T') return PacketType::DATA;
    if (d[1] == 'C') return PacketType::COMMAND;
    return PacketType::UNKNOWN;
}

size_t serialize(const Packet& p, uint8_t* out, size_t cap) {
    if (p.n_targets > kMaxTargets) return 0;
    const size_t total = 16 + static_cast<size_t>(p.n_targets) * 8 + 2;
    if (cap < total) return 0;
    out[0] = 'R';
    out[1] = 'T';
    out[2] = kProtoVersion;
    out[3] = p.pod_id;
    put16(out + 4, p.seq);
    put32(out + 6, p.uptime_ms);
    out[10] = static_cast<uint8_t>(p.mode);
    out[11] = p.battery_pct;
    put16(out + 12, p.flags);
    out[14] = static_cast<uint8_t>(p.head_type);
    out[15] = p.n_targets;
    for (size_t i = 0; i < p.n_targets; i++) {
        uint8_t* t = out + 16 + i * 8;
        put16(t, static_cast<uint16_t>(p.targets[i].x_mm));
        put16(t + 2, static_cast<uint16_t>(p.targets[i].y_mm));
        put16(t + 4, static_cast<uint16_t>(p.targets[i].speed_cms));
        t[6] = p.targets[i].confidence;
        t[7] = static_cast<uint8_t>(p.targets[i].kind);
    }
    put16(out + total - 2, crc16_ccitt_false(out, total - 2));
    return total;
}

ParseResult deserialize(const uint8_t* d, size_t len, Packet& out) {
    if (len < 18) return ParseResult::TOO_SHORT;
    if (d[0] != 'R' || d[1] != 'T') return ParseResult::BAD_MAGIC;
    if (d[2] != kProtoVersion) return ParseResult::BAD_VERSION;
    const uint8_t n = d[15];
    if (n > kMaxTargets) return ParseResult::BAD_FIELD;
    const size_t total = 16 + static_cast<size_t>(n) * 8 + 2;
    if (len != total) return ParseResult::BAD_LENGTH;
    if (get16(d + total - 2) != crc16_ccitt_false(d, total - 2)) return ParseResult::BAD_CRC;
    if (d[10] >= kModeCount || d[14] > static_cast<uint8_t>(HeadType::DUAL))
        return ParseResult::BAD_FIELD;
    for (size_t i = 0; i < n; i++) {
        const uint8_t* t = d + 16 + i * 8;
        if (t[6] > 100 || t[7] > static_cast<uint8_t>(TargetKind::MAYBE))
            return ParseResult::BAD_FIELD;
    }
    Packet p;
    p.pod_id = d[3];
    p.seq = get16(d + 4);
    p.uptime_ms = get32(d + 6);
    p.mode = static_cast<Mode>(d[10]);
    p.battery_pct = d[11];
    p.flags = get16(d + 12);
    p.head_type = static_cast<HeadType>(d[14]);
    p.n_targets = n;
    for (size_t i = 0; i < n; i++) {
        const uint8_t* t = d + 16 + i * 8;
        p.targets[i].x_mm = static_cast<int16_t>(get16(t));
        p.targets[i].y_mm = static_cast<int16_t>(get16(t + 2));
        p.targets[i].speed_cms = static_cast<int16_t>(get16(t + 4));
        p.targets[i].confidence = t[6];
        p.targets[i].kind = static_cast<TargetKind>(t[7]);
    }
    out = p;
    return ParseResult::OK;
}

size_t serialize(const Command& c, uint8_t* out, size_t cap) {
    if (cap < kCommandLen) return 0;
    out[0] = 'R';
    out[1] = 'C';
    out[2] = kProtoVersion;
    out[3] = c.target_pod;
    out[4] = c.seq;
    out[5] = static_cast<uint8_t>(c.cmd);
    out[6] = c.arg;
    put16(out + 7, crc16_ccitt_false(out, 7));
    return kCommandLen;
}

ParseResult deserialize(const uint8_t* d, size_t len, Command& out) {
    if (len < 2) return ParseResult::TOO_SHORT;
    if (d[0] != 'R' || d[1] != 'C') return ParseResult::BAD_MAGIC;
    if (len < kCommandLen) return ParseResult::TOO_SHORT;
    if (d[2] != kProtoVersion) return ParseResult::BAD_VERSION;
    if (len != kCommandLen) return ParseResult::BAD_LENGTH;
    if (get16(d + 7) != crc16_ccitt_false(d, 7)) return ParseResult::BAD_CRC;
    if (d[5] < 1 || d[5] > static_cast<uint8_t>(CmdType::SET_CHANNEL)) return ParseResult::BAD_FIELD;
    if (d[5] == static_cast<uint8_t>(CmdType::SET_MODE) && d[6] >= kModeCount)
        return ParseResult::BAD_FIELD;
    if (d[5] == static_cast<uint8_t>(CmdType::SET_CHANNEL) && (d[6] < 1 || d[6] > 13))
        return ParseResult::BAD_FIELD;
    out.target_pod = d[3];
    out.seq = d[4];
    out.cmd = static_cast<CmdType>(d[5]);
    out.arg = d[6];
    return ParseResult::OK;
}

const char* mode_name(Mode m) {
    switch (m) {
        case Mode::SCAN: return "SCAN";
        case Mode::LISTEN: return "LISTEN";
        case Mode::MULTI: return "MULTI";
    }
    return "?";
}

}  // namespace rt
