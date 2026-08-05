cmake_minimum_required(VERSION 3.20)
project(VoiceChatServer)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)


add_executable(VoiceChatServer main.cpp
        Server.cpp
        Server.h
        Config.cpp
        Config.h
         "../Common/Messages/AudioMessage.h" "../Common/Messages/MessageTypes.h" "../Common/Messages/ClientConfigMessage.h" "../Common/Messages/ServerInfoMessage.h" "../Common/Messages/ServerInfoRequestMessage.h" "../Common/Messages/ClientListMessages.h" "../Common/Serialization/BitStream.h" "../Common/Serialization/Streams.h" "../Common/Serialization/Serialize.h" "../Common/Serialization/AudioStats.h")

target_link_libraries(VoiceChatServer GameNetworkingSockets::shared cxxopts::cxxopts)
