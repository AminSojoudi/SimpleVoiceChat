#include "Server.h"


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

int main(int argc, const char *argv[] )
{
    Server* server = new Server();

    uint16 port = 27020;
    if (argc == 2){
        port = std::stoi(argv[1]);
    } else{
        printf("port not provided, using default port 27020 \n usage: ./server [port]");
    }

    bool success = server->StartServer(port);

    while (success){

        server->PollIncomingMessages();
        server->PollConnectionStateChanges();

        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

    }
}