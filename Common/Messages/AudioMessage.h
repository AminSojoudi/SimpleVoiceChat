//
// Created by Amin on 11/14/23.
//

#pragma once


#include "MessageTypes.h"
#include "stdio.h"
#include <cstddef>



typedef uint16 AUDIO_SAMPLE;

// sampleCount comes before Input so we can send just the samples we captured.
// Everything a reader needs sits before the array, so a short message is still readable.
struct AudioData{
    static constexpr size_t Capacity = 1024;

    uint8_t type = AUDIO;
    uint32 sampleCount = 0;
    AUDIO_SAMPLE Input[Capacity];

    void AddInput(AUDIO_SAMPLE sample){
        if (sampleCount < Capacity)
            Input[sampleCount++] = sample;
        else
            printf("buffer is full \r\n");
    }

    void ResetData(){
        sampleCount = 0;
    }

    // Bytes to send: the header plus only the samples we captured.
    uint32 WireSize() const {
        return HeaderSize() + sampleCount * sizeof(AUDIO_SAMPLE);
    }

    // Bytes before the sample array. A message shorter than this cannot be read.
    static uint32 HeaderSize() {
        return offsetof(AudioData, Input);
    }
};
