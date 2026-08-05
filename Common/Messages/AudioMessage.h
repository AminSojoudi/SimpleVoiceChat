//
// Created by Amin on 11/14/23.
//

#pragma once


#include "MessageTypes.h"
#include "../Serialization/Serialize.h"
#include "stdio.h"


typedef uint16 AUDIO_SAMPLE;

// Wire: type u8, senderId u32, sampleCount u32 (range <= Capacity), samples 16 bits each.
// 9 bytes + 2 per sample.
struct AudioData{
    static constexpr size_t Capacity = 1024;
    static constexpr uint32 MaxWireSize = 9 + 2 * Capacity;

    uint8_t type = AUDIO;

    // Who is talking. Clients send 0; the server stamps the real id before
    // relaying. The serializer fixes this field at wire bytes 1-4 (little
    // endian) — the server patches those bytes in place on the hot path.
    uint32 senderId = 0;

    uint32 sampleCount = 0;
    AUDIO_SAMPLE Input[Capacity];

    template<typename Stream>
    bool Serialize(Stream& stream)
    {
        if (!serialize_uint8(stream, type)) return false;
        if (!serialize_uint32(stream, senderId)) return false;
        if (!serialize_sample_array(stream, Input, sampleCount, Capacity)) return false;
        return true;
    }

    void AddInput(AUDIO_SAMPLE sample){
        if (sampleCount < Capacity)
            Input[sampleCount++] = sample;
        else
            printf("buffer is full \r\n");
    }

    void ResetData(){
        sampleCount = 0;
    }
};
