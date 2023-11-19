#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "GameNetworkingSockets::GameNetworkingSockets" for configuration "Release"
set_property(TARGET GameNetworkingSockets::GameNetworkingSockets APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GameNetworkingSockets::GameNetworkingSockets PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libGameNetworkingSockets.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/libGameNetworkingSockets.dylib"
  )

list(APPEND _cmake_import_check_targets GameNetworkingSockets::GameNetworkingSockets )
list(APPEND _cmake_import_check_files_for_GameNetworkingSockets::GameNetworkingSockets "${_IMPORT_PREFIX}/lib/libGameNetworkingSockets.dylib" )

# Import target "GameNetworkingSockets::GameNetworkingSockets_s" for configuration "Release"
set_property(TARGET GameNetworkingSockets::GameNetworkingSockets_s APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GameNetworkingSockets::GameNetworkingSockets_s PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libGameNetworkingSockets_s.a"
  )

list(APPEND _cmake_import_check_targets GameNetworkingSockets::GameNetworkingSockets_s )
list(APPEND _cmake_import_check_files_for_GameNetworkingSockets::GameNetworkingSockets_s "${_IMPORT_PREFIX}/lib/libGameNetworkingSockets_s.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
