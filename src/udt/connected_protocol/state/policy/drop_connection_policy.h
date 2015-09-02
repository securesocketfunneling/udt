#ifndef UDT_CONNECTED_PROTOCOL_STATE_POLICY_DROP_CONNECTION_POLICY_H_
#define UDT_CONNECTED_PROTOCOL_STATE_POLICY_DROP_CONNECTION_POLICY_H_

#include <memory>

namespace connected_protocol {
namespace state {
namespace policy {

template <class Protocol>
class DropConnectionPolicy {
 private:
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;
  typedef typename Protocol::socket_session SocketSession;

 protected:
  void ProcessConnectionDgr(typename SocketSession::Ptr p_session,
                            ConnectionDatagramPtr p_connection_dgr) {
    // Drop datagram
  }
};

}  // policy
}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_POLICY_DROP_CONNECTION_POLICY_H_
