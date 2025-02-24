#include <iostream>
#include "AudioTools.h"

int main(int argc, const char *argv[] ) {

    in_addr buf;
    SteamNetworkingIPAddr serverAddress;
    NetworkBuffer networkBuffer;

    inet_pton(AF_INET, "127.0.0.1", &buf); // localhost as default address
    serverAddress.SetIPv4(htonl(buf.s_addr), 27020);

    std::string topic = "default";

    if (argc == 4)
    {
        if (inet_pton(AF_INET, argv[1], &buf)){
            serverAddress.SetIPv4(htonl(buf.s_addr), std::stoi(argv[2]));
        } else{
            printf("failed to set address\n");
            return 1;
        }
        topic = argv[4];
    }
    else
    {
        printf("usage: ./client [serverAddress] [port] [topic] \n");
        printf("Server add and port not defined, using defaults \n");
    }

    char serverAddressString[INET_ADDRSTRLEN];
    buf.s_addr = htonl(serverAddress.GetIPv4());

    inet_ntop(AF_INET, &buf, serverAddressString , INET_ADDRSTRLEN);

    printf("trying to connect to server %s on %d \n", serverAddressString , serverAddress.m_port);



    auto* audioTools = new AudioTools();


    bool quit = false;

    // create network socket
    auto clientSocket = new SocketClient();

    clientSocket->Connect(serverAddress, topic);

    SetTopicData message;
    message.topic = topic;

    clientSocket->Send(&message, sizeof(message));


    audioTools->StartRecording(clientSocket, &networkBuffer);


    while (!quit){

        clientSocket->PollIncomingMessages(&networkBuffer);
        clientSocket->PollConnectionStateChanges();


        std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );

    }


    return 0;
}
