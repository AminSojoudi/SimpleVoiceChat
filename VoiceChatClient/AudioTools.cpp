//
// Created by Amin on 10/29/23.
//

#include "AudioTools.h"

#define FRAMES_PER_BUFFER (512)
#define SAMPLE_RATE   (44100)

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

    printf("sending data with size %d : \n", _data.inputCurrentCounter);
    printf("send counter is %d \n", ++counter);
    for (int i = 0; i < _data.inputCurrentCounter; ++i) {
        printf("%d," , _data.Input[i]);
    }
    printf("\n");

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
            output[i] = *value;
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
        std::vector<unsigned int> deviceIds = audio.getDeviceIds();
        if (deviceIds.size() < 1) {
            std::cout << "\nNo audio devices found!\n";
            exit(0);
        }

        RtAudio::StreamParameters parameters;
        parameters.deviceId = audio.getDefaultInputDevice();
        parameters.nChannels = 2;
        parameters.firstChannel = 0;
        unsigned int sampleRate = 44100;
        unsigned int bufferFrames = 256; // 256 sample frames

        if (audio.openStream(NULL, &parameters, RTAUDIO_SINT16,
            sampleRate, &bufferFrames, &record)) {
            std::cout << '\n' << audio.getErrorText() << '\n' << std::endl;
            exit(0); // problem with device settings
        }

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

