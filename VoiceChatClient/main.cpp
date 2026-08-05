#include <iostream>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <atomic>
#include "AudioTools.h"
#include "Config.h"
#include "../Common/Serialization/AudioStats.h"

// How long we wait for the connection and the server info before giving up.
const int HandshakeTimeoutSeconds = 30;

// Set by Ctrl+C so the main loop can exit and clean up.
static std::atomic<bool> g_quit{false};

static void OnSignal(int) {
    g_quit = true;
}

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
        // The server may reject us, e.g. on a protocol version mismatch. The
        // connection callback already printed the server's reason.
        if (!clientSocket->IsConnected()) {
            printf("disconnected while waiting for server info\n");
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

    uint32 serverProtocol = clientSocket->GetServerInfo().protocolVersion;
    if (serverProtocol != PROTOCOL_VERSION) {
        printf("server protocol %u does not match client protocol %u\n", serverProtocol, PROTOCOL_VERSION);
        return 1;
    }

    // Match the audio buffer to how often we poll the network, so playback never runs dry.
    // Work in 64 bits so a large sync interval cannot wrap around and pass the check below.
    uint64_t frames = (uint64_t)sampleRate * config.syncIntervalMs / 1000;

    // The capture side splits large buffers into several audio messages and
    // the playback buffer scales with the interval, so any interval in the
    // allowed 1 to 1000 ms range works.
    if (frames < 1) {
        printf("sync interval of %u ms is too short for the server sample rate of %u Hz\n",
               config.syncIntervalMs, sampleRate);
        return 1;
    }

    unsigned int bufferFrames = (unsigned int)frames;

    // Give playback a few intervals of headroom before audio starts, but
    // never less than a tenth of a second of samples.
    size_t playbackCapacity = (size_t)frames * PlaybackSlackIntervals;
    size_t playbackFloor = sampleRate / PlaybackSlackMinFractionOfSecond;
    if (playbackCapacity < playbackFloor)
        playbackCapacity = playbackFloor;
    networkBuffer.SetCapacity(playbackCapacity);

    SetClientConfig message;
    message.channel = config.channel;
    message.loopback = config.loopback ? 1 : 0;
    message.muted = 0;
    snprintf(message.name, sizeof(message.name), "%s", config.name.c_str());

    uint8_t configBuf[SetClientConfig::MaxWireSize];
    WriteStream configStream(configBuf, sizeof(configBuf));
    if (!message.Serialize(configStream)) {
        printf("failed to encode client config\n");
        return 1;
    }
    configStream.Flush();
    clientSocket->Send(configBuf, configStream.BytesWritten());

    if (!audioTools->StartRecording(clientSocket, &networkBuffer, sampleRate, bufferFrames)) {
        return 1;
    }

    std::signal(SIGINT, OnSignal);

    while (!quit && !g_quit){

        clientSocket->PollIncomingMessages(&networkBuffer);
        clientSocket->PollConnectionStateChanges();

        std::this_thread::sleep_for(syncInterval);

    }

    audioTools->StopRecording();
    delete clientSocket;
    VOICECHAT_AUDIO_STATS_PRINT();

    return 0;
}
