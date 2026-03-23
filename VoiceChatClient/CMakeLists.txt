cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VoiceChatVersion.cmake")

project(VoiceChatClient LANGUAGES CXX VERSION "${VOICECHAT_VERSION}")

find_package(GameNetworkingSockets CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)

set(VOICECHAT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VoiceChatPluginVersion.h.in"
    "${VOICECHAT_GENERATED_DIR}/VoiceChatPluginVersion.h"
    @ONLY
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/VoiceChatPluginVersion.txt" "${VOICECHAT_VERSION}\n")

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
    target_include_directories(VoiceChatClient PRIVATE "${VOICECHAT_GENERATED_DIR}")
    target_link_libraries(VoiceChatClient PRIVATE RtAudio::rtaudio)
endif()

if(_VOICECHAT_IOS)
    add_library(VoiceChatClientPlugin STATIC ${VOICECHAT_PLUGIN_SOURCES})
else()
    add_library(VoiceChatClientPlugin SHARED ${VOICECHAT_PLUGIN_SOURCES})
endif()

target_include_directories(VoiceChatClientPlugin PRIVATE "${VOICECHAT_GENERATED_DIR}")
voicechat_apply_oss_gns(VoiceChatClientPlugin)

if(ANDROID)
    set_target_properties(VoiceChatClientPlugin PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "VoiceChatClientPlugin"
    )
endif()

# Isolate VoiceChat binaries under Assets/Plugins/VoiceChat so other native plugins are not gitignored.
set(UNITY_VOICECHAT_PLUGINS "${CMAKE_CURRENT_SOURCE_DIR}/../UnityVoiceChatClient/Assets/Plugins/VoiceChat" CACHE PATH "Unity Assets/Plugins/VoiceChat directory")
option(VOICECHAT_COPY_PLUGIN_TO_UNITY "Copy VoiceChatClientPlugin into UnityVoiceChatClient/Assets/Plugins/VoiceChat after build" ON)

if(DEFINED VCPKG_INSTALLED_DIR)
    set(_VCPKG_INSTALLED_ROOT "${VCPKG_INSTALLED_DIR}")
elseif(DEFINED VCPKG_TARGET_TRIPLET)
    set(_VCPKG_INSTALLED_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
elseif(WIN32 AND NOT ANDROID)
    set(_VCPKG_INSTALLED_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
else()
    set(_VCPKG_INSTALLED_ROOT "")
endif()

if(VOICECHAT_COPY_PLUGIN_TO_UNITY)
    if(WIN32 AND NOT ANDROID)
        set(_PLUGIN_OUT_DIR "${UNITY_VOICECHAT_PLUGINS}/Windows/x86_64")
    elseif(ANDROID)
        set(_PLUGIN_OUT_DIR "${UNITY_VOICECHAT_PLUGINS}/Android/libs/${ANDROID_ABI}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_PLUGIN_OUT_DIR "${UNITY_VOICECHAT_PLUGINS}/Linux/x86_64")
    elseif(_VOICECHAT_IOS)
        set(_PLUGIN_OUT_DIR "${UNITY_VOICECHAT_PLUGINS}/iOS")
    elseif(APPLE)
        set(_PLUGIN_OUT_DIR "${UNITY_VOICECHAT_PLUGINS}/macOS")
    endif()

    if(DEFINED _PLUGIN_OUT_DIR)
        if(WIN32 AND NOT ANDROID AND _VCPKG_INSTALLED_ROOT)
            add_custom_command(TARGET VoiceChatClientPlugin POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_PLUGIN_OUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:VoiceChatClientPlugin>" "${_PLUGIN_OUT_DIR}/"
                COMMAND ${CMAKE_COMMAND}
                    "-DUNITY_PLUGIN_DLL=${_PLUGIN_OUT_DIR}/VoiceChatClientPlugin.dll"
                    "-DVCPKG_INSTALLED_ROOT=${_VCPKG_INSTALLED_ROOT}"
                    -DCONFIG=$<CONFIG>
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/voicechat_deploy_windows_dlls.cmake"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${UNITY_VOICECHAT_PLUGINS}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/VoiceChatPluginVersion.txt" "${UNITY_VOICECHAT_PLUGINS}/VoiceChatPluginVersion.txt"
                COMMENT "VoiceChat: copy plugin, vcpkg runtime DLLs, and version into Unity"
            )
        else()
            add_custom_command(TARGET VoiceChatClientPlugin POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_PLUGIN_OUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:VoiceChatClientPlugin>" "${_PLUGIN_OUT_DIR}/"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${UNITY_VOICECHAT_PLUGINS}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/VoiceChatPluginVersion.txt" "${UNITY_VOICECHAT_PLUGINS}/VoiceChatPluginVersion.txt"
                COMMENT "VoiceChat: copy native plugin and version into Unity"
            )
        endif()
    endif()
endif()
