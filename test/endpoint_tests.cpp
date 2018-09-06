#include <cstdint>

#include <catch2/catch.hpp>

#include "udt/ip/udt.h"

using udt_protocol = ip::udt<>;

/// Test ASIO protocol requirements
TEST_CASE("UDTEnpoint") {
  using endpoint_type = typename udt_protocol::endpoint;

  endpoint_type ep1;
  endpoint_type ep2(ep1);
  endpoint_type ep3 = ep1;

  typename endpoint_type::protocol_type protocol1;
  (void)protocol1;

  endpoint_type();

  typename endpoint_type::protocol_type protocol2 = ep1.protocol();

  ep1.data();

  std::size_t size = ep1.size();
  (void)size;
  std::size_t capacity = ep1.capacity();
  (void)capacity;
}
