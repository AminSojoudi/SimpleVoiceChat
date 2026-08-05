#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>

// Bit-level writer and reader. The writer packs bits LSB-first into a 64-bit
// scratch word and flushes full 32-bit words as little-endian bytes. Fields
// written at byte-multiple widths land as plain little-endian bytes, so today
// the wire looks byte-level. The bit core exists so later compression can use
// odd widths without changing any call sites.

class BitWriter {
public:
    BitWriter(void* buffer, uint32_t bytes)
        : m_buffer(static_cast<uint8_t*>(buffer)), m_capacityBits(bytes * 8) {}

    // bits must be in [1,32]. Sets the error flag instead of overflowing.
    void WriteBits(uint32_t value, int bits) {
        assert(bits >= 1 && bits <= 32);
        if (m_error)
            return;
        if (m_bitsWritten + bits > m_capacityBits) {
            m_error = true;
            return;
        }
        if (bits < 32)
            value &= (1u << bits) - 1;
        m_scratch |= static_cast<uint64_t>(value) << m_scratchBits;
        m_scratchBits += bits;
        m_bitsWritten += bits;
        if (m_scratchBits >= 32) {
            // A full word only accumulates when those 32 bits fit in the
            // capacity, so this 4-byte copy never passes the buffer end.
            uint32_t word = static_cast<uint32_t>(m_scratch);
            memcpy(m_buffer + m_byteIndex, &word, 4);
            m_byteIndex += 4;
            m_scratch >>= 32;
            m_scratchBits -= 32;
        }
    }

    // Flush the final partial word. Writes only the bytes that hold data, so
    // a buffer length that is not a multiple of 4 stays safe.
    void FlushBits() {
        if (m_error || m_scratchBits == 0)
            return;
        uint32_t tailBytes = (m_scratchBits + 7) / 8;
        uint64_t scratch = m_scratch;
        for (uint32_t i = 0; i < tailBytes; ++i) {
            m_buffer[m_byteIndex + i] = static_cast<uint8_t>(scratch & 0xFF);
            scratch >>= 8;
        }
        m_byteIndex += tailBytes;
        m_scratch = 0;
        m_scratchBits = 0;
    }

    // Exact byte count, valid after FlushBits.
    uint32_t BytesWritten() const { return (m_bitsWritten + 7) / 8; }

    bool Error() const { return m_error; }

private:
    uint8_t* m_buffer;
    uint32_t m_capacityBits;
    uint64_t m_scratch = 0;
    int m_scratchBits = 0;
    uint32_t m_byteIndex = 0;
    uint32_t m_bitsWritten = 0;
    bool m_error = false;
};


class BitReader {
public:
    BitReader(const void* buffer, uint32_t bytes)
        : m_buffer(static_cast<const uint8_t*>(buffer)), m_capacityBytes(bytes),
          m_totalBits(bytes * 8) {}

    // bits must be in [1,32]. Returns 0 and sets the error flag past the end.
    uint32_t ReadBits(int bits) {
        assert(bits >= 1 && bits <= 32);
        if (m_error)
            return 0;
        if (m_bitsRead + bits > m_totalBits) {
            m_error = true;
            return 0;
        }
        while (m_scratchBits < bits) {
            // Refill one word. Read the tail byte by byte so a length that is
            // not a multiple of 4 never over-reads.
            uint32_t remaining = m_capacityBytes - m_byteIndex;
            uint32_t take = remaining < 4 ? remaining : 4;
            uint32_t word = 0;
            memcpy(&word, m_buffer + m_byteIndex, take);
            m_byteIndex += take;
            m_scratch |= static_cast<uint64_t>(word) << m_scratchBits;
            m_scratchBits += take * 8;
        }
        uint32_t value = static_cast<uint32_t>(
            m_scratch & (bits == 32 ? 0xFFFFFFFFull : ((1ull << bits) - 1)));
        m_scratch >>= bits;
        m_scratchBits -= bits;
        m_bitsRead += bits;
        return value;
    }

    bool Error() const { return m_error; }

private:
    const uint8_t* m_buffer;
    uint32_t m_capacityBytes;
    uint32_t m_totalBits;
    uint64_t m_scratch = 0;
    int m_scratchBits = 0;
    uint32_t m_byteIndex = 0;
    uint32_t m_bitsRead = 0;
    bool m_error = false;
};
