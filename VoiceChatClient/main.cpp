#include <iostream>
#include "AudioTools.h"
#include "queue"

int main() {
    std::cout << "Hello, World!" << std::endl;

    auto* audioTools = new AudioTools();


    bool quit = false;

    audioTools->StartRecording();


    while (!quit){

        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

    }


    return 0;
}
