#include "Server.h"
#include "Config.h"


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

    ServerConfig config;
    if (auto exitCode = ConfigParser::Parse(argc, argv, config)) {
        return *exitCode;
    }

    Server* server = new Server();

    bool socket_success = server->StartServer(config.port, config.bindAddress, config.logLevel, config.sampleRate);

    while (socket_success && !shouldExit){

        server->PollIncomingMessages();
        server->PollConnectionStateChanges();
        printf("\r Total Received: %d Kb  Total Sent: %d Kb", server->GetRecievedBytes(), server->GetSentBytes());

        std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
    }
    // clean up
    if (socket_success){
        delete server;
    }
}