
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was GameNetworkingSocketsConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

find_dependency(Threads)
find_dependency(Protobuf)

if(OpenSSL STREQUAL "OpenSSL")
    find_dependency(OpenSSL)
endif()

if(OpenSSL STREQUAL "libsodium")
    find_dependency(sodium)
endif()

if(STEAMWEBRTC_ABSL_SOURCE-NOTFOUND STREQUAL "package")
    find_dependency(absl REQUIRED)
endif()

if(OFF)
    include(${CMAKE_CURRENT_LIST_DIR}/steamwebrtc.cmake)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/GameNetworkingSockets.cmake)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.11.0")
    # Need to set IMPORTED_GLOBAL in order to create the aliases
    # However, IMPORTED_GLOBAL only exists in 3.11 and up
    # Also, setting IMPORTED_GLOBAL from another directory from where it was first set blows up
    # so we can only set it once.

    if(ON)
        get_target_property(GNS_IMPORTED_GLOBAL_SET GameNetworkingSockets::GameNetworkingSockets IMPORTED_GLOBAL)

        if(NOT ${GNS_IMPORTED_GLOBAL_SET})
            set_target_properties(
                GameNetworkingSockets::GameNetworkingSockets
                PROPERTIES IMPORTED_GLOBAL True
            )
        endif()

        unset(GNS_IMPORTED_GLOBAL_SET)

        add_library(GameNetworkingSockets::shared ALIAS GameNetworkingSockets::GameNetworkingSockets)
    endif()

    if(ON)
        get_target_property(GNS_IMPORTED_GLOBAL_SET GameNetworkingSockets::GameNetworkingSockets_s IMPORTED_GLOBAL)

        if(NOT ${GNS_IMPORTED_GLOBAL_SET})
            set_target_properties(
                GameNetworkingSockets::GameNetworkingSockets_s
                PROPERTIES IMPORTED_GLOBAL True
            )
        endif()

        unset(GNS_IMPORTED_GLOBAL_SET)

        add_library(GameNetworkingSockets::static ALIAS GameNetworkingSockets::GameNetworkingSockets_s)
    endif()
endif()

check_required_components(GameNetworkingSockets)
set(GameNetworkingSockets_FOUND 1)
