cmake_minimum_required(VERSION 2.8)

set(project_NAME "udt_client")
project(${project_NAME})

set(CLIENT_FILES
      main.cpp)

include_directories(
  ${Boost_INCLUDE_DIRS})

add_target(${project_NAME}
  TYPE
    executable ${EXEC_FLAG} INSTALL
  FILES
    ${CLIENT_FILES}
    ${COMMON_HEADER_FILES}
    ${COMMON_SOURCE_FILES}
    ${QUEUE_HEADER_FILES}
    ${QUEUE_SOURCE_FILES}
    ${CONNECTED_PROTOCOL_HEADER_FILES}
    ${CONNECTED_PROTOCOL_SOURCE_FILES}
    ${UDT_HEADER_FILES}
    ${UDT_SOURCE_FILES}
  PREFIX_SKIP
    ${project_SRC_DIR})

target_link_libraries(${project_NAME} ${Boost_LIBRARIES} ${PLATFORM_SPECIFIC_LIB_DEP})
