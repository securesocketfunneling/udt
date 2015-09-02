#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTING_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTING_STATE_H_

#include <cstdint>

#include <chrono>
#include <memory>

#include <boost/asio/basic_waitable_timer.hpp>

#include <boost/chrono.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>

#include "udt/common/error/error.h"

#include "udt/connected_protocol/state/base_state.h"
#include "udt/connected_protocol/state/closed_state.h"
#include "udt/connected_protocol/state/connected_state.h"

#include "udt/connected_protocol/state/policy/drop_connection_policy.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class ConnectingState
    : public BaseState<Protocol>,
      public std::enable_shared_from_this<ConnectingState<Protocol>> {
 public:
  typedef typename Protocol::clock Clock;
  typedef typename Protocol::timer Timer;

  typedef std::shared_ptr<ConnectingState> Ptr;
  typedef typename Protocol::socket_session SocketSession;
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;
  typedef state::ClosedState<Protocol> ClosedState;
  typedef state::ConnectedState<
      Protocol, typename policy::DropConnectionPolicy<Protocol>> ConnectedState;

 public:
  static Ptr Create(typename SocketSession::Ptr p_socket_session) {
    return Ptr(new ConnectingState(p_socket_session));
  }

  virtual ~ConnectingState() {}

  virtual typename BaseState<Protocol>::type GetType() {
    return this->CONNECTING;
  }

  virtual boost::asio::io_service &get_io_service() {
    return p_session_->get_io_service();
  }

  virtual void Init() {
    p_session_->start_timestamp = Clock::now();
    Connect();
  }

  virtual void Close() {
    // Unbind session
    p_session_->Unbind();
  }

  virtual void OnConnectionDgr(ConnectionDatagramPtr p_connection_dgr) {
    auto self = this->shared_from_this();
    auto &header = p_connection_dgr->header();
    auto &payload = p_connection_dgr->payload();
    uint32_t receive_cookie = payload.syn_cookie();

    if (payload.IsSynCookie()) {
      // Async send datagram

      {
        boost::recursive_mutex::scoped_lock lock(p_session_->mutex);
        if (!p_session_->syn_cookie) {
          p_session_->syn_cookie = receive_cookie;
        }
      }

      header.set_destination_socket(0);
      payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
      payload.set_connection_type(ConnectionDatagram::Payload::FIRST_RESPONSE);
      payload.set_version(ConnectionDatagram::Payload::FORTH);
      payload.set_syn_cookie(p_session_->syn_cookie);
      payload.set_socket_id(p_session_->socket_id);
      payload.set_initial_packet_sequence_number(
          p_session_->packet_seq_gen.current());
      payload.set_maximum_packet_size(Protocol::MTU);
      payload.set_maximum_window_flow_size(Protocol::MAXIMUM_WINDOW_FLOW_SIZE);

      auto self = this->shared_from_this();
      p_session_->AsyncSendControlPacket(
          *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
          ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
          [self, p_connection_dgr](const boost::system::error_code &,
                                   std::size_t) {});
      return;
    }

    if (payload.IsServerHandshakeResponse()) {
      auto self = this->shared_from_this();

      {
        boost::recursive_mutex::scoped_lock lock(p_session_->mutex);
        p_session_->remote_socket_id = payload.socket_id();
      }

      // Stop sending connection handshake
      stop_sending_ = true;

      // Stop timers
      boost::system::error_code timer_ec;
      send_timer_.cancel(timer_ec);
      timeout_timer_.cancel(timer_ec);

      // Change to connected state and execute success handler

      {
        boost::recursive_mutex::scoped_lock lock(p_session_->mutex);
        p_session_->connection_info.set_packet_data_size(
            payload.maximum_packet_size() - Protocol::PACKET_SIZE_CORRECTION);
        p_session_->max_window_flow_size = payload.maximum_window_flow_size();
        p_session_->window_flow_size = p_session_->max_window_flow_size;
        p_session_->init_packet_seq_num =
            payload.initial_packet_sequence_number();
        p_session_->ChangeState(ConnectedState::Create(p_session_));
      }

      auto connect_op = p_session_->connection_op;
      auto do_complete = [connect_op]() {
        boost::system::error_code ec(::common::error::success,
                                     ::common::error::get_error_category());
        connect_op->complete(ec);
      };
      p_session_->get_io_service().post(std::move(do_complete));
    }
  }

 private:
  ConnectingState(typename SocketSession::Ptr p_session)
      : BaseState<Protocol>(),
        p_session_(p_session),
        send_timer_(p_session_->get_io_service()),
        timeout_timer_(p_session_->get_io_service()),
        stop_sending_(false) {}

  void Connect() {
    // Init connection datagram
    auto p_connection_dgr = std::make_shared<ConnectionDatagram>();

    auto &payload = p_connection_dgr->payload();
    payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
    payload.set_initial_packet_sequence_number(
        p_session_->packet_seq_gen.current());
    payload.set_maximum_packet_size(Protocol::MTU);
    payload.set_maximum_window_flow_size(Protocol::MAXIMUM_WINDOW_FLOW_SIZE);
    payload.set_connection_type(ConnectionDatagram::Payload::REGULAR);
    payload.set_socket_id(p_session_->socket_id);

    StartTimeoutTimer();

    // Do not stop sending the init handshake datagram until timeout or
    // connection established
    p_session_->AsyncSendControlPacket(
        *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
        ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
        boost::bind(&ConnectingState::SendLoopConnectionDgr,
                    this->shared_from_this(), this->shared_from_this(),
                    p_connection_dgr, _1, _2));
  }

  void SendLoopConnectionDgr(
      Ptr self, ConnectionDatagramPtr p_connection_dgr,
      const boost::system::error_code &sent_ec = boost::system::error_code(),
      std::size_t length = 0) {
    if (stop_sending_) {
      return;
    }

    if (sent_ec) {
      self->StopConnection();
      return;
    }

    self->send_timer_.expires_from_now(boost::chrono::milliseconds(250));
    self->send_timer_.async_wait(
        [p_connection_dgr, self, this](const boost::system::error_code &ec) {
          if (ec) {
            // Timer was stopped or destroyed
            return;
          }

          this->p_session_->AsyncSendControlPacket(
              *p_connection_dgr, ConnectionDatagram::Header::CONNECTION,
              ConnectionDatagram::Header::NO_ADDITIONAL_INFO,
              boost::bind(&ConnectingState::SendLoopConnectionDgr, self, self,
                          p_connection_dgr, _1, _2));
        });
  }

  void StartTimeoutTimer() {
    timeout_timer_.expires_from_now(
        boost::chrono::seconds(p_session_->timeout_delay));

    timeout_timer_.async_wait(boost::bind(&ConnectingState::HandleTimeoutTimer,
                                          this->shared_from_this(), _1));
  }

  void HandleTimeoutTimer(const boost::system::error_code &ec) {
    if (!ec) {
      StopConnection();
    }
  }

  void StopConnection() {
    stop_sending_ = true;
    auto self = this->shared_from_this();
    Close();

    // Stop timers
    boost::system::error_code timer_ec;
    send_timer_.cancel(timer_ec);
    timeout_timer_.cancel(timer_ec);

    // call session connection handler with connection aborted
    auto connect_op = p_session_->connection_op;
    auto do_complete = [connect_op]() {
      connect_op->complete(
          boost::system::error_code(::common::error::connection_aborted,
                                    ::common::error::get_error_category()));
    };
    p_session_->get_io_service().post(std::move(do_complete));
  }

 private:
  typename SocketSession::Ptr p_session_;
  Timer send_timer_;
  Timer timeout_timer_;
  bool stop_sending_;
};

}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTING_STATE_H_
