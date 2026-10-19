// Table of live pods (max 4) fed by validated ESP-NOW data packets.
#pragma once
#include "protocol.h"

constexpr int kMaxPods = 4;
constexpr uint32_t kPodTimeoutMs = 3000;

struct PodState {
    bool active = false;
    uint8_t id = 0;
    rt::Packet pkt;
    uint32_t last_rx_ms = 0;
    uint16_t last_seq = 0;
    uint16_t win_rx = 0, win_expected = 0;
    uint8_t link_pct = 100;  // packet delivery over the last ~50 expected packets
    uint8_t people = 0;      // n_targets of the latest packet
};

class PodTable {
public:
    // Returns true if the packet raised this pod's target count (a new confirmed target).
    bool on_packet(const rt::Packet& p, uint32_t now_ms);
    void expire(uint32_t now_ms);
    int active_count() const;
    PodState& at(int i) { return pods_[i]; }
    const PodState& at(int i) const { return pods_[i]; }
    int find(uint8_t id) const;
    // Next active slot after `from` (wraps); -1 if none.
    int next_active(int from) const;

private:
    PodState pods_[kMaxPods];
};
