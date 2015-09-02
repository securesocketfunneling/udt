project(RUN_TESTS CXX)

enable_testing()
  
include_directories(
  ${Boost_INCLUDE_DIRS})

include_directories(
  ${GTEST_ROOT_DIR}/include
)
      
file(GLOB_RECURSE LAYER_TEST_FIXTURES_FILES
      "${project_SRC_DIR}/tests/*.h")

# --- UDT tests
add_target("udt_tests"
  TYPE
    executable ${EXEC_FLAG} TEST
  LINKS 
    ${Boost_LIBRARIES}
    ${PLATFORM_SPECIFIC_LIB_DEP}
    PREFIX_SKIP     .*/src
    HEADER_FILTER   "\\.h(h|m|pp|xx|\\+\\+)?" 
  FILES
    "udt_tests.cpp"
    ${COMMON_HEADER_FILES}
    ${COMMON_SOURCE_FILES}
    ${QUEUE_HEADER_FILES}
    ${QUEUE_SOURCE_FILES}
    ${CONNECTED_PROTOCOL_HEADER_FILES}
    ${CONNECTED_PROTOCOL_SOURCE_FILES}
    ${UDT_HEADER_FILES}
    ${UDT_SOURCE_FILES}
    ${LAYER_TEST_FIXTURES_FILES}
)
