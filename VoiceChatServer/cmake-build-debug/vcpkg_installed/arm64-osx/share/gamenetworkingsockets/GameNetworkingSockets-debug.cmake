#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "GameNetworkingSockets::GameNetworkingSockets" for configuration "Debug"
set_property(TARGET GameNetworkingSockets::GameNetworkingSockets APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(GameNetworkingSockets::GameNetworkingSockets PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libGameNetworkingSockets.dylib"
  IMPORTED_SONAME_DEBUG "@rpath/libGameNetworkingSockets.dylib"
  )

list(APPEND _cmake_import_check_targets GameNetworkingSockets::GameNetworkingSockets )
list(APPEND _cmake_import_check_files_for_GameNetworkingSockets::GameNetworkingSockets "${_IMPORT_PREFIX}/debug/lib/libGameNetworkingSockets.dylib" )

# Import target "GameNetworkingSockets::GameNetworkingSockets_s" for configuration "Debug"
set_property(TARGET GameNetworkingSockets::GameNetworkingSockets_s APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(GameNetworkingSockets::GameNetworkingSockets_s PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C;CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libGameNetworkingSockets_s.a"
  )

list(APPEND _cmake_import_check_targets GameNetworkingSockets::GameNetworkingSockets_s )
list(APPEND _cmake_import_check_files_for_GameNetworkingSockets::GameNetworkingSockets_s "${_IMPORT_PREFIX}/debug/lib/libGameNetworkingSockets_s.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
