#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTING_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTING_STATE_H_

#include <cstdint>

#include <chrono>
#include <memory>

#include <boost/asio/basic_waitable_timer.hpp>

#include <chrono>

#include <boost/system/error_code.hpp>

#include "../../common/error/error.h"

#include "../io/connect_op.h"
#include "base_state.h"
#include "closed_state.h"
#include "connected_state.h"

#include "policy/drop_connection_policy.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class ConnectingState
    : public BaseState<Protocol>,
      public std::enable_shared_from_this<ConnectingState<Protocol>> {
 public:
  using Clock = typename Protocol::clock;
  using Timer = typename Protocol::timer;

  using Ptr = std::shared_ptr<ConnectingState>;
  using SocketSession = typename Protocol::socket_session;
  using ConnectionDatagram = typename Protocol::ConnectionDatagram;
  using ConnectionDatagramPtr = std::shared_ptr<ConnectionDatagram>;
  using ClosedState = state::ClosedState<Protocol>;
  using ConnectedState =
      state::ConnectedState<Protocol,
                            typename policy::DropConnectionPolicy<Protocol>>;

 public:
  static Ptr Create(
      typename SocketSession::Ptr p_socket_session,
      io::basic_pending_connect_operation<Protocol> *p_connection_op) {
    return Ptr(new ConnectingState(p_socket_session, p_connection_op));
  }

  virtual ~ConnectingState() {}

  virtual typename BaseState<Protocol>::type GetType() {
    return this->CONNECTING;
  }

  virtual void Init() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    p_session->set_start_timestamp(Clock::now());
    Connect();
  }

  virtual void Close() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    // Unbind session
    p_session->Unbind();
  }

  virtual void OnConnectionDgr(ConnectionDatagramPtr p_connection_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    auto self = this->shared_from_this();
    auto &header = p_connection_dgr->header();
    auto &payload = p_connection_dgr->payload();
    uint32_t receive_cookie = payload.syn_cookie();

    if (payload.IsSynCookie()) {
      // Async send datagram
      if (!p_session->syn_cookie()) {
        p_session->set_syn_cookie(receive_cookie);
      }

      header.set_destination_socket(0);
      payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
      payload.set_connection_type(ConnectionDatagram::Payload::FIRST_RESPONSE);
      payload.set_version(ConnectionDatagram::Payload::FORTH);
      payload.set_syn_cookie(p_session->syn_cookie());
      payload.set_socket_id(p_session->socket_id());
      payload.set_initial_packet_sequence_number(
          p_session->packet_seq_gen().current());
      payload.set_maximum_packet_size(Protocol::MTU);
      payload.set_maximum_window_flow_size(Protocol::MAXIMUM_WINDOW_FLOW_SIZE);

      auto self = this->shared_from_this();
      p_session->AsyncSendControlPacket(
          *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
          ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
          [self, p_connection_dgr](const boost::system::error_code &,
                                   std::size_t) {});
      return;
    }

    if (payload.IsServerHandshakeResponse()) {
      auto self = this->shared_from_this();

      p_session->set_remote_socket_id(payload.socket_id());

      // Stop sending connection handshake
      stop_sending_ = true;

      // Stop timers
      boost::system::error_code timer_ec;
      send_timer_.cancel(timer_ec);
      timeout_timer_.cancel(timer_ec);

      // Change to connected state and execute success handler
      p_session->get_p_connection_info()->set_packet_data_size(
          payload.maximum_packet_size() - Protocol::PACKET_SIZE_CORRECTION);
      p_session->set_max_window_flow_size(payload.maximum_window_flow_size());
      p_session->set_window_flow_size(p_session->max_window_flow_size());
      p_session->set_init_packet_seq_num(
          payload.initial_packet_sequence_number());

      auto p_connect_op = p_connection_op_;
      auto do_complete = [p_connect_op]() {
        boost::system::error_code ec(::common::error::success,
                                     ::common::error::get_error_category());
        p_connect_op->complete(ec);
      };

      p_session->ChangeState(ConnectedState::Create(p_session));

      this->get_io_context().post(std::move(do_complete));
    }
  }

 private:
  ConnectingState(
      typename SocketSession::Ptr p_session,
      io::basic_pending_connect_operation<Protocol> *p_connection_op)
      : BaseState<Protocol>(p_session->get_io_context()),
        p_session_(p_session),
        p_connection_op_(p_connection_op),
        send_timer_(p_session->get_io_context()),
        timeout_timer_(p_session->get_io_context()),
        stop_sending_(false) {}

  void Connect() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    // Init connection datagram
    auto p_connection_dgr = std::make_shared<ConnectionDatagram>();

    auto &payload = p_connection_dgr->payload();
    payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
    payload.set_initial_packet_sequence_number(
        p_session->packet_seq_gen().current());
    payload.set_maximum_packet_size(Protocol::MTU);
    payload.set_maximum_window_flow_size(Protocol::MAXIMUM_WINDOW_FLOW_SIZE);
    payload.set_connection_type(ConnectionDatagram::Payload::REGULAR);
    payload.set_socket_id(p_session->socket_id());

    StartTimeoutTimer();

    // Do not stop sending the init handshake datagram until timeout or
    // connection established
    p_session->AsyncSendControlPacket(
        *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
        ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
        boost::bind(&ConnectingState::SendLoopConnectionDgr,
                    this->shared_from_this(), p_connection_dgr, _1, _2));
  }

  void SendLoopConnectionDgr(
      ConnectionDatagramPtr p_connection_dgr,
      const boost::system::error_code &sent_ec = boost::system::error_code(),
      std::size_t length = 0) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (stop_sending_) {
      return;
    }

    if (sent_ec) {
      StopConnection();
      return;
    }

    auto self = this->shared_from_this();
    send_timer_.expires_from_now(std::chrono::milliseconds(250));
    send_timer_.async_wait(
        [p_connection_dgr, self, this](const boost::system::error_code &ec) {
          auto p_session = this->p_session_.lock();
          if (!p_session) {
            return;
          }

          if (ec) {
            // Timer was stopped or destroyed
            return;
          }

          p_session->AsyncSendControlPacket(
              *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
              ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
              boost::bind(&ConnectingState::SendLoopConnectionDgr, self,
                          p_connection_dgr, _1, _2));
        });
  }

  void StartTimeoutTimer() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    timeout_timer_.expires_from_now(
        std::chrono::seconds(p_session->timeout_delay()));

    timeout_timer_.async_wait(boost::bind(&ConnectingState::HandleTimeoutTimer,
                                          this->shared_from_this(), _1));
  }

  void HandleTimeoutTimer(const boost::system::error_code &ec) {
    if (!ec) {
      StopConnection();
    }
  }

  void StopConnection() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    stop_sending_ = true;
    auto self = this->shared_from_this();
    Close();

    // Stop timers
    boost::system::error_code timer_ec;
    send_timer_.cancel(timer_ec);
    timeout_timer_.cancel(timer_ec);

    // call session connection handler with connection aborted
    auto p_connect_op = p_connection_op_;
    auto do_complete = [p_connect_op]() {
      p_connect_op->complete(
          boost::system::error_code(::common::error::connection_aborted,
                                    ::common::error::get_error_category()));
    };
    this->get_io_context().post(std::move(do_complete));
  }

 private:
  std::weak_ptr<SocketSession> p_session_;
  io::basic_pending_connect_operation<Protocol> *p_connection_op_;
  Timer send_timer_;
  Timer timeout_timer_;
  bool stop_sending_;
};

}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTING_STATE_H_
