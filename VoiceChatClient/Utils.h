#pragma once


#include "ConcurrentBag.hpp"
#include "../Common/Messages/AudioMessage.h"

// Playback buffer slack, in poll intervals. Arrival and playback drift
// against each other; a few intervals of headroom absorbs the jitter.
// More slack also means more worst-case latency, so keep it small.
constexpr size_t PlaybackSlackIntervals = 4;

// Floor for the slack in samples: a tenth of a second. Short poll intervals
// need it because OS sleep jitter (~16 ms on Windows) spans many intervals.
constexpr size_t PlaybackSlackMinFractionOfSecond = 10;

// Default before main.cpp knows the real frames per interval.
constexpr size_t DefaultNetworkBufferCapacity = 4096;

struct NetworkBuffer {
    ConcurrentBag<AUDIO_SAMPLE> buffer;
    size_t capacity = DefaultNetworkBufferCapacity;

    NetworkBuffer() : buffer(DefaultNetworkBufferCapacity) {}

    // Call before audio starts. Sized from the poll interval so slack stays
    // constant in intervals no matter how big one interval is.
    void SetCapacity(size_t samples) {
        capacity = samples;
        buffer.SetMaxSize(samples);
    }

    void AddInput(AUDIO_SAMPLE sample) {
        if (buffer.Size() >= capacity)
            printf("playback buffer full, dropping oldest samples \r\n");
        buffer.Add(sample);
    }

    bool BufferIsFull() {
        return buffer.Size() == capacity;
    }

    size_t Size() const {
        return buffer.Size();
    }

    std::optional<AUDIO_SAMPLE> ReadAt(size_t i) {
        return buffer.GetAt(i);
    }

    void ResetData() {
        buffer.Reset();
    }

    void RemoveFirstItems(int numbers) {
        buffer.Erase(numbers);
    }
};