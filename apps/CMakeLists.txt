add_executable(udt_client)
target_sources(udt_client PRIVATE udt_client.cpp)
target_link_libraries(udt_client PRIVATE AsioUDT::AsioUDT Boost::boost)

add_executable(udt_server)
target_sources(udt_server PRIVATE udt_server.cpp)
target_link_libraries(udt_server PRIVATE AsioUDT::AsioUDT Boost::boost)
