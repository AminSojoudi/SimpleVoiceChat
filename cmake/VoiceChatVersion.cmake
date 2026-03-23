# Shared protocol / native plugin version (read from repo root VERSION).
# Include from VoiceChatClient or VoiceChatServer CMakeLists.txt before or after project().
get_filename_component(VOICECHAT_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(VOICECHAT_VERSION_FILE "${VOICECHAT_REPO_ROOT}/VERSION")

if(DEFINED ENV{VOICECHAT_VERSION_OVERRIDE} AND NOT "$ENV{VOICECHAT_VERSION_OVERRIDE}" STREQUAL "")
    set(VOICECHAT_VERSION "$ENV{VOICECHAT_VERSION_OVERRIDE}")
elseif(EXISTS "${VOICECHAT_VERSION_FILE}")
    file(READ "${VOICECHAT_VERSION_FILE}" VOICECHAT_VERSION)
    string(STRIP "${VOICECHAT_VERSION}" VOICECHAT_VERSION)
else()
    set(VOICECHAT_VERSION "0.0.0-dev")
endif()

message(STATUS "VoiceChat version (protocol / native plugin): ${VOICECHAT_VERSION}")
