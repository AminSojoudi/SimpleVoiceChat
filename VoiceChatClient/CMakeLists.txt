cmake_minimum_required(VERSION 3.20)
project(VoiceChatClient)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(rtaudio CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)


set(CMAKE_CXX_STANDARD 17)

add_executable(VoiceChatClient main.cpp
        SocketClient.cpp
        SocketClient.h
        AudioTools.cpp
        AudioTools.h
        Config.cpp
        Config.h
        "../Common/Messages/AudioMessage.h" "../Common/Messages/SetChannelMessage.h" "../Common/Messages/MessageTypes.h" "../Common/Messages/ServerInfoMessage.h" "../Common/Messages/ServerInfoRequestMessage.h" "Utils.h")

target_link_libraries(VoiceChatClient GameNetworkingSockets::shared RtAudio::rtaudio cxxopts::cxxopts)