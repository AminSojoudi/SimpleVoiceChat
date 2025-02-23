#ifndef UTILS_H
#define UTILS_H


#include "ConcurrentBag.hpp"
#include "Messages/AudioMessage.h"

struct NetworkBuffer {
    ConcurrentBag<AUDIO_SAMPLE> buffer;

    NetworkBuffer() : buffer(NetworkBufferSize) {}

    void AddInput(AUDIO_SAMPLE sample) {
        if (buffer.Size() >= NetworkBufferSize)
            printf("buffer is full \r\n");
        buffer.Add(sample);
    }

    bool BufferIsFull() {
        return buffer.Size() == NetworkBufferSize;
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

#endif // UTILS_H