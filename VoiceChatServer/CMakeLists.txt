cmake_minimum_required(VERSION 3.20)
project(VoiceChatServer)

find_package( GameNetworkingSockets CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(VoiceChatServer main.cpp
        Server.cpp
        Server.h
        MessageType.h)

target_link_libraries(VoiceChatServer GameNetworkingSockets::shared)
