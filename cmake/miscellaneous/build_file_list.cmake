if(__H_BULD_FILE_LIST)
  return()
endif()
set(__H_BULD_FILE_LIST TRUE)

# ===
# === Define helper functions
# ===

# --- Build a list of pattern relative to path
# ---
# --- Syntax : build_glob_pattern(<relative_path> <pattern_list> <result_var>)
# ---
# --- Example:
# --- build_glob_pattern(
# ---     "../src/"
# ---     
# ---     "*.cpp;*.c"
# ---     
# ---     glob_pattern
# --- )
# --- 
# --- will create the list :
# --- set(glob_pattern "../src/*.cpp;../src/*.c")
function (build_glob_pattern relative_path pattern_list pattern_glob_list)
    set (glob_list "")
    foreach(pattern IN LISTS pattern_list)
        list( APPEND glob_list "${relative_path}${pattern}")
    endforeach()
    set (${pattern_glob_list} ${glob_list} PARENT_SCOPE)
endfunction()

# --- Build a list of all source files (with pattern) contained in the relative drectory.
# --- Only the source files written for the target platform are included :
# --- ${relative_path}/* and 
# --- ${relative_path}/os_specific/${CMAKE_SYSTEM_NAME}/* and 
# ---
# --- Syntax : get_matching_files(<relative_path> <pattern_list> <result>)
# ---
# --- Example :
# --- with the following tree:
# ---     src
# ---     |-  gui
# ---     |   |-  main.c
# ---     |   \-  dialog.cpp
# ---     |
# ---     |-  tools
# ---     |    \-  convert.cpp
# ---     |
# ---     |-  os_specific
# ---     |   |-  Windows
# ---     |   |    \- windows_src.cpp
# ---     |   |-  Darwin
# ---     |       \- darwin_src.cpp
# ---

# --- call get_matching_files("src/" "*.cpp;*.c" result) on OsX
# --- will produce :
# --- result : "src/gui/main.c;src/gui/dialog.cpp;src/tools/convert.cpp;src/os_specific/Darwin/darwin_src.cpp"
# ---
function (get_matching_files relative_path pattern_list result)

    # --- Search for all the source file to compile
    # --- note : since it is not possible to make a 'globbing' pattern
    # ---        which can find file (*.h, ...) which do NOT contain a 
    # ---        specific pattern, we build first a list, then remove
    # ---        the elements which match the specific pattern
    if(relative_path)
      set(relative_path "${relative_path}/")
    endif()
    build_glob_pattern("${relative_path}" "${pattern_list}" glob_pattern)

    file( GLOB_RECURSE all_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        ${glob_pattern}
    )

    # find all specific files which are not dependent of ${CMAKE_SYSTEM_NAME}
    set (wrong_specific_files "")
    foreach (found_file IN LISTS all_files)
        if (
                (${found_file} MATCHES "${relative_path}os_specific/") AND
            NOT (${found_file} MATCHES "${relative_path}os_specific/${CMAKE_SYSTEM_NAME}"))
            list (APPEND wrong_specific_files ${found_file})
        endif()
    endforeach()

    list( LENGTH wrong_specific_files num_specific_files)
    if (num_specific_files GREATER 0)
        list ( REMOVE_ITEM all_files ${wrong_specific_files})
    endif()


    set (${result} ${all_files} PARENT_SCOPE)
endfunction()
