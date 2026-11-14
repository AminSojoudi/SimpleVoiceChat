find_package(rtaudio CONFIG REQUIRED)

add_library(VoiceChatCore STATIC VoiceChatSession.cpp
        VoiceChatSession.h
        SampleRing.h
        "../Common/Messages/AudioMessage.h" "../Common/Messages/MessageTypes.h" "../Common/Messages/ClientConfigMessage.h" "../Common/Messages/ServerInfoMessage.h" "../Common/Messages/ServerInfoRequestMessage.h" "../Common/Messages/ClientListMessages.h" "../Common/Serialization/BitStream.h" "../Common/Serialization/Streams.h" "../Common/Serialization/Serialize.h" "../Common/Serialization/AudioStats.h")

target_include_directories(VoiceChatCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VoiceChatCore PRIVATE ${GNS_LIB} RtAudio::rtaudio)

add_executable(SampleRingTest SampleRingTest.cpp SampleRing.h)
