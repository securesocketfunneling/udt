cmake_minimum_required(VERSION 3.8)

project(AsioUDT VERSION 1.0.0 LANGUAGES CXX)

##
## OPTIONS
##
option(ASIO_UDT_BUILD_APPS "Build the apps is enabled." ON)
option(ASIO_UDT_BUILD_TESTS "Build the unit tests when BUILD_TESTING is enabled." ON)

##
## Main TARGET
## create target and add include path
##
find_package(Boost 1.67 REQUIRED)
find_package(Threads REQUIRED)

add_library(asio_udt INTERFACE)
add_library(AsioUDT::AsioUDT ALIAS asio_udt)
target_compile_features(asio_udt INTERFACE cxx_std_11)
target_compile_definitions(asio_udt INTERFACE BOOST_ASIO_ENABLE_OLD_SERVICES BOOST_ERROR_CODE_HEADER_ONLY)
target_link_libraries(asio_udt INTERFACE Boost::boost Threads::Threads)
target_include_directories(asio_udt INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(asio_udt SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
file(GLOB_RECURSE ASIO_UDT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/udt/*.h")
target_sources(asio_udt INTERFACE $<BUILD_INTERFACE:${ASIO_UDT_HEADERS}>)

if (ASIO_UDT_BUILD_APPS)
  add_subdirectory(apps)
endif ()

##
## TESTS
## create and configure the unit test target
##
include(CTest) #adds option BUILD_TESTING (default ON)

if (BUILD_TESTING AND ASIO_UDT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif ()
