#include "pods.h"

int PodTable::find(uint8_t id) const {
    for (int i = 0; i < kMaxPods; i++)
        if (pods_[i].active && pods_[i].id == id) return i;
    return -1;
}

int PodTable::active_count() const {
    int c = 0;
    for (const auto& p : pods_) c += p.active;
    return c;
}

int PodTable::next_active(int from) const {
    for (int k = 1; k <= kMaxPods; k++) {
        int i = ((from < 0 ? -1 : from) + k) % kMaxPods;
        if (pods_[i].active) return i;
    }
    return -1;
}

bool PodTable::on_packet(const rt::Packet& p, uint32_t now) {
    int i = find(p.pod_id);
    bool fresh = false;
    if (i < 0) {
        for (int k = 0; k < kMaxPods; k++)
            if (!pods_[k].active) { i = k; break; }
        if (i < 0) return false;  // table full: ignore the 5th pod
        pods_[i] = PodState();
        pods_[i].active = true;
        pods_[i].id = p.pod_id;
        pods_[i].last_seq = static_cast<uint16_t>(p.seq - 1);
        fresh = true;
    }
    PodState& s = pods_[i];
    uint16_t gap = static_cast<uint16_t>(p.seq - s.last_seq);
    if (gap == 0 || gap > 50) gap = 1;  // duplicate or pod restart: don't count as loss
    s.win_expected = static_cast<uint16_t>(s.win_expected + gap);
    s.win_rx++;
    if (s.win_expected >= 50) {
        s.link_pct = static_cast<uint8_t>(100u * s.win_rx / s.win_expected);
        s.win_rx = s.win_expected = 0;
    }
    s.last_seq = p.seq;
    s.last_rx_ms = now;
    bool raised = !fresh && p.n_targets > s.people;
    if (fresh && p.n_targets > 0) raised = true;
    s.people = p.n_targets;
    s.pkt = p;
    return raised;
}

void PodTable::expire(uint32_t now) {
    for (auto& s : pods_)
        if (s.active && now - s.last_rx_ms > kPodTimeoutMs) s.active = false;
}
