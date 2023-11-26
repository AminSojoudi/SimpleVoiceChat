#include "Server.h"

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