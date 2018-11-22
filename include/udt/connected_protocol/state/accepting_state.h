#ifndef UDT_CONNECTED_PROTOCOL_STATE_ACCEPTING_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_ACCEPTING_STATE_H_

#include <cstdint>

#include <memory>

#include <chrono>

#include <boost/system/error_code.hpp>

#include "base_state.h"
#include "closed_state.h"
#include "connected_state.h"

#include "policy/response_connection_policy.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class AcceptingState
    : public BaseState<Protocol>,
      public std::enable_shared_from_this<AcceptingState<Protocol>> {
 public:
  using Ptr = std::shared_ptr<AcceptingState>;
  using SocketSession = typename Protocol::socket_session;
  using ConnectionDatagram = typename Protocol::ConnectionDatagram;
  using ConnectionDatagramPtr = std::shared_ptr<ConnectionDatagram>;
  using ClosedState = typename state::ClosedState<Protocol>;

  using Clock = typename Protocol::clock;
  using Timer = typename Protocol::timer;
  using TimePoint = typename Protocol::time_point;

  using ConnectedState = typename state::ConnectedState<
      Protocol, typename policy::ResponseConnectionPolicy<Protocol>>;

 public:
  static Ptr Create(typename SocketSession::Ptr p_session) {
    return Ptr(new AcceptingState(std::move(p_session)));
  }

  virtual ~AcceptingState() {}

  virtual typename BaseState<Protocol>::type GetType() {
    return this->ACCEPTING;
  }

  virtual void Init() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    p_session->set_start_timestamp(Clock::now());
  }

  void Close() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    auto self = this->shared_from_this();
    // state session to closed state
    p_session->ChangeState(ClosedState::Create(this->get_io_context()));

    // stop timers
    boost::system::error_code timer_ec;
    timeout_timer_.cancel(timer_ec);

    // Unbind session
    p_session->Unbind();
  }

  virtual void OnConnectionDgr(ConnectionDatagramPtr p_connection_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    auto& header = p_connection_dgr->header();
    auto& payload = p_connection_dgr->payload();
    uint32_t receive_cookie = payload.syn_cookie();
    uint32_t client_socket = payload.socket_id();

    auto self = this->shared_from_this();
    if (receive_cookie == p_session->syn_cookie() &&
        client_socket == p_session->remote_socket_id()) {
      // Very first handshake response
      if (p_session->max_window_flow_size() == 0) {
        p_session->get_p_connection_info()->set_packet_data_size(std::min(
            static_cast<uint32_t>(Protocol::MTU),
            payload.maximum_packet_size() - Protocol::PACKET_SIZE_CORRECTION));
        p_session->set_max_window_flow_size(
            std::min(static_cast<uint32_t>(Protocol::MAXIMUM_WINDOW_FLOW_SIZE),
                     payload.maximum_window_flow_size()));
        p_session->set_window_flow_size(p_session->max_window_flow_size());
        p_session->set_init_packet_seq_num(
            payload.initial_packet_sequence_number());
        p_session->get_p_packet_seq_gen()->set_current(
            p_session->init_packet_seq_num());
        p_session->ChangeState(ConnectedState::Create(p_session));
        boost::system::error_code timer_ec;
        timeout_timer_.cancel(timer_ec);
      }

      // Reply handshake response
      header.set_destination_socket(p_session->remote_socket_id());
      payload.set_version(ConnectionDatagram::Payload::FORTH);
      payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
      payload.set_connection_type(ConnectionDatagram::Payload::FIRST_RESPONSE);
      payload.set_initial_packet_sequence_number(
          p_session->get_p_packet_seq_gen()->current());
      payload.set_syn_cookie(p_session->syn_cookie());
      payload.set_maximum_packet_size(
          p_session->connection_info().packet_data_size() +
          Protocol::PACKET_SIZE_CORRECTION);
      payload.set_maximum_window_flow_size(p_session->max_window_flow_size());
      payload.set_socket_id(p_session->socket_id());
    }

    p_session->AsyncSendControlPacket(
        *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
        ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
        [self, p_connection_dgr](const boost::system::error_code&,
                                 std::size_t) {});
  }

 private:
  AcceptingState(typename SocketSession::Ptr p_session)
      : BaseState<Protocol>(p_session->get_io_context()),
        p_session_(p_session),
        timeout_timer_(p_session->get_io_context()) {}

  void StartTimeoutTimer() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    timeout_timer_.expires_from_now(
        std::chrono::seconds(p_session->timeout_delay()));

    timeout_timer_.async_wait(boost::bind(&AcceptingState::HandleTimeoutTimer,
                                          this->shared_from_this(), boost::placeholders::_1));
  }

  void HandleTimeoutTimer(const boost::system::error_code& ec) {
    if (!ec) {
      // Accepting timeout
      Close();
    }
  }

 private:
  std::weak_ptr<SocketSession> p_session_;
  Timer timeout_timer_;
};

}  // namespace state
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_ACCEPTING_STATE_H_
