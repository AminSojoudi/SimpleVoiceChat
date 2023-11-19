cmake_minimum_required(VERSION 3.26)
project(ValveGameServerTest)

find_package( GameNetworkingSockets CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(ValveGameServerTest main.cpp
        Server.cpp
        Server.h
        main2.cpp
        MessageType.h)

target_link_libraries(ValveGameServerTest GameNetworkingSockets::shared)
