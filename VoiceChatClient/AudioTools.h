//
// Created by Amin on 10/29/23.
//

#ifndef VOICECHATCLIENT_AUDIOTOOLS_H
#define VOICECHATCLIENT_AUDIOTOOLS_H

#include "Message.h"
#include "SocketClient.h"


class AudioTools {
private:
    PaStream *stream;
    PaError err;

    static int patestCallback( const void *inputBuffer, void *outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void *userData );

public:
    bool StartRecording(SteamNetworkingIPAddr serverAddress);
    bool StopRecording();
    ~AudioTools();
};


#endif //VOICECHATCLIENT_AUDIOTOOLS_H
