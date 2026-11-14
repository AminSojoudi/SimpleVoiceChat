#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

// Lock-free sample queue for exactly one producer thread and one consumer thread.
// head and tail count samples ever written/read; index = count % capacity.
class SampleRing {
public:
    // Only while no other thread uses the ring (before the audio stream starts).
    void Resize(size_t capacity) { data.assign(capacity, 0); head = 0; tail = 0; }

    // Producer. Returns how many samples fit; the rest are dropped (newest lost when full).
    size_t Push(const uint16_t* in, size_t n) {
        uint64_t h = head.load(std::memory_order_relaxed);
        uint64_t t = tail.load(std::memory_order_acquire);
        n = std::min(n, data.size() - (size_t)(h - t));
        for (size_t i = 0; i < n; ++i) data[(h + i) % data.size()] = in[i];
        head.store(h + n, std::memory_order_release);
        return n;
    }

    // Consumer.
    size_t Pop(uint16_t* out, size_t n) {
        uint64_t t = tail.load(std::memory_order_relaxed);
        uint64_t h = head.load(std::memory_order_acquire);
        n = std::min(n, (size_t)(h - t));
        for (size_t i = 0; i < n; ++i) out[i] = data[(t + i) % data.size()];
        tail.store(t + n, std::memory_order_release);
        return n;
    }

    // Consumer only. Drops everything queued.
    void Clear() { tail.store(head.load(std::memory_order_acquire), std::memory_order_release); }

private:
    std::vector<uint16_t> data;
    std::atomic<uint64_t> head{0};
    std::atomic<uint64_t> tail{0};
};
