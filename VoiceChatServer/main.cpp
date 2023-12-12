#include "Server.h"


bool shouldExit = false;

void signal_callback_handler(int signum) {
    // Terminate program
    printf("terminating voice server \n");
    shouldExit = true;
}

int main(int argc, const char *argv[] )
{
    // Register signal and signal handler
    signal(SIGINT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);

    Server* server = new Server();

    uint16 port = 27020;
    if (argc == 2){
        port = std::stoi(argv[1]);
    } else{
        printf("port not provided, using default port 27020 \n usage: ./server [port]");
    }

    bool socket_success = server->StartServer(port);

    while (socket_success && !shouldExit){

        server->PollIncomingMessages();
        server->PollConnectionStateChanges();

        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
    // clean up
    if (socket_success){
        delete server;
    }
}