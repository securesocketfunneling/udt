find_package(Catch2 REQUIRED)

add_executable(udt_tests)

target_sources(udt_tests
    PRIVATE
    udt_tests_main.cpp
    endpoint_tests.cpp
    protocol_tests.cpp
    stream_tests.cpp)

target_link_libraries(udt_tests AsioUDT::AsioUDT Catch2::Catch2)

include(ParseAndAddCatchTests)
ParseAndAddCatchTests(udt_tests)
