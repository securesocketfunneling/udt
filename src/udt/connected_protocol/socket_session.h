#ifndef UDT_CONNECTED_PROTOCOL_SOCKET_SESSION_H_
#define UDT_CONNECTED_PROTOCOL_SOCKET_SESSION_H_

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <memory>
#include <set>

#include <boost/asio/io_service.hpp>
#include <boost/asio/socket_base.hpp>

#include <boost/chrono.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include "udt/connected_protocol/io/connect_op.h"
#include "udt/connected_protocol/io/write_op.h"
#include "udt/connected_protocol/io/read_op.h"

#include "udt/connected_protocol/cache/connection_info.h"

#include "udt/connected_protocol/common/observer.h"

#include "udt/connected_protocol/logger/log_entry.h"

#include "udt/connected_protocol/sequence_generator.h"
#include "udt/connected_protocol/state/base_state.h"
#include "udt/connected_protocol/state/closed_state.h"

namespace connected_protocol {

template <class Protocol>
class SocketSession
    : public std::enable_shared_from_this<SocketSession<Protocol>> {
 private:
  typedef typename Protocol::endpoint Endpoint;
  typedef std::shared_ptr<Endpoint> EndpointPtr;
  typedef typename Protocol::next_layer_protocol::endpoint NextLayerEndpoint;

 private:
  typedef std::shared_ptr<typename Protocol::multiplexer> MultiplexerPtr;
  typedef std::shared_ptr<typename Protocol::flow> FlowPtr;
  typedef cache::ConnectionInfo ConnectionInfo;
  typedef cache::ConnectionInfo::Ptr ConnectionInfoPtr;
  typedef typename common::Observer<SocketSession> SessionObserver;
  typedef SessionObserver* SessionObserverPtr;
  typedef state::ClosedState<Protocol> ClosedState;
  typedef
      typename connected_protocol::state::BaseState<Protocol>::Ptr BaseStatePtr;

 private:
  typedef typename Protocol::time_point TimePoint;
  typedef typename Protocol::clock Clock;
  typedef typename Protocol::timer Timer;
  typedef typename Protocol::logger Logger;

 private:
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;
  typedef typename Protocol::GenericControlDatagram ControlDatagram;
  typedef typename ControlDatagram::Header ControlHeader;
  typedef std::shared_ptr<ControlDatagram> ControlDatagramPtr;
  typedef typename Protocol::SendDatagram SendDatagram;
  typedef std::shared_ptr<SendDatagram> SendDatagramPtr;
  typedef typename Protocol::DataDatagram DataDatagram;
  typedef std::shared_ptr<DataDatagram> DataDatagramPtr;
  typedef typename Protocol::AckDatagram AckDatagram;
  typedef std::shared_ptr<AckDatagram> AckDatagramPtr;
  typedef typename Protocol::AckOfAckDatagram AckOfAckDatagram;
  typedef std::shared_ptr<AckOfAckDatagram> AckOfAckDatagramPtr;
  typedef uint32_t PacketSequenceNumber;
  typedef uint32_t SocketId;

 public:
  typedef std::shared_ptr<SocketSession> Ptr;

 public:
  static Ptr Create(MultiplexerPtr p_multiplexer, FlowPtr p_flow) {
    Ptr p_session(
        new SocketSession(std::move(p_multiplexer), std::move(p_flow)));
    p_session->Init();

    return p_session;
  }

  void set_next_remote_endpoint(const NextLayerEndpoint& next_remote_ep) {
    if (next_remote_endpoint_ == NextLayerEndpoint()) {
      next_remote_endpoint_ = next_remote_ep;
      p_connection_info_cache =
          Protocol::connections_info_manager_.GetConnectionInfo(next_remote_ep);
      connection_info = *p_connection_info_cache;
    }
  }

  const NextLayerEndpoint& next_remote_endpoint() {
    return next_remote_endpoint_;
  }

  void set_next_local_endpoint(const NextLayerEndpoint& next_local_ep) {
    if (next_local_endpoint_ == NextLayerEndpoint()) {
      next_local_endpoint_ = next_local_ep;
    }
  }

  const NextLayerEndpoint& next_local_endpoint() {
    return next_local_endpoint_;
  }

  bool IsClosed() {
    return state::BaseState<Protocol>::CLOSED == p_state_->GetType();
  }

  void Close() {
    auto p_state = p_state_;
    p_state_->Close();
  }

  void PushReadOp(io::basic_pending_stream_read_operation<Protocol>* read_op) {
    auto p_state = p_state_;
    p_state_->PushReadOp(read_op);
  }

  void PushWriteOp(io::basic_pending_write_operation* write_op) {
    auto p_state = p_state_;
    p_state_->PushWriteOp(write_op);
  }

  void PushConnectionDgr(ConnectionDatagramPtr p_connection_dgr) {
    auto p_state = p_state_;
    p_state_->OnConnectionDgr(p_connection_dgr);
  }

  void PushControlDgr(ControlDatagram* p_control_dgr) {
    auto p_state = p_state_;
    p_state_->OnControlDgr(p_control_dgr);
  }

  void PushDataDgr(DataDatagram* p_datagram) {
    auto p_state = p_state_;
    p_state_->OnDataDgr(p_datagram);
  }

  bool HasPacketToSend() {
    auto p_state = p_state_;
    return p_state_->HasPacketToSend();
  }

  SendDatagram* NextScheduledPacket() {
    auto p_state = p_state_;
    return p_state_->NextScheduledPacket();
  }

  boost::chrono::nanoseconds NextScheduledPacketTime() {
    auto p_state = p_state_;
    return p_state_->NextScheduledPacketTime();
  }

  // High priority sending : use for control packet only
  template <class Datagram, class Handler>
  void AsyncSendControlPacket(Datagram& datagram,
                              typename ControlHeader::type type,
                              uint32_t additional_info, Handler handler) {
    FillControlHeader(&(datagram.header()), type, additional_info);
    p_multiplexer_->AsyncSendControlPacket(datagram, next_remote_endpoint_,
                                           handler);
  }

  template <class Datagram, class Handler>
  void AsyncSendPacket(Datagram* p_datagram, Handler handler) {
    p_multiplexer_->AsyncSendDataPacket(p_datagram, next_remote_endpoint_,
                                        handler);
  }

  // State management
  void AddObserver(SessionObserverPtr p_observer) {
    boost::recursive_mutex::scoped_lock lock(mutex);
    observers_.insert(p_observer);
  }

  void RemoveObserver(SessionObserverPtr p_observer) {
    boost::recursive_mutex::scoped_lock lock(mutex);
    observers_.erase(p_observer);
  }

  // Change session's current state
  void ChangeState(BaseStatePtr p_new_state) {
    auto p_state = p_state_;
    if (p_state_) {
      p_state_->Stop();
    }
    p_state_ = std::move(p_new_state);
    p_state_->Init();
    NotifyObservers();
  }

  void Unbind() {
    boost::system::error_code ec;
    logger_timer_.cancel(ec);
    p_multiplexer_->RemoveSocketSession(next_remote_endpoint_, socket_id);
  }

  typename connected_protocol::state::BaseState<Protocol>::type GetState() {
    return p_state_->GetType();
  }

  uint32_t get_window_flow_size() { return window_flow_size.load(); }

  boost::asio::io_service& get_timer_io_service() {
    return p_multiplexer_->get_timer_io_service();
  }

  boost::asio::io_service& get_io_service() {
    return p_multiplexer_->get_io_service();
  }

 private:
  SocketSession(MultiplexerPtr p_multiplexer, FlowPtr p_fl)
      : p_flow(std::move(p_fl)),
        message_seq_gen(Protocol::MAX_MSG_SEQUENCE_NUMBER),
        packet_seq_gen(Protocol::MAX_PACKET_SEQUENCE_NUMBER),
        ack_seq_gen(Protocol::MAX_ACK_SEQUENCE_NUMBER),
        syn_cookie(0),
        socket_id(0),
        remote_socket_id(0),
        timeout_delay(60),
        max_window_flow_size(0),
        window_flow_size(0),
        p_multiplexer_(std::move(p_multiplexer)),
        observers_(),
        p_state_(ClosedState::Create(p_multiplexer_->get_io_service())),
        logger_timer_(p_multiplexer_->get_timer_io_service()),
        logger_() {
    // initialize local endpoint with multiplexer's one
    boost::system::error_code ec;
    next_local_endpoint_ = p_multiplexer_->local_endpoint(ec);
  }

  void Init() { LaunchLoggerTimer(); }

  void LaunchLoggerTimer() {
    if (Logger::ACTIVE) {
      ResetLog();
      logger_timer_.expires_from_now(
          boost::chrono::milliseconds(Logger::FREQUENCY));
      logger_timer_.async_wait(boost::bind(&SocketSession::LoggerTimerHandler,
                                           this->shared_from_this(), _1));
    }
  }

  void LoggerTimerHandler(const boost::system::error_code& ec) {
    if (!ec) {
      logger::LogEntry log_entry;
      Log(&log_entry);
      p_state_->Log(&log_entry);
      p_flow->Log(&log_entry);
      p_multiplexer_->Log(&log_entry);

      logger_.Log(log_entry);
      LaunchLoggerTimer();
    }
  }

  void Log(connected_protocol::logger::LogEntry* p_log) {
    p_log->sending_period = connection_info.sending_period();
    p_log->cc_window_flow_size = connection_info.window_flow_size();
    p_log->remote_window_flow_size = get_window_flow_size();
    p_log->remote_arrival_speed = connection_info.packet_arrival_speed();
    p_log->remote_estimated_link_capacity =
        connection_info.estimated_link_capacity();
    p_log->rtt = connection_info.rtt().count();
    p_log->rtt_var = connection_info.rtt_var().count();
    p_log->ack_period = connection_info.ack_period().count();
  }

  void ResetLog() {
    p_flow->ResetLog();
    p_multiplexer_->ResetLog();
    p_state_->ResetLog();
  }

  void FillControlHeader(ControlHeader* p_control_header,
                         typename ControlHeader::type type,
                         uint32_t additional_info) {
    p_control_header->set_flags(type);
    p_control_header->set_additional_info(additional_info);
    p_control_header->set_destination_socket(remote_socket_id);
    p_control_header->set_timestamp(
        (uint32_t)(boost::chrono::duration_cast<boost::chrono::microseconds>(
                       Clock::now() - start_timestamp)
                       .count()));
  }

  void NotifyObservers() {
    boost::recursive_mutex::scoped_lock lock(mutex);
    for (auto& p_observer : observers_) {
      p_observer->Notify(this);
    }
  }

 public:
  FlowPtr p_flow;
  ConnectionInfo connection_info;
  ConnectionInfoPtr p_connection_info_cache;
  SequenceGenerator message_seq_gen;
  SequenceGenerator packet_seq_gen;
  SequenceGenerator ack_seq_gen;
  uint32_t syn_cookie;
  SocketId socket_id;
  SocketId remote_socket_id;
  PacketSequenceNumber init_packet_seq_num;
  int timeout_delay;
  boost::recursive_mutex mutex;
  uint32_t max_window_flow_size;
  std::atomic<uint32_t> window_flow_size;
  io::basic_pending_connect_operation<Protocol>* connection_op;
  TimePoint start_timestamp;

 private:
  MultiplexerPtr p_multiplexer_;
  std::set<SessionObserverPtr> observers_;
  BaseStatePtr p_state_;
  NextLayerEndpoint next_local_endpoint_;
  NextLayerEndpoint next_remote_endpoint_;
  Timer logger_timer_;
  Logger logger_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_SOCKET_SESSION_H_
