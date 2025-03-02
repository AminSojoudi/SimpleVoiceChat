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
    int inputCurrentCounter = -1;

    void AddInput(AUDIO_SAMPLE sample){
        if (inputCurrentCounter < BufferSize - 1)
            Input[++inputCurrentCounter] = sample;
        else
            printf("buffer is full \r\n");
    }
    
    void ResetData(){
        inputCurrentCounter = -1;
    }
};


#undef BufferSize

