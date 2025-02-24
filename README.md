
[![Windows CI](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-windows-platform.yml/badge.svg?branch=main)](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-windows-platform.yml)
[![Ubuntu CI](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-ubuntu-platform.yml/badge.svg?branch=main)](https://github.com/AminSojoudi/SimpeVoiceChat/actions/workflows/cmake-ubuntu-platform.yml)
# SimpeVoiceChat
Simple C++ UDP based Voice Chat Application using Valve [Game Network Sockets](https://github.com/ValveSoftware/GameNetworkingSockets) and [RTAudio](https://github.com/thestk/rtaudio)

# TODO
- [X] Make Client Multiplatform
- [X] Separate Network thread from Audio thread
- [ ] Improve Client/Server connection handling with proper logs
- [ ] Check Projects for memory leaks and consumption
- [X] improve architecture
- [ ] Dockerize Server and push to DockerHub
- [ ] Make Releases for Client in Github
- [X] Automate Builds
- [ ] Add log Level functionality to server and client
- [ ] improve resiliency to different network bandwidths and network changes
- [ ] Calculate bandwith usage
- [ ] Optimise bandwith
- [ ] Add some terminal voice visualization to client
