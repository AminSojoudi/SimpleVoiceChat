#include <iostream>
#include "AudioTools.h"
#include "queue"

int main(int argc, const char *argv[] ) {

    in_addr buf;
    SteamNetworkingIPAddr serverAddress;
    serverAddress.SetIPv4(2130706433, 27020);   // localhost

    if (argc == 4)
    {
        if (inet_pton(AF_INET, argv[1], &buf)){
            serverAddress.SetIPv4(htonl(buf.s_addr), std::stoi(argv[2]));
        } else{
            printf("failed to set address\n");
            return 1;
        }
    }
    else
    {
        printf("usage: ./client [serverAddress] [port] [logLevel] \n");
        printf("Server add and port not defined, using defaults \n");
    }

    char serverAddressString[INET_ADDRSTRLEN];
    buf.s_addr = htonl(serverAddress.GetIPv4());

    inet_ntop(AF_INET, &buf, serverAddressString , INET_ADDRSTRLEN);

    printf("trying to connect to server %s on %d \n", serverAddressString , serverAddress.m_port);



    auto* audioTools = new AudioTools();


    bool quit = false;

    audioTools->StartRecording(serverAddress);


    while (!quit){

        std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );

    }


    return 0;
}
