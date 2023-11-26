//
// Created by Amin on 11/14/23.
//

#ifndef VOICECHATCLIENT_HEADERS_H
#define VOICECHATCLIENT_HEADERS_H



#define PLATFORM_WINDOWS  1
#define PLATFORM_MAC      2
#define PLATFORM_UNIX     3

#if defined(_WIN32)
#define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PLATFORM PLATFORM_MAC
#else
#define PLATFORM PLATFORM_UNIX
#endif


#if PLATFORM == PLATFORM_WINDOWS
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <thread>
#include "queue"
#include "portaudio.h"
#include <queue>
#include <cassert>

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

