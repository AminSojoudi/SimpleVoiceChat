find_package(cxxopts CONFIG REQUIRED)

add_executable(VoiceChatServer main.cpp
        Server.cpp
        Server.h
        Config.cpp
        Config.h
         "../Common/Messages/AudioMessage.h" "../Common/Messages/MessageTypes.h" "../Common/Messages/ClientConfigMessage.h" "../Common/Messages/ServerInfoMessage.h" "../Common/Messages/ServerInfoRequestMessage.h" "../Common/Messages/ClientListMessages.h" "../Common/Serialization/BitStream.h" "../Common/Serialization/Streams.h" "../Common/Serialization/Serialize.h" "../Common/Serialization/AudioStats.h")

target_link_libraries(VoiceChatServer ${GNS_LIB} cxxopts::cxxopts)
