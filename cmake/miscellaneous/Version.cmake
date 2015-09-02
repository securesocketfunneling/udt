cmake_minimum_required(VERSION 2.8)

#
# This functions extract from git's tag version informations, they apply regex
# to 'git describe' result.
# Version informations will be update after any commit thanks to
# GetGitRevisionDescription() refresh trick.
#
#
# get_version(VERSION)
#   set version from git's tag in VERSION parent variable
#     1.0-42 => 1.0-42 (with 42 commits after tag)
#
# get_tag(TAG)
#   set tag from git's tag in TAG parent variable
#     1.0-42 => 1.0 (event 42 commits after)
#
# get_version_major(VERSION_MAJOR)
#   set major version number from git's tag
#     1.2.3 => 1
#
# get_version_minor(VERSION_MINOR)
#   set minor version number in VERSION_MINOR from git's tag
#     1.2.3 => 2.3
#
# get_commit(COMMIT)
#   set git's commit hash value in COMMIT
#
# get_version_debian(VERSION_DEBIAN)
#   set in VERSION_DEBIAN a packaging compatible version name
#     1.0rc3-42 => 1.0~rc3-42
#
# get_revision_info(TAG VERSION COMMIT VERSION_DEBIAN)
#   you can call directly this function which is wrapped in the previous ones
#   if you want to setup those value with one call
#

if(__version)
  return()
endif()
set(__version YES)

# Fork from GetGitRevisionDescription.cmake to enable git status from anywhere
function(get_git_head_revision_fork _directory _refspecvar _hashvar)
  set(GIT_PARENT_DIR "${_directory}")
  set(GIT_DIR "${GIT_PARENT_DIR}/.git")
  while(NOT EXISTS "${GIT_DIR}")  # .git dir not found, search parent directories
    set(GIT_PREVIOUS_PARENT "${GIT_PARENT_DIR}")
    get_filename_component(GIT_PARENT_DIR ${GIT_PARENT_DIR} PATH)
    if(GIT_PARENT_DIR STREQUAL GIT_PREVIOUS_PARENT)
      # We have reached the root directory, we are not in git
      set(${_refspecvar} "GITDIR-NOTFOUND" PARENT_SCOPE)
      set(${_hashvar} "GITDIR-NOTFOUND" PARENT_SCOPE)
      return()
    endif()
    set(GIT_DIR "${GIT_PARENT_DIR}/.git")
  endwhile()
  # check if this is a submodule
  if(NOT IS_DIRECTORY ${GIT_DIR})
    file(READ ${GIT_DIR} submodule)
    string(REGEX REPLACE "gitdir: (.*)\n$" "\\1" GIT_DIR_RELATIVE ${submodule})
    get_filename_component(SUBMODULE_DIR ${GIT_DIR} PATH)
    get_filename_component(GIT_DIR ${SUBMODULE_DIR}/${GIT_DIR_RELATIVE} ABSOLUTE)
  endif()
  set(GIT_DATA "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/git-data")
  if(NOT EXISTS "${GIT_DATA}")
    file(MAKE_DIRECTORY "${GIT_DATA}")
  endif()

  if(NOT EXISTS "${GIT_DIR}/HEAD")
    return()
  endif()
  set(HEAD_FILE "${GIT_DATA}/HEAD")
  configure_file("${GIT_DIR}/HEAD" "${HEAD_FILE}" COPYONLY)

  configure_file("${_gitdescmoddir}/GetGitRevisionDescription.cmake.in"
    "${GIT_DATA}/grabRef.cmake"
    @ONLY)
  include("${GIT_DATA}/grabRef.cmake")

  set(${_refspecvar} "${HEAD_REF}" PARENT_SCOPE)
  set(${_hashvar} "${HEAD_HASH}" PARENT_SCOPE)
endfunction()

# Fork from GetGitRevisionDescription.cmake to allow exec git from
# any _directory instead of "${CMAKE_SOURCE_DIR}"
function(git_describe_fork _directory _var)
  if(NOT GIT_FOUND)
    find_package(Git QUIET)
  endif()
  # call get_git_head_revision() to force cmake to refresh on every git commit
  # to update version
  include(GetGitRevisionDescription)
  get_git_head_revision_fork("${_directory}" refspec hash)
  if(NOT GIT_FOUND)
    set(${_var} "GIT-NOTFOUND" PARENT_SCOPE)
    return()
  endif()
  if(NOT hash)
    set(${_var} "HEAD-HASH-NOTFOUND" PARENT_SCOPE)
    return()
  endif()

  execute_process(COMMAND
  ${GIT_EXECUTABLE} describe ${ARGN}
    WORKING_DIRECTORY
    "${_directory}"
    RESULT_VARIABLE
    res
    OUTPUT_VARIABLE
    out
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT res EQUAL 0)
    set(out "${out}-${res}-NOTFOUND")
  endif()

  set(${_var} "${out}" PARENT_SCOPE)
endfunction()


function(git_get_exact_tag _var)
	git_describe(out --exact-match ${ARGN})
	set(${_var} "${out}" PARENT_SCOPE)
endfunction()

