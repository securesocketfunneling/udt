#ifndef UDT_CONNECTED_PROTOCOL_STATE_POLICY_RESPONSE_CONNECTION_POLICY_H_
#define UDT_CONNECTED_PROTOCOL_STATE_POLICY_RESPONSE_CONNECTION_POLICY_H_

#include <memory>

namespace connected_protocol {
namespace state {
namespace policy {

template <class Protocol>
class ResponseConnectionPolicy {
 private:
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;
  typedef typename Protocol::socket_session SocketSession;

 protected:
  void ProcessConnectionDgr(typename SocketSession::Ptr p_session,
                            ConnectionDatagramPtr p_connection_dgr) {
    // Reply after each connection dgr
    auto& header = p_connection_dgr->header();
    auto& payload = p_connection_dgr->payload();
    header.set_destination_socket(p_session->remote_socket_id);
    payload.set_version(ConnectionDatagram::Payload::FORTH);
    payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
    payload.set_connection_type(ConnectionDatagram::Payload::SECOND_RESPONSE);
    payload.set_initial_packet_sequence_number(p_session->init_packet_seq_num);
    payload.set_syn_cookie(p_session->syn_cookie);
    payload.set_maximum_packet_size(
        p_session->connection_info.packet_data_size() +
        Protocol::PACKET_SIZE_CORRECTION);
    payload.set_maximum_window_flow_size(p_session->max_window_flow_size);
    payload.set_socket_id(p_session->socket_id);

    p_session->AsyncSendControlPacket(
        *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
        ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
        [p_connection_dgr, p_session](const boost::system::error_code&,
                                      std::size_t) {});
  }
};

}  // policy
}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_POLICY_RESPONSE_CONNECTION_POLICY_H_
