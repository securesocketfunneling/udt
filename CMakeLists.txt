cmake_minimum_required(VERSION 3.8)

project(UDT VERSION 1.0.0 LANGUAGES CXX)

##
## OPTIONS
##
option(UDT_BUILD_APPS "Build the apps is enabled." ON)
option(UDT_BUILD_TESTS "Build the unit tests when BUILD_TESTING is enabled." ON)

##
## Main TARGET
## create target and add include path
##
find_package(Boost 1.67 REQUIRED)

add_library(udt INTERFACE)
add_library(SecureSocketFunneling::UDT ALIAS udt)
target_compile_features(udt INTERFACE cxx_std_11)
target_compile_definitions(udt INTERFACE BOOST_ASIO_ENABLE_OLD_SERVICES BOOST_ERROR_CODE_HEADER_ONLY)
target_link_libraries(udt INTERFACE Boost::boost)
target_include_directories(udt INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(udt SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
file(GLOB_RECURSE UDT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/udt/*.h")
target_sources(udt INTERFACE $<BUILD_INTERFACE:${UDT_HEADERS}>)

if (UDT_BUILD_APPS)
  add_subdirectory(apps)
endif ()

##
## TESTS
## create and configure the unit test target
##
include(CTest) #adds option BUILD_TESTING (default ON)

if (BUILD_TESTING AND UDT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif ()
