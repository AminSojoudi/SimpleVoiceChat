cmake_minimum_required(VERSION 3.26)
project(VoiceChatClient)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)


set(CMAKE_CXX_STANDARD 17)

add_executable(VoiceChatClient main.cpp
        SocketClient.cpp
        SocketClient.h
        AudioTools.cpp
        AudioTools.h
        Headers.h)

target_link_libraries(VoiceChatClient GameNetworkingSockets::shared portaudio_static)