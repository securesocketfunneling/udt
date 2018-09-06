#ifndef UDT_CONNECTED_PROTOCOL_STATE_POLICY_DROP_CONNECTION_POLICY_H_
#define UDT_CONNECTED_PROTOCOL_STATE_POLICY_DROP_CONNECTION_POLICY_H_

#include <memory>

namespace connected_protocol {
namespace state {
namespace policy {

template <class Protocol>
class DropConnectionPolicy {
 private:
  using ConnectionDatagram = typename Protocol::ConnectionDatagram;
  using ConnectionDatagramPtr = std::shared_ptr<ConnectionDatagram>;
  using SocketSession = typename Protocol::socket_session;

 protected:
  void ProcessConnectionDgr(SocketSession* p_session,
                            ConnectionDatagramPtr p_connection_dgr) {
    // Drop datagram
  }
};

}  // namespace policy
}  // namespace state
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_POLICY_DROP_CONNECTION_POLICY_H_
