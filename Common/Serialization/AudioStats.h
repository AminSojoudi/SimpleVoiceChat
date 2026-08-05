#pragma once

// Audio traffic instrumentation, compiled out by default. Build with
// -DVOICECHAT_AUDIO_STATS to record, per packet, the sample count and a
// bits-required histogram over zig-zag-mapped samples. The data feeds the
// later compression decision; no compression exists yet.
//
// Samples are signed PCM stored in uint16 (RtAudio uses RTAUDIO_SINT16).
// Zig-zag mapping folds negatives near zero; a plain unsigned histogram
// would count every negative sample as 16 bits.

#ifdef VOICECHAT_AUDIO_STATS

#include <cstdint>
#include <cstdio>

struct AudioStats {
    uint64_t packets = 0;
    uint64_t samples = 0;
    uint64_t minCount = UINT64_MAX;
    uint64_t maxCount = 0;
    uint64_t bitsHistogram[17] = {}; // index = bits required, 0..16

    static AudioStats& Get() {
        static AudioStats instance;
        return instance;
    }

    void Record(const uint16_t* data, uint32_t count) {
        ++packets;
        samples += count;
        if (count < minCount) minCount = count;
        if (count > maxCount) maxCount = count;
        for (uint32_t i = 0; i < count; ++i) {
            int32_t v = static_cast<int16_t>(data[i]);
            uint32_t z = static_cast<uint32_t>((v << 1) ^ (v >> 31));
            int bits = 0;
            while (z) { ++bits; z >>= 1; }
            ++bitsHistogram[bits];
        }
    }

    void Print() const {
        if (packets == 0) {
            printf("audio stats: no packets recorded\n");
            return;
        }
        printf("audio stats: %llu packets, %llu samples, count min %llu max %llu avg %.1f\n",
               (unsigned long long)packets, (unsigned long long)samples,
               (unsigned long long)minCount, (unsigned long long)maxCount,
               (double)samples / (double)packets);
        printf("bits-required histogram (zig-zag mapped):\n");
        for (int i = 0; i <= 16; ++i) {
            printf("  %2d bits: %llu (%.2f%%)\n", i,
                   (unsigned long long)bitsHistogram[i],
                   100.0 * (double)bitsHistogram[i] / (double)samples);
        }
    }
};

#define VOICECHAT_AUDIO_STATS_RECORD(data, count) AudioStats::Get().Record((data), (count))
#define VOICECHAT_AUDIO_STATS_PRINT() AudioStats::Get().Print()

#else

#define VOICECHAT_AUDIO_STATS_RECORD(data, count) ((void)0)
#define VOICECHAT_AUDIO_STATS_PRINT() ((void)0)

#endif
