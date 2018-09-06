#include <catch2/catch.hpp>

#include "udt/ip/udt.h"

using udt_protocol = ip::udt<>;

/// Test ASIO protocol requirements
TEST_CASE("UDTProtocol") {
  udt_protocol protocol1 = udt_protocol::v4();
  udt_protocol protocol2(protocol1);
  udt_protocol protocol3 = udt_protocol::v4();

  protocol3 = protocol2;

  typename udt_protocol::endpoint ep;

  int family = protocol1.family();
  (void) family;
  int type = protocol1.type();
  (void) type;
  int protocol = protocol1.protocol();
  (void) protocol;
}
