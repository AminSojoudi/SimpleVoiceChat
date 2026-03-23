cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VoiceChatVersion.cmake")

project(VoiceChatServer VERSION "${VOICECHAT_VERSION}")

find_package(GameNetworkingSockets CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)


add_executable(VoiceChatServer main.cpp
        Server.cpp
        Server.h
         "../Common/Messages/AudioMessage.h" "../Common/Messages/MessageTypes.h" "../Common/Messages/SetChannelMessage.h")

target_link_libraries(VoiceChatServer GameNetworkingSockets::shared)
target_compile_definitions(VoiceChatServer PRIVATE
    STEAMNETWORKINGSOCKETS_OPENSOURCE
    "VOICECHAT_PROTOCOL_VERSION_STRING=\"${VOICECHAT_VERSION}\""
)
