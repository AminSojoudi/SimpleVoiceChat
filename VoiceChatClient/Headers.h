//
// Created by Amin on 11/14/23.
//

#ifndef VOICECHATCLIENT_HEADERS_H
#define VOICECHATCLIENT_HEADERS_H

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <thread>
#include <arpa/inet.h>
#include "queue"
#include "portaudio.h"
#include <queue>

typedef short AUDIO_SAMPLE;
#define BufferSize 1024

struct AudioData{
    AUDIO_SAMPLE Input[BufferSize];
    int sampleCounter;
    int inputCurrentCounter = 0;

    AudioData(){
        sampleCounter = 0;
    }

    void AddInput(AUDIO_SAMPLE sample){
        if (inputCurrentCounter < BufferSize)
            Input[inputCurrentCounter++] = sample;
    }
};


#endif //VOICECHATCLIENT_HEADERS_H

