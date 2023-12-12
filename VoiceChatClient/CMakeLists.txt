# Windows -DPORTAUDIO_LINK=portaudio
# Linux/Mac -DPORTAUDIO_LINK=portaudio_static
cmake_minimum_required(VERSION 3.20)
project(VoiceChatClient)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)


set(CMAKE_CXX_STANDARD 17)

add_executable(VoiceChatClient main.cpp
        SocketClient.cpp
        SocketClient.h
        AudioTools.cpp
        AudioTools.h
        Message.h)

target_link_libraries(VoiceChatClient GameNetworkingSockets::shared ${PORTAUDIO_LINK})