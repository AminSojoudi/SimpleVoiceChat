//
// Created by Amin on 10/29/23.
//

#include "AudioTools.h"

#define FRAMES_PER_BUFFER (512)
#define SAMPLE_RATE   (22500)

AudioData data;
SocketClient* clientSocket;

int counter = 0;

void DoNetwork(const AudioData &_data, AUDIO_SAMPLE* _voiceOutputBuffer){

    clientSocket->PollIncomingMessages(_voiceOutputBuffer);
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

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
int AudioTools::patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    unsigned int i;

    const auto *input = (const AUDIO_SAMPLE*)inputBuffer;
    auto *output = (AUDIO_SAMPLE*) outputBuffer;
    auto * _data = static_cast<AudioData*>(userData);

    if (_data == nullptr)
        return 0;

    _data->sampleCounter++;

    if (_data->sampleCounter > SAMPLE_RATE){
        _data->sampleCounter = 0;
    }

    for( i=0; i<framesPerBuffer; i++ )
    {
        //output[i] = input[i];     // loop back

        // recording
        _data->AddInput(input[i]);
    }

    if (_data->sampleCounter % 1 == 0){
        DoNetwork(data, output);
        // clear input queue
        _data->inputCurrentCounter = 0;
    }

    return 0;
}


bool AudioTools::StartRecording(SteamNetworkingIPAddr serverAddress) {

    // create network socket
    clientSocket = new SocketClient();

    clientSocket->Connect(serverAddress);

    err = Pa_Initialize();
    if( err != paNoError ){
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return false;
    }


    /* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                1,       /* 1 input channel */
                                1,     /* stereo output */
                                paInt16,  /* 32 bit floating point output */
                                SAMPLE_RATE,
                                FRAMES_PER_BUFFER, /* frames per buffer i.e. the number
                                                   of sample frames that PortAudio will
                                                   request from the callback. Many apps
                                                   may want to use
                                                   paFramesPerBufferUnspecified, which
                                                   tells PortAudio to pick the best,
                                                   possibly changing, buffer size.*/
                                patestCallback, /* this is your callback function */
                                &data ); /*This is a pointer that will be passed to your callback*/

    if( err != paNoError ){
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return false;
    };

    err = Pa_StartStream( stream );
    if( err != paNoError ){
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return false;
    }

    return true;
}

AudioTools::~AudioTools() {
    err = Pa_Terminate();
    if( err != paNoError )
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );

}

bool AudioTools::StopRecording() {
    err = Pa_StopStream( stream );
    if( err != paNoError ) {
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return false;
    }
    err = Pa_CloseStream( stream );
    if( err != paNoError ){
        printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
        return false;
    }
    printf("Recording finished.\n");

    return true;
}

