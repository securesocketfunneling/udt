cmake_minimum_required(VERSION 2.8)
set(project_name "UDT")
project(${project_name})

# --- Include CMake build components and CMake common files
add_subdirectory(./cmake)
include(HelpersIdeTarget)

# --- Project variables
set(project_BINARY_DIR "${${project_name}_BINARY_DIR}")
set(project_ROOT_DIR "${${project_name}_SOURCE_DIR}")
set(project_SRC_DIR "${project_ROOT_DIR}/src")
set(project_THIRDPARTY_DIR "${project_ROOT_DIR}/third_party")

# --- Project files declaration

file(GLOB_RECURSE COMMON_HEADER_FILES
      "${project_SRC_DIR}/udt/common/*.h")
    
file(GLOB_RECURSE COMMON_SOURCE_FILES
      "${project_SRC_DIR}/udt/common/*.cpp")

file(GLOB_RECURSE QUEUE_HEADER_FILES
      "${project_SRC_DIR}/udt/queue/*.h")
    
file(GLOB_RECURSE QUEUE_SOURCE_FILES
      "${project_SRC_DIR}/udt/queue/*.cpp")

file(GLOB_RECURSE TIMER_HEADER_FILES
      "${project_SRC_DIR}/udt/timer/*.hpp")
    
file(GLOB_RECURSE TIMER_SOURCE_FILES
      "${project_SRC_DIR}/udt/timer/*.cpp")
      
file(GLOB_RECURSE CONNECTED_PROTOCOL_HEADER_FILES
      "${project_SRC_DIR}/udt/connected_protocol/*.h")

file(GLOB_RECURSE CONNECTED_PROTOCOL_SOURCE_FILES
      "${project_SRC_DIR}/udt/connected_protocol/*.cpp")
      
file(GLOB_RECURSE UDT_HEADER_FILES
      "${project_SRC_DIR}/udt/ip/*.h")
      
file(GLOB_RECURSE UDT_SOURCE_FILES
      "${project_SRC_DIR}/udt/ip/*.cpp")

set(BOOST_ROOT
      "${project_THIRDPARTY_DIR}/boost" CACHE PATH "Path of boost library")
message("** BOOST_ROOT: ${BOOST_ROOT}")

# --- Include platform specific CMakeList

# ---- Third party parameters
set(BOOST_PLATFORM_FLAGS "")
set(BOOST_PLATFORM_COMPONENTS "")
set(OPENSSL_PLATFORM_COMPONENTS "")

if (WIN32)
  include(./cmake-ms/CMakeLists.txt)
elseif (UNIX)
  include(./cmake-unix/CMakeLists.txt)
endif ()

# --- Define boost version if not set
if(NOT DEP_BOOST_VERSION)
  set(DEP_BOOST_VERSION 1.58.0)
endif()

# --- Setup Boost dependencies
set(Boost_USE_STATIC_LIBS       ON)
set(Boost_USE_MULTITHREADED     ON)
set(Boost_USE_STATIC_RUNTIME    ON)

# --- Boost components
find_package(
  Boost REQUIRED 
  FLAGS 
    STATIC
    ${BOOST_PLATFORM_FLAGS}
  WITH_COMPONENTS
    system
    serialization
    program_options
    date_time
    filesystem
    coroutine
    context
    log
    regex
    thread
    chrono
    ${BOOST_PLATFORM_COMPONENTS}
)

# --- Include bin directory
include_directories(${project_BINARY_DIR})
  
# --- Include source directory
include_directories(${project_SRC_DIR})

set (BUILD_UNIT_TESTS ON)
if (BUILD_UNIT_TESTS)
  include(GTest)
  
  set(GTEST_ROOT_DIR 
        "${project_THIRDPARTY_DIR}/gtest" CACHE PATH "Path of gtest library")

  # --- Extract GTest archive in build directory
  gtest_unpack_archive()

  enable_testing()

  # --- Add GTest project 
  add_subdirectory(${GTEST_ROOT_DIR})
  
  # --- Add src test 
  add_subdirectory("${project_SRC_DIR}/tests")
endif (BUILD_UNIT_TESTS)

add_subdirectory("${project_SRC_DIR}/udt_client")
add_subdirectory("${project_SRC_DIR}/udt_server")
add_subdirectory("${project_SRC_DIR}/timer_benchmark")
