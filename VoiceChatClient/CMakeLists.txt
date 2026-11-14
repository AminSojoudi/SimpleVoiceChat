find_package(cxxopts CONFIG REQUIRED)

add_executable(VoiceChatClient main.cpp Config.cpp Config.h "../Common/Serialization/AudioStats.h")

target_link_libraries(VoiceChatClient VoiceChatCore cxxopts::cxxopts)
