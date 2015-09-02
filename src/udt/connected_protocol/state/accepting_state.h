#ifndef UDT_CONNECTED_PROTOCOL_STATE_ACCEPTING_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_ACCEPTING_STATE_H_

#include <cstdint>

#include <memory>

#include <boost/chrono.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>

#include "udt/connected_protocol/state/base_state.h"
#include "udt/connected_protocol/state/connected_state.h"
#include "udt/connected_protocol/state/closed_state.h"

#include "udt/connected_protocol/state/policy/response_connection_policy.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class AcceptingState
    : public BaseState<Protocol>,
      public std::enable_shared_from_this<AcceptingState<Protocol>> {
 public:
  typedef std::shared_ptr<AcceptingState> Ptr;
  typedef typename Protocol::socket_session SocketSession;
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;
  typedef typename state::ClosedState<Protocol> ClosedState;

  typedef typename Protocol::clock Clock;
  typedef typename Protocol::timer Timer;
  typedef typename Protocol::time_point TimePoint;

  typedef typename state::ConnectedState<
      Protocol, typename policy::ResponseConnectionPolicy<Protocol>>
      ConnectedState;

 public:
  static Ptr Create(typename SocketSession::Ptr p_session) {
    return Ptr(new AcceptingState(std::move(p_session)));
  }

  virtual ~AcceptingState() {}

  virtual typename BaseState<Protocol>::type GetType() {
    return this->ACCEPTING;
  }

  virtual boost::asio::io_service& get_io_service() {
    return p_session_->get_io_service();
  }

  virtual void Init() { p_session_->start_timestamp = Clock::now(); }

  void Close() {
    auto self = this->shared_from_this();
    // state session to closed state
    p_session_->ChangeState(ClosedState::Create(p_session_->get_io_service()));

    // stop timers
    boost::system::error_code timer_ec;
    timeout_timer_.cancel(timer_ec);

    // Unbind session
    p_session_->Unbind();
  }

  virtual void OnConnectionDgr(ConnectionDatagramPtr p_connection_dgr) {
    auto& header = p_connection_dgr->header();
    auto& payload = p_connection_dgr->payload();
    uint32_t receive_cookie = payload.syn_cookie();
    uint32_t client_socket = payload.socket_id();

    auto self = this->shared_from_this();
    if (receive_cookie == p_session_->syn_cookie &&
        client_socket == p_session_->remote_socket_id) {
      // Very first handshake response
      {
        boost::recursive_mutex::scoped_lock lock(p_session_->mutex);
        if (p_session_->max_window_flow_size == 0) {
          p_session_->connection_info.set_packet_data_size(
              std::min(static_cast<uint32_t>(Protocol::MTU),
                       payload.maximum_packet_size() -
                           Protocol::PACKET_SIZE_CORRECTION));
          p_session_->max_window_flow_size = std::min(
              static_cast<uint32_t>(Protocol::MAXIMUM_WINDOW_FLOW_SIZE),
              payload.maximum_window_flow_size());
          p_session_->window_flow_size = p_session_->max_window_flow_size;
          p_session_->init_packet_seq_num =
              payload.initial_packet_sequence_number();
          p_session_->packet_seq_gen.set_current(
              p_session_->init_packet_seq_num);
          p_session_->ChangeState(ConnectedState::Create(p_session_));
          boost::system::error_code timer_ec;
          timeout_timer_.cancel(timer_ec);
        }
      }

      // Reply handshake response
      header.set_destination_socket(p_session_->remote_socket_id);
      payload.set_version(ConnectionDatagram::Payload::FORTH);
      payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
      payload.set_connection_type(ConnectionDatagram::Payload::FIRST_RESPONSE);
      payload.set_initial_packet_sequence_number(
          p_session_->packet_seq_gen.current());
      payload.set_syn_cookie(p_session_->syn_cookie);
      payload.set_maximum_packet_size(
          p_session_->connection_info.packet_data_size() +
          Protocol::PACKET_SIZE_CORRECTION);
      payload.set_maximum_window_flow_size(p_session_->max_window_flow_size);
      payload.set_socket_id(p_session_->socket_id);
    }

    p_session_->AsyncSendControlPacket(
        *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
        ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
        [self, p_connection_dgr](const boost::system::error_code&,
                                 std::size_t) {});
  }

 private:
  AcceptingState(typename SocketSession::Ptr p_session)
      : p_session_(std::move(p_session)),
        timeout_timer_(p_session_->get_io_service()) {}

  void StartTimeoutTimer() {
    timeout_timer_.expires_from_now(
        boost::chrono::seconds(p_session_->timeout_delay));

    timeout_timer_.async_wait(boost::bind(&AcceptingState::HandleTimeoutTimer,
                                          this->shared_from_this(), _1));
  }

  void HandleTimeoutTimer(const boost::system::error_code& ec) {
    if (!ec) {
      // Accepting timeout
      Close();
    }
  }

 private:
  typename SocketSession::Ptr p_session_;
  Timer timeout_timer_;
  TimePoint creation_time_point;
};

}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_ACCEPTING_STATE_H_
