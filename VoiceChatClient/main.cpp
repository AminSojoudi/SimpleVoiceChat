#include <iostream>
#include <chrono>
#include <cstdint>
#include "AudioTools.h"
#include "Config.h"

// How long we wait for the connection and the server info before giving up.
const int HandshakeTimeoutSeconds = 30;

int main(int argc, const char *argv[] ) {

    ClientConfig config;
    if (auto exitCode = ConfigParser::Parse(argc, argv, config)) {
        return *exitCode;
    }

    in_addr buf;
    SteamNetworkingIPAddr serverAddress;
    NetworkBuffer networkBuffer;

    if (!inet_pton(AF_INET, config.serverAddress.c_str(), &buf)) {
        printf("failed to set address\n");
        return 1;
    }
    serverAddress.SetIPv4(htonl(buf.s_addr), config.port);

    char serverAddressString[INET_ADDRSTRLEN];
    buf.s_addr = htonl(serverAddress.GetIPv4());

    inet_ntop(AF_INET, &buf, serverAddressString , INET_ADDRSTRLEN);

    printf("trying to connect to server %s on %d \n", serverAddressString , serverAddress.m_port);

    auto* audioTools = new AudioTools();

    bool quit = false;

    // create network socket
    auto clientSocket = new SocketClient(config.logLevel);

    if (!clientSocket->Connect(serverAddress)) {
        return 1;
    }

    auto syncInterval = std::chrono::milliseconds(config.syncIntervalMs);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(HandshakeTimeoutSeconds);

    // Wait for the connection to come up.
    while (!clientSocket->IsConnected()) {
        if (std::chrono::steady_clock::now() > deadline) {
            printf("timed out after %d seconds waiting to connect to the server\n", HandshakeTimeoutSeconds);
            return 1;
        }
        clientSocket->PollConnectionStateChanges();
        std::this_thread::sleep_for(syncInterval);
    }

    // Ask the server for its settings. We need the sample rate before we can open the audio stream.
    clientSocket->RequestServerInfo();

    while (!clientSocket->HasServerInfo()) {
        if (std::chrono::steady_clock::now() > deadline) {
            printf("timed out after %d seconds waiting for server info\n", HandshakeTimeoutSeconds);
            return 1;
        }
        clientSocket->PollIncomingMessages(&networkBuffer);
        clientSocket->PollConnectionStateChanges();
        std::this_thread::sleep_for(syncInterval);
    }

    // Don't trust the rate the server sent us blindly, we do maths with it below.
    unsigned int sampleRate = clientSocket->GetServerInfo().sampleRate;
    if (sampleRate < MinSampleRate || sampleRate > MaxSampleRate) {
        printf("server reported a sample rate of %u Hz, which is outside the supported range of %u to %u Hz\n",
               sampleRate, MinSampleRate, MaxSampleRate);
        return 1;
    }

    // Match the audio buffer to how often we poll the network, so playback never runs dry.
    // Work in 64 bits so a large sync interval cannot wrap around and pass the check below.
    uint64_t frames = (uint64_t)sampleRate * config.syncIntervalMs / 1000;

    // A buffer bigger than the audio message can hold would drop samples.
    if (frames < 1 || frames > AudioData::Capacity) {
        printf("sync interval of %u ms does not work with the server sample rate of %u Hz: "
               "it needs %llu frames per buffer, but only 1 to %zu are supported\n",
               config.syncIntervalMs, sampleRate, (unsigned long long)frames, AudioData::Capacity);
        return 1;
    }

    unsigned int bufferFrames = (unsigned int)frames;

    SetChannel message;
    message.channel = config.channel;

    clientSocket->Send(&message, sizeof(message));

    if (!audioTools->StartRecording(clientSocket, &networkBuffer, sampleRate, bufferFrames)) {
        return 1;
    }

    while (!quit){

        clientSocket->PollIncomingMessages(&networkBuffer);
        clientSocket->PollConnectionStateChanges();

        std::this_thread::sleep_for(syncInterval);

    }


    return 0;
}