function(get_revision_info_debian GIT_DIR _TAG _VERSION _COMMIT _VERSION_DEBIAN)
  # set default value to be sure any inherited value will be overwrite
  set(VERSION "")
  set(TAG "")
  set(COMMIT "")
  set(VERSION_DEBIAN "")

  # get raw string description from git
  # ex: 1.0rc1-42-g13bce971f2f1e7cae101637a04977df0eb2aaeaa
  #   1.0rc1: the tag
  #   42: number of commit since the tag
  #   13bce971f2f1e7cae101637a04977df0eb2aaeaa: commit hash
  git_describe_fork(${GIT_DIR} VERSION_RAW --always --tags --abbrev=64 --long)

  # extract tag, distance from tag and commit hash
  string(REGEX MATCH "([0-9]+[.0-9]+.*)-(.*)-g(.*)" match ${VERSION_RAW})
  if (match)
    # extracted tag (ex: 1.0rc1-42)
    set(TAG ${CMAKE_MATCH_1})

    # number of commits since the tag (ex: 1.0rc1-42 -> 42)
    set(TAG_DISTANCE ${CMAKE_MATCH_2})

    # extracted commit
    # ex: 1.0rc1-42-g13bce971f2f1e7cae101637a04977df0eb2aaeaa
    #                               -> 13bce971f2f1e7cae101637a04977df0eb2aaeaa)
    set(COMMIT ${CMAKE_MATCH_3})

    # compute version (ex: 1.0rc1-42 or 1.0rc1)
    if (${TAG_DISTANCE} AND ${TAG_DISTANCE} GREATER "0")
      set(VERSION ${TAG}-${TAG_DISTANCE})
    else()
      set(VERSION ${TAG})
    endif()

    # version number (1.0rc1-42 -> 1.0)
    string(REGEX MATCH "([.0-9]+).*" _dummy ${TAG})
    set(VERSION_NUMBER ${CMAKE_MATCH_1})

    # version substring (1.0rc1-42 -> rc1-42)
    string(REGEX MATCH "[.0-9]+-?(.*)" _dummy ${TAG})
    if (${TAG_DISTANCE} GREATER "0")
      set(VERSION_SUBSTRING ${CMAKE_MATCH_1}-${TAG_DISTANCE})
    else()
      set(VERSION_SUBSTRING ${CMAKE_MATCH_1})
    endif()

    # debian compatible version naming
    # '1.1rc3' must be split as '1.1~rc3' to allow debian's version
    # sorting algorithm to work with alpha, beta, rc, release..
    if (NOT ${VERSION_SUBSTRING} EQUAL "")
      # replace forbidden characters
      string(REGEX REPLACE "/" "-" VERSION_SUBSTRING ${VERSION_SUBSTRING})
      string(REGEX REPLACE "_" "-" VERSION_SUBSTRING ${VERSION_SUBSTRING})
      # delete leading '-' to avoid a VERSION_DEBIAN like something~-substring
      string(REGEX REPLACE "^(-)" "" VERSION_SUBSTRING ${VERSION_SUBSTRING})

      set(VERSION_DEBIAN ${VERSION_NUMBER}~${VERSION_SUBSTRING})
    else()
      set(VERSION_DEBIAN ${VERSION})
    endif()
  else()
    # if regex doesn't work it's because there's no tag, only commit hash
    set(COMMIT ${VERSION_RAW})
    message(WARNING "VERSION CANNOT BE EXTRACTED FROM TAG !")
  endif()

  # export variables
  set(${_VERSION} "${VERSION}" PARENT_SCOPE)
  set(${_TAG} "${TAG}" PARENT_SCOPE)
  set(${_COMMIT} "${COMMIT}" PARENT_SCOPE)
  set(${_VERSION_DEBIAN} "${VERSION_DEBIAN}" PARENT_SCOPE)
endfunction()

function(get_revision_info _GIT_DIR _TAG _VERSION _COMMIT)
  get_revision_info_debian(${_GIT_DIR} TAG VERSION COMMIT DEBIAN_VERSION)

  # export variables
  set(${_VERSION} "${VERSION}" PARENT_SCOPE)
  set(${_TAG} "${TAG}" PARENT_SCOPE)
  set(${_COMMIT} "${COMMIT}" PARENT_SCOPE)
endfunction()

# extract version from git
function(get_version _GIT_DIR _VERSION)
  get_revision_info(${_GIT_DIR} TAG VERSION COMMIT)
  set(${_VERSION} "${VERSION}" PARENT_SCOPE)
endfunction()

# extract tag from git
function(get_tag _GIT_DIR _TAG)
  get_revision_info(${_GIT_DIR} TAG VERSION COMMIT)
  set(${_TAG} "${TAG}" PARENT_SCOPE)
endfunction()

# extract commit from git
function(get_commit _GIT_DIR _COMMIT)
  get_revision_info(${_GIT_DIR} TAG VERSION COMMIT)
  set(${_COMMIT} "${COMMIT}" PARENT_SCOPE)
endfunction()

# extract version with debian package compatible format 1.1rc3 => 1.1~rc3
function(get_version_debian _GIT_DIR _VERSION_DEBIAN)
  get_revision_info_debian(${_GIT_DIR} TAG VERSION COMMIT VERSION_DEBIAN)
  set(${_VERSION_DEBIAN} "${VERSION_DEBIAN}" PARENT_SCOPE)
endfunction()

# extract major version number 1.2.3 => 1
function(get_version_major _VERSION_MAJOR)
  get_revision_info(TAG VERSION COMMIT)

  string(REGEX MATCH "([0-9]+)\\..*" match ${VERSION})
  if (match)
    set(${_VERSION_MAJOR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    return()
  endif()
  message(WARNING "Cannot extract major version from '${VERSION}'")
endfunction()

# extract minor version number 1.2.3 => 2.3
function(get_version_minor _VERSION_MINOR)
  get_revision_info(TAG VERSION COMMIT)

  string(REGEX MATCH "[0-9]+\\.([.0-9]+).*" match ${VERSION})
  if (match)
    set(${_VERSION_MINOR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    return()
  endif()
  message(WARNING "Cannot extract minor version from '${VERSION}'")
endfunction()
