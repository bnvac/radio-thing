#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "protocol.h"
#include "tests.h"

using namespace rt;

static Packet sample(uint8_t n) {
    Packet p;
    p.pod_id = 3;
    p.seq = 0xBEEF;
    p.uptime_ms = 0x01020304;
    p.mode = Mode::LISTEN;
    p.battery_pct = 87;
    p.flags = Flag::BLOCKED | Flag::CHARGING;
    p.head_type = HeadType::DUAL;
    p.n_targets = n;
    for (uint8_t i = 0; i < n; i++) {
        p.targets[i].x_mm = static_cast<int16_t>(-1000 + i * 37);
        p.targets[i].y_mm = static_cast<int16_t>(2500 + i);
        p.targets[i].speed_cms = static_cast<int16_t>(-i);
        p.targets[i].confidence = static_cast<uint8_t>(10 * i);
        p.targets[i].kind = static_cast<TargetKind>(i % 3);
    }
    return p;
}

static void test_crc_check_value() {
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16_ccitt_false(reinterpret_cast<const uint8_t*>(s), 9));
}

static void test_roundtrip_and_layout() {
    Packet p = sample(8);
    uint8_t buf[kMaxPacketLen];
    size_t n = serialize(p, buf, sizeof buf);
    TEST_ASSERT_EQUAL(82, n);
    TEST_ASSERT_TRUE(n <= 250);
    TEST_ASSERT_EQUAL_UINT8('R', buf[0]);
    TEST_ASSERT_EQUAL_UINT8('T', buf[1]);
    TEST_ASSERT_EQUAL_UINT8(1, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, buf[4]);  // seq little-endian
    TEST_ASSERT_EQUAL_HEX8(0xBE, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[6]);  // uptime little-endian
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[9]);
    Packet q;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::OK), static_cast<int>(deserialize(buf, n, q)));
    TEST_ASSERT_EQUAL_UINT8(3, q.pod_id);
    TEST_ASSERT_EQUAL_UINT16(0xBEEF, q.seq);
    TEST_ASSERT_EQUAL_UINT32(0x01020304, q.uptime_ms);
    TEST_ASSERT_EQUAL(static_cast<int>(Mode::LISTEN), static_cast<int>(q.mode));
    TEST_ASSERT_EQUAL_UINT8(87, q.battery_pct);
    TEST_ASSERT_EQUAL_UINT16(Flag::BLOCKED | Flag::CHARGING, q.flags);
    TEST_ASSERT_EQUAL(static_cast<int>(HeadType::DUAL), static_cast<int>(q.head_type));
    TEST_ASSERT_EQUAL_UINT8(8, q.n_targets);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT16(p.targets[i].x_mm, q.targets[i].x_mm);
        TEST_ASSERT_EQUAL_INT16(p.targets[i].y_mm, q.targets[i].y_mm);
        TEST_ASSERT_EQUAL_INT16(p.targets[i].speed_cms, q.targets[i].speed_cms);
        TEST_ASSERT_EQUAL_UINT8(p.targets[i].confidence, q.targets[i].confidence);
        TEST_ASSERT_EQUAL(static_cast<int>(p.targets[i].kind), static_cast<int>(q.targets[i].kind));
    }
}

static void test_zero_targets() {
    Packet p = sample(0);
    uint8_t buf[kMaxPacketLen];
    size_t n = serialize(p, buf, sizeof buf);
    TEST_ASSERT_EQUAL(18, n);
    Packet q;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::OK), static_cast<int>(deserialize(buf, n, q)));
    TEST_ASSERT_EQUAL_UINT8(0, q.n_targets);
}

static void test_serialize_rejects() {
    Packet p = sample(2);
    uint8_t small[10];
    TEST_ASSERT_EQUAL(0, serialize(p, small, sizeof small));
    p.n_targets = 9;
    uint8_t buf[200];
    TEST_ASSERT_EQUAL(0, serialize(p, buf, sizeof buf));
}

static void test_reject_malformed() {
    Packet p = sample(3), q;
    uint8_t buf[kMaxPacketLen];
    size_t n = serialize(p, buf, sizeof buf);
    uint8_t b[kMaxPacketLen];

    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::TOO_SHORT), static_cast<int>(deserialize(buf, 5, q)));

    std::memcpy(b, buf, n);
    b[0] = 'X';
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_MAGIC), static_cast<int>(deserialize(b, n, q)));

    std::memcpy(b, buf, n);
    b[2] = 2;  // version (CRC would also fail, version is checked first)
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_VERSION), static_cast<int>(deserialize(b, n, q)));

    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_LENGTH), static_cast<int>(deserialize(buf, n - 1, q)));
    std::memcpy(b, buf, n);
    b[n] = 0;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_LENGTH), static_cast<int>(deserialize(b, n + 1, q)));

    // flip every single bit in the body: CRC (or an earlier check) must reject each one
    for (size_t i = 0; i < n; i++) {
        for (int bit = 0; bit < 8; bit++) {
            std::memcpy(b, buf, n);
            b[i] ^= static_cast<uint8_t>(1u << bit);
            TEST_ASSERT_NOT_EQUAL(static_cast<int>(ParseResult::OK), static_cast<int>(deserialize(b, n, q)));
        }
    }
}

