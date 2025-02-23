//
// Created by Amin on 10/29/23.
//

#ifndef VOICECHATCLIENT_AUDIOTOOLS_H
#define VOICECHATCLIENT_AUDIOTOOLS_H

#include "Messages/AudioMessage.h"
#include "SocketClient.h"
#include "rtaudio/RtAudio.h"


class AudioTools {
private:
    RtAudio audio;

    //int record(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData);

public:
    bool StartRecording(SteamNetworkingIPAddr serverAddress, std::string topic);
    bool StopRecording();
    ~AudioTools();
};


#endif //VOICECHATCLIENT_AUDIOTOOLS_H
