#
# Define some helpers for Target management
#
if(__H_HELPERS_TARGET_INCLUDED)
  return()
endif()
set(__H_HELPERS_TARGET_INCLUDED TRUE)

# ===
# === Include some external files
# ===
include(CMakeParseArguments)

# ===
# === Define helper functions
# ===

# --- Add new definitions to specified target
# ---
# --- Syntax : add_target_definitions(TARGET <targets_name> DEFINITIONS <new_definitions_to_add>)
# ---
# --- Example:
# ---  add_target_definitions(
# ---      TARGET
# ----         myTarget1
# ----         myTarget2
# ----
# ----     DEFINITIONS
# ----         MY_DEF1=VAL1
# ----         MY_DEF2=VAL2
# ----         MY_DEF
# ---- )
function(add_target_definitions)
    CMake_Parse_Arguments(
        # The prefix for generated variables
        _ATD

        # The names of optional flag arguments
        ""

        # The names of OneValue arguments
        ""

        # The names of MultiValues arguments
        "TARGET;DEFINITIONS"

        # The list of arguments to parse
        ${ARGN}
    )

    foreach(_tgt ${_ATD_TARGET})
        get_property(_save_cfg TARGET ${_tgt} PROPERTY COMPILE_DEFINITIONS)
        set_property(TARGET ${_tgt} PROPERTY COMPILE_DEFINITIONS ${_save_cfg} ${_ATD_DEFINITIONS})
    endforeach()
endfunction(add_target_definitions)

# --- Add new linker's flags to specified target
# ---
# --- Syntax : add_target_link_flags(TARGET <targets_name> FLAGS <new_definitions_to_add>)
# ---
# --- Example:
# ---  add_target_definitions(
# ---      TARGET
# ----         myTarget1
# ----         myTarget2
# ----
# ----     FLAGS
# ----         FLAG1
# ----         FLAG2
# ---- )
function(add_target_link_flags)
    # --- Parse arguments
    set(_cfg_list "MINSIZEREL;RELEASE;RELWITHDEBINFO;DEBUG")
    CMake_Parse_Arguments(
        # The prefix for generated variables
        _ATLF

        # The names of optional flag arguments
        ""

        # The names of OneValue arguments
        ""

        # The names of MultiValues arguments
        "TARGET;${_cfg_list}"

        # The list of arguments to parse
        ${ARGN}
    )

    # --- Adjust some arguments with a default value
    if(NOT _ATLF_MINSIZEREL OR NOT _ATLF_RELEASE)
        if(NOT _ATLF_MINSIZEREL)
            set(_ATLF_MINSIZEREL ${_ATLF_RELEASE})
        else()
            set(_ATLF_RELEASE ${_ATLF_MINSIZEREL})
        endif()
    endif()

    if(NOT _ATLF_RELWITHDEBINFO OR NOT _ATLF_DEBUG)
        if(NOT _ATLF_RELWITHDEBINFO)
            set(_ATLF_RELWITHDEBINFO ${_ATLF_DEBUG})
        else()
            set(_ATLF_DEBUG ${_ATLF_RELWITHDEBINFO})
        endif()
    endif()

    # --- Iterate on all specified targets
    foreach(_tgt ${_ATLF_TARGET})
        # --- Check target's type to use valid property name for linker flags
        get_property(_target_type TARGET "${_tgt}" PROPERTY TYPE)
        if(_target_type STREQUAL "STATIC_LIBRARY")
            set(_propname_base STATIC_LIBRARY_FLAGS)
        else()
            set(_propname_base LINK_FLAGS)
        endif()

        # --- Iterate on all given flags' configuration
        foreach(_var ${_cfg_list})
            set(_propname "${_propname_base}_${_var}")
            set(_propname_value "_ATLF_${_var}")
            if (${_propname_value})
                get_property(_link_flags TARGET "${_tgt}" PROPERTY "${_propname}")
                string(STRIP "${_link_flags} ${${_propname_value}}" _link_flags)
                set_property(TARGET "${_tgt}" PROPERTY "${_propname}" "${_link_flags}")
            endif()
        endforeach()
    endforeach()
endfunction(add_target_link_flags)