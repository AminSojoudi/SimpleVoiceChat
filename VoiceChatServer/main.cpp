#include "Server.h"

int main(int argc, const char *argv[] )
{
    Server* server = new Server();
    bool success = server->StartServer(27020);

    while (success){

        server->PollIncomingMessages();
        server->PollConnectionStateChanges();

        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

    }
}