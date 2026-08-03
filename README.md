
[![Windows CI](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-windows-platform.yml/badge.svg?branch=main)](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-windows-platform.yml)
[![Ubuntu CI](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-ubuntu-platform.yml/badge.svg?branch=main)](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-ubuntu-platform.yml)
# SimpeVoiceChat
Simple C++ UDP based Voice Chat Application using Valve [Game Network Sockets](https://github.com/ValveSoftware/GameNetworkingSockets) and [RTAudio](https://github.com/thestk/rtaudio)

# Quick Start

- Build and run the server using Docker

        docker build -t voice-chat-server .
        docker run -p 27020:27020/udp voice-chat-server

- Build and run the client to connect to the server

        ./VoiceChatClient --address 127.0.0.1 --port 27020 --channel 0 --log-level info --sync-interval 5

  The sample rate is a server setting (`--sample-rate` on `VoiceChatServer`, default 44100). Clients
  ask the server for it when they connect, so every client on a server uses the same rate. The client
  picks its own `--sync-interval` to trade latency against how often it polls the network.

  Both `VoiceChatServer` and `VoiceChatClient` accept `--help` for the full list of flags and defaults.


# TODO
- [X] Make Client Multiplatform
- [X] Separate Network thread from Audio thread
- [ ] Improve Client/Server connection handling with proper logs
- [ ] Check Projects for memory leaks and consumption
- [X] improve architecture
- [X] Dockerize Server and push to DockerHub
- [ ] Make Releases for Client in Github
- [X] Automate Builds
- [X] Add log Level functionality to server and client
- [ ] improve resiliency to different network bandwidths and network changes
- [X] Calculate bandwith usage
- [ ] Optimise bandwith
- [ ] Add some terminal voice visualization to client
