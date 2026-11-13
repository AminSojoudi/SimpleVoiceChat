cmake_minimum_required(VERSION 3.20)

# Each switch turns on the program and its vcpkg feature (see vcpkg.json).
# vcpkg reads VCPKG_MANIFEST_FEATURES in project(), so set it before that.
option(VOICECHAT_BUILD_SERVER "Build the server" ON)
option(VOICECHAT_BUILD_CLIENT "Build the client" ON)
if(VOICECHAT_BUILD_SERVER)
    list(APPEND VCPKG_MANIFEST_FEATURES server)
endif()
if(VOICECHAT_BUILD_CLIENT)
    list(APPEND VCPKG_MANIFEST_FEATURES client)
endif()

project(SimpleVoiceChat)
set(CMAKE_CXX_STANDARD 17)

# Both programs use GNS. Its targets are global, so it can only be found once.
find_package(GameNetworkingSockets CONFIG REQUIRED)
# vcpkg builds it shared on Windows and static on Linux.
if(TARGET GameNetworkingSockets::shared)
    set(GNS_LIB GameNetworkingSockets::shared)
else()
    set(GNS_LIB GameNetworkingSockets::static)
endif()

if(VOICECHAT_BUILD_SERVER)
    add_subdirectory(VoiceChatServer)
endif()
if(VOICECHAT_BUILD_CLIENT)
    add_subdirectory(VoiceChatClient)
endif()
