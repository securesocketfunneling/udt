# FindDrExplain.cmake
#
# Helpers to find DrExplain package from the system
#

if(DREXPLAIN_FOUND)
  return()
endif()

cmake_minimum_required(VERSION 2.8.11 FATAL_ERROR)

include(FindPackageHandleStandardArgs)

# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
function(search_for_drexplain_location)
  # --- Try first to find DrExplain tool from some registry keys
  foreach(_root HKEY_CURRENT_USER HKEY_LOCAL_MACHINE)
    get_filename_component(_command
      "[${_root}\\Software\\Indigo Byte Systems\\Dr.Explain\\App;path]"
      ABSOLUTE
    )

    if(_command AND NOT ("${_command}" STREQUAL "/registry"))
      break()
    endif()
    unset(_command)
  endforeach()

  if(NOT _command)
    # --- Registry keys did not work. So, try from program files...
    unset(_search_path)
    foreach(_name "PROGRAMFILES" "PROGRAMFILES(X86)" "PROGRAMW6432")
      foreach(_variant DrExplain Dr.Explain)
        get_filename_component(_name "$ENV{${_name}}/${_variant}" ABSOLUTE)
        list(APPEND _search_path "${_name}")
      endforeach()
    endforeach()

    find_program(_command
      NAMES DrExplain
      PATHS ${_search_path}
      NO_CMAKE_ENVIRONMENT_PATH
      NO_CMAKE_PATH
      NO_CMAKE_SYSTEM_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
  endif()

  if(_command)
    get_filename_component(DREXPLAIN_INSTALL_PATH "${_command}" DIRECTORY)
    set(DREXPLAIN_INSTALL_PATH "${DREXPLAIN_INSTALL_PATH}" PARENT_SCOPE)
  endif()

  unset(_command CACHE)
endfunction()

# ------------------------------------------------------------------------------
function(find_drexplain_info)
  # -- Define keys to retrieve
  set(_value_names
    "DREXPLAIN_INSTALL_PATH:InstallLocation"
    "DREXPLAIN_NAME=DisplayName"
    "DREXPLAIN_VERSION=DisplayVersion"
  )

  # -- Lookup registry
  set(_base "HKEY_LOCAL_MACHINE\\SOFTWARE")
  foreach(_root "${_base}" "${_base}\\Wow6432Node")
    set(_root "${_root}\\Microsoft\\Windows\\CurrentVersion\\Uninstall")
    set(_found ON)
    foreach(_value_name IN LISTS _value_names)
      if(_value_name MATCHES "^(.+)=(.+)$")
        set(_mode NAME)
      elseif(_value_name MATCHES "^(.+):(.+)$")
        set(_mode ABSOLUTE)
      endif()
      set(_var_name ${CMAKE_MATCH_1})
      set(_value_name ${CMAKE_MATCH_2})
      get_filename_component(${_var_name}
        "[${_root}\\Dr.Explain_is1;${_value_name}]"
        ${_mode}
      )
      if(NOT ${_var_name} OR (${_var_name} MATCHES "^/?registry$"))
        unset(_found)
        break()
      endif()
    endforeach()
    if(_found)
      break()
    endif()
  endforeach()

  if(_found)
    foreach(_value_name IN LISTS _value_names)
      if(_value_name MATCHES "^(.+)[=:](.+)$")
        set(${CMAKE_MATCH_1} "${${CMAKE_MATCH_1}}" PARENT_SCOPE)
      endif()
    endforeach()

    if(DREXPLAIN_VERSION MATCHES "^([0-9]+)\\.(.+)$")
      set(DREXPLAIN_VERSION_MAJOR "${CMAKE_MATCH_1}" PARENT_SCOPE)
      set(DREXPLAIN_VERSION_MINOR "${CMAKE_MATCH_2}" PARENT_SCOPE)
    elseif(DREXPLAIN_VERSION MATCHES "^([0-9]+).*$")
      set(DREXPLAIN_VERSION_MAJOR "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

# ------------------------------------------------------------------------------
function(find_drexplain_tools)
  # Search for DrExplain main command
  find_program(DREXPLAIN_COMMAND
    NAMES DrExplain
    HINTS ${DREXPLAIN_INSTALL_PATH}
    NO_CMAKE_ENVIRONMENT_PATH
    NO_CMAKE_PATH
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
  )

  # Search for DrExplain export tool
  find_program(DREXPLAIN_TOOL
    NAMES         deexport
    HINTS         ${DREXPLAIN_INSTALL_PATH}
    PATH_SUFFIXES tools
    DOC           "Path to Dr.Explain export tool"
    NO_CMAKE_ENVIRONMENT_PATH
    NO_CMAKE_PATH
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
  )
endfunction()

# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------

find_drexplain_info()
if(NOT DREXPLAIN_INSTALL_PATH)
 search_for_drexplain_location()
 set(DREXPLAIN_VERSION "unknown")
endif()

if(DREXPLAIN_INSTALL_PATH AND EXISTS "${DREXPLAIN_INSTALL_PATH}")
  find_drexplain_tools()
endif()

find_package_handle_standard_args(DrExplain
  REQUIRED_VARS
    DREXPLAIN_VERSION
    DREXPLAIN_INSTALL_PATH
    DREXPLAIN_COMMAND
    DREXPLAIN_TOOL
  VERSION_VAR
    DREXPLAIN_VERSION
)

if(DREXPLAIN_FOUND)
  foreach(_var  DREXPLAIN_FOUND
                DREXPLAIN_VERSION
                DREXPLAIN_INSTALL_PATH
                DREXPLAIN_COMMAND
                DREXPLAIN_TOOL
                DREXPLAIN_NAME
                DREXPLAIN_VERSION_MAJOR
                DREXPLAIN_VERSION_MINOR)
    if(${_var})
      set(${_var} "${${_var}}" CACHE INTERNAL "")
      mark_as_advanced(${_var})
    endif()
  endforeach()
endif()
