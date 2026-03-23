# Deploy vcpkg runtime DLLs next to VoiceChatClientPlugin.dll (Windows).
# Invoked with: cmake -P voicechat_deploy_windows_dlls.cmake
#   -DUNITY_PLUGIN_DLL=<full path to VoiceChatClientPlugin.dll in Unity tree>
#   -DVCPKG_INSTALLED_ROOT=<vcpkg_installed/<triplet> root>
#   -DCONFIG=<Release|Debug|...> (Visual Studio multi-config; empty => Release layout)

if(NOT UNITY_PLUGIN_DLL)
    message(FATAL_ERROR "UNITY_PLUGIN_DLL not set")
endif()
if(NOT EXISTS "${UNITY_PLUGIN_DLL}")
    message(WARNING "VoiceChat: plugin DLL not found yet at ${UNITY_PLUGIN_DLL}; skip vcpkg deploy")
    return()
endif()
if(NOT DEFINED ENV{VCPKG_ROOT})
    message(STATUS "VoiceChat: VCPKG_ROOT not set; skip vcpkg DLL deploy")
    return()
endif()
if(NOT VCPKG_INSTALLED_ROOT)
    message(WARNING "VoiceChat: VCPKG_INSTALLED_ROOT not set; skip vcpkg DLL deploy")
    return()
endif()

if(CONFIG STREQUAL "Debug")
    set(_vcpkg_bin "${VCPKG_INSTALLED_ROOT}/debug/bin")
else()
    set(_vcpkg_bin "${VCPKG_INSTALLED_ROOT}/bin")
endif()

if(NOT EXISTS "${_vcpkg_bin}")
    message(WARNING "VoiceChat: vcpkg bin directory not found: ${_vcpkg_bin}")
    return()
endif()

set(_applocal "$ENV{VCPKG_ROOT}/scripts/buildsystems/msbuild/applocal.ps1")
if(NOT EXISTS "${_applocal}")
    message(WARNING "VoiceChat: applocal.ps1 not found at ${_applocal}")
    return()
endif()

find_program(_VOICECHAT_PWSH NAMES pwsh powershell REQUIRED)

execute_process(
    COMMAND "${_VOICECHAT_PWSH}" -NoProfile -ExecutionPolicy Bypass
        -File "${_applocal}"
        -targetBinary "${UNITY_PLUGIN_DLL}"
        -installedDir "${_vcpkg_bin}"
    RESULT_VARIABLE _ec
)
if(NOT _ec EQUAL 0)
    message(WARNING "VoiceChat: applocal.ps1 failed (exit ${_ec}); copy vcpkg DLLs manually from ${_vcpkg_bin}")
endif()
