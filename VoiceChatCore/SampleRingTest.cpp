// Keep asserts on in release builds too, since CI builds release.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <thread>

#include "SampleRing.h"

int main() {
    SampleRing ring;
    ring.Resize(4);
    uint16_t in[6] = {1, 2, 3, 4, 5, 6};
    uint16_t out[6] = {};

    // Round trip.
    assert(ring.Push(in, 3) == 3);
    assert(ring.Pop(out, 6) == 3);
    assert(out[0] == 1 && out[1] == 2 && out[2] == 3);
    assert(ring.Pop(out, 1) == 0);

    // Wrap past the end: head is at 3, so these land at indexes 3, 0, 1.
    assert(ring.Push(in + 3, 3) == 3);
    assert(ring.Pop(out, 3) == 3);
    assert(out[0] == 4 && out[1] == 5 && out[2] == 6);

    // A full ring takes only what fits.
    assert(ring.Push(in, 6) == 4);
    assert(ring.Push(in, 1) == 0);

    // Clear drops everything.
    ring.Clear();
    assert(ring.Pop(out, 1) == 0);

    // Two threads: the consumer must see a counting sequence in order.
    const uint32_t total = 1000000;
    ring.Resize(64);
    std::thread producer([&] {
        uint32_t next = 0;
        while (next < total) {
            uint16_t chunk[16];
            size_t n = 0;
            while (n < 16 && next + n < total) { chunk[n] = (uint16_t)(next + n); ++n; }
            next += (uint32_t)ring.Push(chunk, n);
        }
    });
    uint32_t expected = 0;
    while (expected < total) {
        uint16_t chunk[16];
        size_t n = ring.Pop(chunk, 16);
        for (size_t i = 0; i < n; ++i, ++expected)
            assert(chunk[i] == (uint16_t)expected);
    }
    producer.join();

    printf("SampleRingTest passed\n");
    return 0;
}
