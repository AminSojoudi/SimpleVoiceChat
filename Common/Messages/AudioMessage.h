//
// Created by Amin on 11/14/23.
//

#pragma once


#include "MessageTypes.h"
#include "stdio.h"



typedef uint16 AUDIO_SAMPLE;
#define BufferSize 1024

struct AudioData{
    uint8_t type = AUDIO;
    AUDIO_SAMPLE Input[BufferSize];
    int sampleCounter;
    int inputCurrentCounter = -1;

    AudioData(){
        sampleCounter = -1;
    }

    void AddInput(AUDIO_SAMPLE sample){
        if (inputCurrentCounter < BufferSize)
            Input[++inputCurrentCounter] = sample;
        else
            printf("buffer is full \r\n");
    }
    
    bool BufferIsAlmostFull(){
        if (inputCurrentCounter < BufferSize * 0.9) {
            return false;
        }
        return true;
    }
    
    void ResetData(){
        inputCurrentCounter = -1;
    }
};


#undef BufferSize

