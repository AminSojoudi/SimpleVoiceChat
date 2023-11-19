//
// Created by Amin on 11/19/23.
//

#ifndef VALVEGAMESERVERTEST_MESSAGETYPE_H
#define VALVEGAMESERVERTEST_MESSAGETYPE_H

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


#endif //VALVEGAMESERVERTEST_MESSAGETYPE_H
