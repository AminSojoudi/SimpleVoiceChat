#pragma once

#include "Streams.h"

// The wire format is explicit little-endian. Big-endian hosts are not
// supported; fail the build instead of producing a broken peer.
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "Big-endian targets are not supported by the wire format."
#endif

// Field helpers. These are the only intended call surface over the streams.
// Every helper returns false when the stream has failed; callers chain them
// with early returns.

template<typename S>
bool serialize_uint8(S& stream, uint8_t& value) {
    uint32_t bits = value;
    if (!stream.SerializeBits(bits, 8))
        return false;
    if constexpr (!S::IsWriting)
        value = static_cast<uint8_t>(bits);
    return true;
}

template<typename S>
bool serialize_uint16(S& stream, uint16_t& value) {
    uint32_t bits = value;
    if (!stream.SerializeBits(bits, 16))
        return false;
    if constexpr (!S::IsWriting)
        value = static_cast<uint16_t>(bits);
    return true;
}

template<typename S>
bool serialize_uint32(S& stream, uint32_t& value) {
    uint32_t bits = value;
    if (!stream.SerializeBits(bits, 32))
        return false;
    if constexpr (!S::IsWriting)
        value = bits;
    return true;
}

// Two 32-bit halves, low half first.
template<typename S>
bool serialize_int64(S& stream, int64_t& value) {
    uint32_t low = static_cast<uint32_t>(static_cast<uint64_t>(value));
    uint32_t high = static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32);
    if (!stream.SerializeBits(low, 32))
        return false;
    if (!stream.SerializeBits(high, 32))
        return false;
    if constexpr (!S::IsWriting)
        value = static_cast<int64_t>((static_cast<uint64_t>(high) << 32) | low);
    return true;
}

// Reads fail outside [min, max]. Payload is untrusted even from a valid peer.
template<typename S>
bool serialize_uint32_range(S& stream, uint32_t& value, uint32_t min, uint32_t max) {
    if (!serialize_uint32(stream, value))
        return false;
    if constexpr (!S::IsWriting) {
        if (value < min || value > max)
            return false;
    }
    return true;
}

// Fixed 32 bytes on the wire. Bytes after the terminator go out as zeros, and
// the read side always forces buf[31] = 0.
constexpr uint32_t SerializedNameBytes = 32;

template<typename S>
bool serialize_name(S& stream, char* buf) {
    bool ended = false;
    for (uint32_t i = 0; i < SerializedNameBytes; ++i) {
        uint32_t bits = 0;
        if constexpr (S::IsWriting) {
            if (buf[i] == '\0')
                ended = true;
            bits = ended ? 0 : static_cast<uint8_t>(buf[i]);
        }
        if (!stream.SerializeBits(bits, 8))
            return false;
        if constexpr (!S::IsWriting)
            buf[i] = static_cast<char>(bits);
    }
    if constexpr (!S::IsWriting)
        buf[SerializedNameBytes - 1] = '\0';
    return true;
}

// The audio sample array's encoding lives here and only here. Future
// candidates, none implemented now — measure real traffic first
// (VOICECHAT_AUDIO_STATS): narrower fixed width, per-value size-class
// tags, presence bitmask, delta vs previous packet, codec (Opus).
template<typename S>
bool serialize_sample_array(S& stream, uint16_t* samples, uint32_t& count, uint32_t capacity) {
    if (!serialize_uint32_range(stream, count, 0, capacity))
        return false;
    for (uint32_t i = 0; i < count; ++i) {
        if (!serialize_uint16(stream, samples[i]))
            return false;
    }
    return true;
}
