#ifndef UDT_TESTS_ENDPOINT_HELPERS_H_
#define UDT_TESTS_ENDPOINT_HELPERS_H_

#include <cstdint>

/// Test ASIO protocol requirements
template <class AsioProtocol>
void TestAsioEndpoint() {
  using endpoint_type = typename AsioProtocol::endpoint;

  endpoint_type ep1;
  endpoint_type ep2(ep1);
  endpoint_type ep3 = ep1;

  typename endpoint_type::protocol_type protocol1;
  (void)protocol1;

  endpoint_type();

  typename endpoint_type::protocol_type protocol2 = ep1.protocol();

  ep1.data();

  std::size_t size = ep1.size();
  (void) size;
  std::size_t capacity = ep1.capacity();
  (void) capacity;
}

#endif  // UDT_TESTS_ENDPOINT_HELPERS_H_
