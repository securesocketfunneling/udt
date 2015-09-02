#ifndef UDT_TESTS_PROTOCOL_HELPERS_H_
#define UDT_TESTS_PROTOCOL_HELPERS_H_

/// Test ASIO protocol requirements
template <class AsioProtocol>
void TestAsioProtocol() {
  AsioProtocol protocol1;
  AsioProtocol protocol2(protocol1);
  AsioProtocol protocol3;

  protocol3 = protocol2;

  typename AsioProtocol::endpoint ep;

  int family = protocol1.family();
  (void) family;
  int type = protocol1.type();
  (void) type;
  int protocol = protocol1.protocol();
  (void) protocol;
}

#endif  // UDT_TESTS_PROTOCOL_HELPERS_H_
