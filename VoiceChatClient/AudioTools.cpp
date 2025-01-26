//
// Created by Amin on 10/29/23.
//

#include "AudioTools.h"

#define FRAMES_PER_BUFFER (512)
#define SAMPLE_RATE   (44100)

//typedef signed short MY_TYPE;
//#define FORMAT RTAUDIO_SINT16

AudioData data;
NetworkBuffer networkBuffer;
SocketClient* clientSocket;

int counter = 0;

void DoNetwork(const AudioData &_data){

    clientSocket->PollIncomingMessages(&networkBuffer);
    clientSocket->PollConnectionStateChanges();

    if (!clientSocket->IsConnected()){
        return;
    }

    // Only enable this part for debugging, any action here causes delays on the voice
    /*printf("sending data with size %d : \n", _data.inputCurrentCounter);
    printf("send counter is %d \n", ++counter);
    for (int i = 0; i < _data.inputCurrentCounter; ++i) {
        printf("%d," , _data.Input[i]);
    }
    printf("\n");*/

    clientSocket->Send(&_data, sizeof(_data));
}

int record(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames,
    double streamTime, RtAudioStreamStatus status, void* userData)
{
    if (status)
        std::cout << "Stream overflow detected!" << std::endl;

    // Do something with the data in the "inputBuffer" buffer.
    unsigned int i;

    const auto* input = (const AUDIO_SAMPLE*)inputBuffer;
    auto* output = (AUDIO_SAMPLE*)outputBuffer;
    auto* _data = static_cast<AudioData*>(userData);

    if (_data == nullptr)
        return 0;

    _data->sampleCounter++;

    if (_data->sampleCounter > SAMPLE_RATE) {
        _data->sampleCounter = 0;
    }

    for (i = 0; i < nBufferFrames; i++)
    {
        //output[i] = input[i];     // loop back

        // recording
        _data->AddInput(input[i]);
    }

    for (i = 0; i < networkBuffer.Size(); i++) {
        auto value = networkBuffer.ReadAt(i);
        if (value.has_value())
        {
            output[2 * i] = *value; // left
            output[2 * i + 1] = *value; // right
        }
    }
    networkBuffer.ResetData();

    //if (_data->BufferIsAlmostFull()){
    DoNetwork(data);
    // clear input queue
    _data->ResetData();
    //}

    return 0;
}


bool AudioTools::StartRecording(SteamNetworkingIPAddr serverAddress) {

    // create network socket
    clientSocket = new SocketClient();

    clientSocket->Connect(serverAddress);

    try
    {
        if (audio.getDeviceCount() < 1) {
            std::cerr << "No audio devices found!\n";
            return 1;
        }
        audio.showWarnings(true);

        // Parameters
        unsigned int sampleRate = 44100;
        unsigned int bufferFrames = 256; // frames per buffer
        RtAudio::StreamParameters iParams, oParams;

        // --- MONO INPUT ---
        iParams.deviceId = audio.getDefaultInputDevice();
        iParams.nChannels = 1;
        iParams.firstChannel = 0;

        // --- STEREO OUTPUT ---
        oParams.deviceId = audio.getDefaultOutputDevice();
        oParams.nChannels = 2;
        oParams.firstChannel = 0;

        // Options
        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_HOG_DEVICE | RTAUDIO_SCHEDULE_REALTIME;

        audio.openStream(
            &oParams, &iParams,
            RTAUDIO_SINT16, sampleRate,
            &bufferFrames,
            &record,
            &data
        );

        // Stream is open ... now start it.
        if (audio.startStream()) {
            std::cout << audio.getErrorText() << std::endl;
            this->StopRecording();
        }

        return true;

    }
    catch (const std::exception&)
    {
        return false;
    }
}

AudioTools::~AudioTools() {
    if (audio.isStreamOpen()) audio.closeStream();
}

bool AudioTools::StopRecording() {
    if (audio.isStreamRunning())
        audio.stopStream();  // or could call adc.abortStream();
    if (audio.isStreamOpen())
        audio.closeStream();
    printf("Recording finished.\n");
    return true;
}