static void test_reject_bad_fields_with_valid_crc() {
    Packet p = sample(1), q;
    uint8_t buf[kMaxPacketLen];
    size_t n = serialize(p, buf, sizeof buf);
    // mode out of range, re-sign CRC
    buf[10] = 7;
    uint16_t c = crc16_ccitt_false(buf, n - 2);
    buf[n - 2] = c & 0xFF;
    buf[n - 1] = c >> 8;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_FIELD), static_cast<int>(deserialize(buf, n, q)));
    // n_targets > 8
    n = serialize(p, buf, sizeof buf);
    buf[15] = 9;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_FIELD), static_cast<int>(deserialize(buf, n, q)));
}

static void test_command_roundtrip_and_reject() {
    Command c;
    c.target_pod = 2;
    c.seq = 9;
    c.cmd = CmdType::SET_MODE;
    c.arg = static_cast<uint8_t>(Mode::MULTI);
    uint8_t buf[16];
    size_t n = serialize(c, buf, sizeof buf);
    TEST_ASSERT_EQUAL(kCommandLen, n);
    TEST_ASSERT_EQUAL(static_cast<int>(PacketType::COMMAND), static_cast<int>(packet_type(buf, n)));
    Command d;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::OK), static_cast<int>(deserialize(buf, n, d)));
    TEST_ASSERT_EQUAL_UINT8(2, d.target_pod);
    TEST_ASSERT_EQUAL_UINT8(9, d.seq);
    TEST_ASSERT_EQUAL(static_cast<int>(CmdType::SET_MODE), static_cast<int>(d.cmd));
    TEST_ASSERT_EQUAL_UINT8(2, d.arg);

    uint8_t b[16];
    std::memcpy(b, buf, n);
    b[6] ^= 1;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_CRC), static_cast<int>(deserialize(b, n, d)));
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_LENGTH), static_cast<int>(deserialize(buf, n + 1, d)));
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::TOO_SHORT), static_cast<int>(deserialize(buf, 4, d)));
    // a data packet is not a command
    Packet p = sample(0);
    uint8_t pb[kMaxPacketLen];
    size_t pn = serialize(p, pb, sizeof pb);
    TEST_ASSERT_EQUAL(static_cast<int>(PacketType::DATA), static_cast<int>(packet_type(pb, pn)));
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_MAGIC), static_cast<int>(deserialize(pb, pn, d)));
    // invalid mode argument with valid CRC
    c.arg = 9;
    n = serialize(c, buf, sizeof buf);
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_FIELD), static_cast<int>(deserialize(buf, n, d)));
    TEST_ASSERT_EQUAL(static_cast<int>(PacketType::UNKNOWN), static_cast<int>(packet_type(buf, 1)));
    // SET_CHANNEL range
    c.cmd = CmdType::SET_CHANNEL;
    for (uint8_t ch : {uint8_t(0), uint8_t(14)}) {
        c.arg = ch;
        n = serialize(c, buf, sizeof buf);
        TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_FIELD), static_cast<int>(deserialize(buf, n, d)));
    }
    c.arg = 11;
    n = serialize(c, buf, sizeof buf);
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::OK), static_cast<int>(deserialize(buf, n, d)));
    TEST_ASSERT_EQUAL_UINT8(11, d.arg);
    // unknown command id
    n = serialize(c, buf, sizeof buf);
    buf[5] = 9;
    uint16_t crc = crc16_ccitt_false(buf, 7);
    buf[7] = crc & 0xFF;
    buf[8] = crc >> 8;
    TEST_ASSERT_EQUAL(static_cast<int>(ParseResult::BAD_FIELD), static_cast<int>(deserialize(buf, n, d)));
}

void run_protocol_tests() {
    RUN_TEST(test_crc_check_value);
    RUN_TEST(test_roundtrip_and_layout);
    RUN_TEST(test_zero_targets);
    RUN_TEST(test_serialize_rejects);
    RUN_TEST(test_reject_malformed);
    RUN_TEST(test_reject_bad_fields_with_valid_crc);
    RUN_TEST(test_command_roundtrip_and_reject);
}
