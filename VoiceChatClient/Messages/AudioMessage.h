//
// Created by Amin on 11/14/23.
//

#ifndef VOICECHATCLIENT_MESSAGE_H
#define VOICECHATCLIENT_MESSAGE_H


#include "MessageTypes.h"




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
#include <Ws2tcpip.h>
#include <string>
#else
#include <arpa/inet.h>
#endif



typedef uint16 AUDIO_SAMPLE;
#define BufferSize 256
#define NetworkBufferSize 4096

struct AudioData{
    MessageType type = AUDIO;
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


#endif //VOICECHATCLIENT_MESSAGE_H

