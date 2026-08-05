#pragma once

#include "BitStream.h"

// Three stream types with the same surface, so one Serialize(Stream&) method
// per message serves write, read, and size measurement.

struct WriteStream {
    static constexpr bool IsWriting = true;

    WriteStream(void* buffer, uint32_t bytes) : writer(buffer, bytes) {}

    bool SerializeBits(uint32_t& value, int bits) {
        writer.WriteBits(value, bits);
        return !writer.Error();
    }

    // Call after the last field, before reading BytesWritten.
    void Flush() { writer.FlushBits(); }
    uint32_t BytesWritten() const { return writer.BytesWritten(); }
    bool Error() const { return writer.Error(); }

    BitWriter writer;
};

struct ReadStream {
    static constexpr bool IsWriting = false;

    ReadStream(const void* buffer, uint32_t bytes) : reader(buffer, bytes) {}

    bool SerializeBits(uint32_t& value, int bits) {
        value = reader.ReadBits(bits);
        return !reader.Error();
    }

    bool Error() const { return reader.Error(); }

    BitReader reader;
};

// Counts bits without touching memory. Used to compute wire sizes.
struct MeasureStream {
    static constexpr bool IsWriting = true;

    bool SerializeBits(uint32_t&, int bitCount) {
        bits += bitCount;
        return true;
    }

    uint32_t BytesWritten() const { return (bits + 7) / 8; }
    bool Error() const { return false; }

    uint32_t bits = 0;
};
