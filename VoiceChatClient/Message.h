//
// Created by Amin on 11/14/23.
//

#ifndef VOICECHATCLIENT_MESSAGE_H
#define VOICECHATCLIENT_MESSAGE_H



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

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <thread>
#include "queue"
#include "rtaudio/RtAudio.h"
#include <queue>
#include <cassert>
#include "ConcurrentBag.hpp"


typedef uint16 AUDIO_SAMPLE;
#define BufferSize 256
#define NetworkBufferSize 4096

struct AudioData{
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


struct NetworkBuffer{
    ConcurrentBag<AUDIO_SAMPLE> buffer;
    
    NetworkBuffer() : buffer(NetworkBufferSize) {}

    void AddInput(AUDIO_SAMPLE sample){
        if (buffer.Size() >= NetworkBufferSize)
            printf("buffer is full \r\n");
        buffer.Add(sample);
    }
    
    bool BufferIsFull(){
        return buffer.Size() == NetworkBufferSize;
    }
    
    size_t Size() const{
        return buffer.Size();
    }
    
    std::optional<AUDIO_SAMPLE> ReadAt(size_t i){
        return buffer.GetAt(i);
    }
    
    void ResetData(){
        buffer.Reset();
    }

    void RemoveFirstItems(int numbers) {
        buffer.Erase(numbers);
    }
};


#endif //VOICECHATCLIENT_MESSAGE_H

