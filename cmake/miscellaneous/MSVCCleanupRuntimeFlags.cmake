#
# Helper to remove ALL default MSVC's flags linked to Runtime link
#
if(__H_MSVC_CLEANUP_RUNTIME_FLAGS)
  return()
endif()
set(__H_MSVC_CLEANUP_RUNTIME_FLAGS TRUE)

if(MSVC)
function(msvc_cleanup_runtime)
  foreach(_cfg "" _DEBUG _RELEASE _MINSIZEREL _RELWITHDEBINFO)
    foreach(_lang C CXX)
      set(_var "CMAKE_${_lang}_FLAGS${_cfg}")
      string(REGEX REPLACE "([\t ]*)/M[TD]d?[\t ]*" "\\1" ${_var} "${${_var}}")
      set(${_var} "${${_var}}" PARENT_SCOPE)
    endforeach()
  endforeach()
endfunction()

msvc_cleanup_runtime()
endif()
