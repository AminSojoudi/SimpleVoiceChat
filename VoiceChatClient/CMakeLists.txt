cmake_minimum_required(VERSION 3.20)
project(VoiceChatClient LANGUAGES CXX)

find_package(GameNetworkingSockets CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)

set(_VOICECHAT_IOS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_VOICECHAT_IOS TRUE)
endif()

# RtAudio is only for the desktop CLI client, not the Unity plugin.
if(NOT ANDROID AND NOT _VOICECHAT_IOS)
    find_package(RtAudio CONFIG REQUIRED)
endif()

set(VOICECHAT_PLUGIN_SOURCES
    VoiceChatClientPlugin.cpp
    SocketClient.cpp
)

function(voicechat_apply_oss_gns target_name)
    target_compile_definitions(${target_name} PRIVATE STEAMNETWORKINGSOCKETS_OPENSOURCE)
    target_link_libraries(${target_name} PRIVATE GameNetworkingSockets::shared)
endfunction()

if(NOT ANDROID AND NOT _VOICECHAT_IOS)
    add_executable(VoiceChatClient
        main.cpp
        AudioTools.cpp
        AudioTools.h
        SocketClient.cpp
        SocketClient.h
        Utils.h
        ConcurrentBag.hpp
    )
    voicechat_apply_oss_gns(VoiceChatClient)
    target_link_libraries(VoiceChatClient PRIVATE RtAudio::rtaudio)
endif()

if(_VOICECHAT_IOS)
    add_library(VoiceChatClientPlugin STATIC ${VOICECHAT_PLUGIN_SOURCES})
else()
    add_library(VoiceChatClientPlugin SHARED ${VOICECHAT_PLUGIN_SOURCES})
endif()

voicechat_apply_oss_gns(VoiceChatClientPlugin)

if(ANDROID)
    set_target_properties(VoiceChatClientPlugin PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "VoiceChatClientPlugin"
    )
endif()

set(UNITY_PLUGINS_PARENT "${CMAKE_CURRENT_SOURCE_DIR}/../UnityVoiceChatClient/Assets/Plugins" CACHE PATH "Unity Assets/Plugins (parent directory)")
option(VOICECHAT_COPY_PLUGIN_TO_UNITY "Copy VoiceChatClientPlugin into UnityVoiceChatClient/Assets/Plugins after build" ON)

if(VOICECHAT_COPY_PLUGIN_TO_UNITY)
    if(WIN32 AND NOT ANDROID)
        set(_PLUGIN_OUT_DIR "${UNITY_PLUGINS_PARENT}/x86_64")
    elseif(ANDROID)
        set(_PLUGIN_OUT_DIR "${UNITY_PLUGINS_PARENT}/Android/libs/${ANDROID_ABI}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_PLUGIN_OUT_DIR "${UNITY_PLUGINS_PARENT}/x86_64")
    elseif(_VOICECHAT_IOS)
        set(_PLUGIN_OUT_DIR "${UNITY_PLUGINS_PARENT}/iOS")
    elseif(APPLE)
        set(_PLUGIN_OUT_DIR "${UNITY_PLUGINS_PARENT}/macOS")
    endif()

    if(DEFINED _PLUGIN_OUT_DIR)
        add_custom_command(TARGET VoiceChatClientPlugin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_PLUGIN_OUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:VoiceChatClientPlugin>" "${_PLUGIN_OUT_DIR}/"
            COMMENT "VoiceChat: copying native plugin to ${_PLUGIN_OUT_DIR}"
        )
    endif()
endif()
