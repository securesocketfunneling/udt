#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_STATE_H_

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

#include <boost/bind.hpp>

#include "../io/buffers.h"
#include "../io/read_op.h"
#include "../io/write_op.h"

#include "base_state.h"

#include "connected/receiver.h"
#include "connected/sender.h"

namespace connected_protocol {
namespace state {

template <class Protocol, class ConnectionPolicy>
class ConnectedState : public BaseState<Protocol>,
                       public std::enable_shared_from_this<
                           ConnectedState<Protocol, ConnectionPolicy>>,
                       public ConnectionPolicy {
 public:
  using Ptr = std::shared_ptr<ConnectedState>;

 private:
  using CongestionControl = typename Protocol::congestion_control;
  using SocketSession = typename Protocol::socket_session;
  using Clock = typename Protocol::clock;
  using Timer = typename Protocol::timer;
  using TimePoint = typename Protocol::time_point;
  using Logger = typename Protocol::logger;

 private:
  using SendDatagram = typename Protocol::SendDatagram;
  using DataDatagram = typename Protocol::DataDatagram;
  using ConnectionDatagram = typename Protocol::ConnectionDatagram;
  using ConnectionDatagramPtr = std::shared_ptr<ConnectionDatagram>;
  using ControlDatagram = typename Protocol::GenericControlDatagram;
  using AckDatagram = typename Protocol::AckDatagram;
  using AckDatagramPtr = std::shared_ptr<AckDatagram>;
  using NAckDatagram = typename Protocol::NAckDatagram;
  using NAckDatagramPtr = std::shared_ptr<NAckDatagram>;
  using AckOfAckDatagram = typename Protocol::AckOfAckDatagram;
  using AckOfAckDatagramPtr = std::shared_ptr<AckOfAckDatagram>;
  using KeepAliveDatagram = typename Protocol::KeepAliveDatagram;
  using KeepAliveDatagramPtr = std::shared_ptr<KeepAliveDatagram>;
  using ShutdownDatagram = typename Protocol::ShutdownDatagram;
  using ShutdownDatagramPtr = std::shared_ptr<ShutdownDatagram>;

 private:
  using ClosedState = typename state::ClosedState<Protocol>;
  using Sender = typename connected::Sender<Protocol, ConnectedState>;
  using Receiver = typename connected::Receiver<Protocol, ConnectedState>;

 private:
  using PacketSequenceNumber = uint32_t;
  using AckSequenceNumber = uint32_t;

 public:
  static Ptr Create(typename SocketSession::Ptr p_session) {
    return Ptr(new ConnectedState(std::move(p_session)));
  }

  virtual ~ConnectedState() {}

  virtual typename BaseState<Protocol>::type GetType() {
    return this->CONNECTED;
  }

  virtual void Init() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    receiver_.Init(this->shared_from_this(), p_session->init_packet_seq_num());
    sender_.Init(this->shared_from_this(), &congestion_control_);
    congestion_control_.Init(p_session->init_packet_seq_num(),
                             p_session->max_window_flow_size());

    ack_timer_.expires_from_now(p_session->connection_info().ack_period());
    ack_timer_.async_wait(boost::bind(&ConnectedState::AckTimerHandler,
                                      this->shared_from_this(), _1, false));
    exp_timer_.expires_from_now(p_session->connection_info().exp_period());
    exp_timer_.async_wait(boost::bind(&ConnectedState::ExpTimerHandler,
                                      this->shared_from_this(), _1));

    /* Nack not send periodically anymore
    receiver_.nack_timer.expires_from_now(
        p_session_->connection_info.nack_period());
    receiver_.nack_timer.async_wait(boost::bind(
        &ConnectedState::NAckTimerHandler, this->shared_from_this(), _1));*/
  }

  virtual void Stop() {
    StopTimers();
    StopServices();
    CloseConnection();
  }

  virtual void Close() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (!closed_.load()) {
      closed_ = true;
      p_session->ChangeState(ClosedState::Create(this->get_io_context()));
    }
  }

  virtual void OnDataDgr(DataDatagram* p_datagram) {
    if (closed_.load()) {
      return;
    }

    ResetExp(false);

    if (Logger::ACTIVE) {
      received_count_ = received_count_.load() + 1;
    }

    congestion_control_.OnPacketReceived(*p_datagram);
    receiver_.OnDataDatagram(p_datagram);

    packet_received_since_light_ack_ =
        packet_received_since_light_ack_.load() + 1;
    if (packet_received_since_light_ack_.load() >= 64) {
      AckTimerHandler(boost::system::error_code(), true);
    }
  }

  virtual void PushReadOp(
      io::basic_pending_stream_read_operation<Protocol>* read_op) {
    if (closed_.load()) {
      return;
    }
    receiver_.PushReadOp(read_op);
  }

  virtual void PushWriteOp(io::basic_pending_write_operation* write_op) {
    if (closed_.load()) {
      return;
    }
    sender_.PushWriteOp(write_op);
  }

  virtual bool HasPacketToSend() { return sender_.HasPacketToSend(); }

  virtual std::chrono::nanoseconds NextScheduledPacketTime() {
    return sender_.NextScheduledPacketTime();
  }

  virtual SendDatagram* NextScheduledPacket() {
    SendDatagram* p_datagram(sender_.NextScheduledPacket());
    if (p_datagram) {
      congestion_control_.OnPacketSent(*p_datagram);
    }

    return p_datagram;
  }

  virtual void OnConnectionDgr(ConnectionDatagramPtr p_connection_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }
    if (closed_.load()) {
      return;
    }
    // Call policy to process connection datagram
    this->ProcessConnectionDgr(p_session.get(), std::move(p_connection_dgr));
  }

  virtual void OnControlDgr(ControlDatagram* p_control_dgr) {
    if (closed_.load()) {
      return;
    }
    switch (p_control_dgr->header().flags()) {
      case ControlDatagram::Header::KEEP_ALIVE:
        ResetExp(false);
        break;
      case ControlDatagram::Header::ACK: {
        ResetExp(true);
        AckDatagram ack_dgr;
        boost::asio::buffer_copy(ack_dgr.GetMutableBuffers(),
                                 p_control_dgr->GetConstBuffers());
        ack_dgr.payload().set_payload_size(p_control_dgr->payload().GetSize());
        OnAck(ack_dgr);
        break;
      }
      case ControlDatagram::Header::NACK: {
        ResetExp(true);
        NAckDatagram nack_dgr;
        nack_dgr.payload().SetSize(p_control_dgr->payload().GetSize());
        boost::asio::buffer_copy(nack_dgr.GetMutableBuffers(),
                                 p_control_dgr->GetConstBuffers());
        OnNAck(nack_dgr);
        break;
      }
      case ControlDatagram::Header::SHUTDOWN:
        ResetExp(false);
        this->Close();
        break;
      case ControlDatagram::Header::ACK_OF_ACK: {
        ResetExp(false);
        AckOfAckDatagram ack_of_ack_dgr;
        boost::asio::buffer_copy(ack_of_ack_dgr.GetMutableBuffers(),
                                 p_control_dgr->GetConstBuffers());
        OnAckOfAck(ack_of_ack_dgr);
        break;
      }
      case ControlDatagram::Header::MESSAGE_DROP_REQUEST:
        ResetExp(false);
        break;
    }
  }

 private:
  ConnectedState(typename SocketSession::Ptr p_session)
      : BaseState<Protocol>(p_session->get_io_context()),
        p_session_(p_session),
        sender_(p_session),
        receiver_(p_session),
        unqueue_write_op_(false),
        congestion_control_(p_session->get_p_connection_info()),
        stop_timers_(false),
        ack_timer_(p_session->get_io_context()),
        nack_timer_(p_session->get_io_context()),
        exp_timer_(p_session->get_io_context()),
        closed_(false),
        nack_count_(0),
        ack_count_(0),
        ack_sent_count_(0),
        ack2_count_(0),
        ack2_sent_count_(0),
        received_count_(0),
        packet_received_since_light_ack_(0) {}

 private:
  void StopServices() {
    sender_.Stop();
    receiver_.Stop();
  }

  // Timer processing
 private:
  void StopTimers() {
    boost::system::error_code ec;
    stop_timers_ = true;
    ack_timer_.cancel(ec);
    nack_timer_.cancel(ec);
    exp_timer_.cancel(ec);
  }

  void AckTimerHandler(const boost::system::error_code& ec,
                       bool light_ack = false) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (stop_timers_.load()) {
      return;
    }

    if (!light_ack) {
      LaunchAckTimer();
    }

    PacketSequenceNumber ack_number =
        receiver_.AckNumber(p_session->packet_seq_gen());

    if (!light_ack &&
        (ack_number == receiver_.largest_ack_number_acknowledged() ||
         ((ack_number == receiver_.last_ack_number()) &&
          std::chrono::duration_cast<std::chrono::microseconds>(
              Clock::now() - receiver_.last_ack_timestamp()) <
              2 * p_session->connection_info().rtt()))) {
      return;
    }

    if (Logger::ACTIVE) {
      ack_sent_count_ = ack_sent_count_.load() + 1;
    }

    auto* p_ack_seq_gen = p_session->get_p_ack_seq_gen();
    AckDatagramPtr p_ack_datagram = std::make_shared<AckDatagram>();
    auto& header = p_ack_datagram->header();
    auto& payload = p_ack_datagram->payload();
    AckSequenceNumber ack_seq_num = p_ack_seq_gen->current();
    p_ack_seq_gen->Next();

    payload.set_max_packet_sequence_number(ack_number);
    if (light_ack && packet_received_since_light_ack_.load() >= 64) {
      packet_received_since_light_ack_ = 0;
      payload.SetAsLightAck();
    } else {
      payload.SetAsFullAck();
      payload.set_rtt(
          static_cast<uint32_t>(p_session->connection_info().rtt().count()));
      payload.set_rtt_var(static_cast<uint32_t>(
          p_session->connection_info().rtt_var().count()));
      uint32_t available_buffer(receiver_.AvailableReceiveBufferSize());

      if (available_buffer < 2) {
        available_buffer = 2;
      }

      payload.set_available_buffer_size(available_buffer);

      payload.set_packet_arrival_speed(
          static_cast<uint32_t>(ceil(receiver_.GetPacketArrivalSpeed())));
      payload.set_estimated_link_capacity(
          static_cast<uint32_t>(ceil(receiver_.GetEstimatedLinkCapacity())));
    }

    // register ack
    receiver_.StoreAck(ack_seq_num, ack_number, light_ack);
    receiver_.set_last_ack_number(ack_number);

    header.set_timestamp(static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            receiver_.last_ack_timestamp() - p_session->start_timestamp())
            .count()));

    auto self = this->shared_from_this();

    p_session->AsyncSendControlPacket(
        *p_ack_datagram, AckDatagram::Header::ACK, ack_seq_num,
        [self, p_ack_datagram](const boost::system::error_code&, std::size_t) {
        });
  }

  void LaunchAckTimer() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (stop_timers_.load()) {
      return;
    }
    ack_timer_.expires_from_now(p_session->connection_info().ack_period());
    ack_timer_.async_wait(boost::bind(&ConnectedState::AckTimerHandler,
                                      this->shared_from_this(), _1, false));
  }

  virtual void Log(connected_protocol::logger::LogEntry* p_log) {
    p_log->received_count = received_count_.load();
    p_log->nack_count = nack_count_.load();
    p_log->ack_count = ack_count_.load();
    p_log->ack2_count = ack2_count_.load();
    p_log->local_arrival_speed = receiver_.GetPacketArrivalSpeed();
    p_log->local_estimated_link_capacity = receiver_.GetEstimatedLinkCapacity();
    p_log->ack_sent_count = ack_sent_count_.load();
    p_log->ack2_sent_count = ack2_sent_count_.load();
  }

  void ResetLog() {
    nack_count_ = 0;
    ack_count_ = 0;
    ack2_count_ = 0;
    received_count_ = 0;
    ack_sent_count_ = 0;
    ack2_sent_count_ = 0;
  }

  virtual double PacketArrivalSpeed() {
    return receiver_.GetPacketArrivalSpeed();
  }

  virtual double EstimatedLinkCapacity() {
    return receiver_.GetEstimatedLinkCapacity();
  }

  void NAckTimerHandler(const boost::system::error_code& ec) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (stop_timers_.load()) {
      return;
    }
    nack_timer_.expires_from_now(p_session->connection_info.nack_period());
    nack_timer_.async_wait(boost::bind(&ConnectedState::NAckTimerHandler,
                                       this->shared_from_this(), _1));
  }

  /// Reset expiration
  /// @param with_timer reset the timer as well
  void ResetExp(bool with_timer) {
    receiver_.ResetExpCounter();

    if (with_timer || !sender_.HasNackPackets()) {
      boost::system::error_code ec;
      exp_timer_.cancel(ec);
    }
  }

  void LaunchExpTimer() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (stop_timers_.load()) {
      return;
    }

    p_session->get_p_connection_info()->UpdateExpPeriod(receiver_.exp_count());

    exp_timer_.expires_from_now(p_session->connection_info().exp_period());
    exp_timer_.async_wait(boost::bind(&ConnectedState::ExpTimerHandler,
                                      this->shared_from_this(), _1));
  }

  void ExpTimerHandler(const boost::system::error_code& ec) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (stop_timers_.load()) {
      return;
    }

    if (ec) {
      LaunchExpTimer();
      return;
    }

    if (!sender_.HasLossPackets()) {
      sender_.UpdateLossListFromNackPackets();
    }

    // session expired -> exp count > 16 && 10 seconds since last reset exp
    // counter
    if (receiver_.HasTimeout()) {
      std::cerr << "Connected state : timeout\n";
      congestion_control_.OnTimeout();
      Close();
      return;
    }

    if (!sender_.HasLossPackets()) {
      // send keep alive datagram
      auto self = this->shared_from_this();
      KeepAliveDatagramPtr p_keep_alive_dgr =
          std::make_shared<KeepAliveDatagram>();

      p_session->AsyncSendControlPacket(
          *p_keep_alive_dgr, KeepAliveDatagram::Header::KEEP_ALIVE,
          KeepAliveDatagram::Header::NO_ADDITIONAL_INFO,
          [self, p_keep_alive_dgr](const boost::system::error_code&,
                                   std::size_t) {});
    }

    receiver_.IncExpCounter();

    LaunchExpTimer();
  }

  // Packet processing
 private:
  void OnAck(const AckDatagram& ack_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    auto self = this->shared_from_this();
    const auto& packet_seq_gen = p_session->packet_seq_gen();
    auto& header = ack_dgr.header();
    auto& payload = ack_dgr.payload();
    AckSequenceNumber ack_seq_num = header.additional_info();

    if (Logger::ACTIVE) {
      ack_count_ = ack_count_.load() + 1;
    }

    // Send ACK2 immediatly (RTT computing)
    AckOfAckDatagramPtr p_ack2_dgr = std::make_shared<AckOfAckDatagram>();
    p_session->AsyncSendControlPacket(
        *p_ack2_dgr, AckOfAckDatagram::Header::ACK_OF_ACK, ack_seq_num,
        [self, p_ack2_dgr](const boost::system::error_code&, std::size_t) {
          if (Logger::ACTIVE) {
            self->ack2_sent_count_ = self->ack2_sent_count_.load() + 1;
          }
        });

    PacketSequenceNumber packet_ack_number =
        GetPacketSequenceValue(payload.max_packet_sequence_number());
    // Ack packets which have been received
    sender_.AckPackets(packet_ack_number);

    receiver_.set_last_ack2_seq_number(ack_seq_num);

    if (payload.IsLightAck()) {
      if (packet_seq_gen.Compare(packet_ack_number,
                                 receiver_.largest_acknowledged_seq_number()) >=
          0) {
        // available buffer size in packets
        int32_t offset = packet_seq_gen.SeqOffset(
            receiver_.largest_acknowledged_seq_number(), packet_ack_number);
        // update remote window flow
        p_session->set_window_flow_size(p_session->window_flow_size() - offset);
        receiver_.set_largest_acknowledged_seq_number(packet_ack_number);
      }
      return;
    }

    p_session->get_p_connection_info()->UpdateRTT(payload.rtt());
    uint32_t rtt_var = static_cast<uint32_t>(
        std::abs(static_cast<long long>(payload.rtt()) -
                 p_session->connection_info().rtt().count()));
    p_session->get_p_connection_info()->UpdateRTTVar(rtt_var);
    p_session->get_p_connection_info()->UpdateAckPeriod();
    p_session->get_p_connection_info()->UpdateNAckPeriod();

    congestion_control_.OnAck(ack_dgr, packet_seq_gen);

    if (payload.IsFull()) {
      uint32_t arrival_speed = payload.packet_arrival_speed();
      uint32_t estimated_link = payload.estimated_link_capacity();
      if (arrival_speed > 0) {
        p_session->get_p_connection_info()->UpdatePacketArrivalSpeed(
            static_cast<double>(payload.packet_arrival_speed()));
      }
      if (estimated_link > 0) {
        p_session->get_p_connection_info()->UpdateEstimatedLinkCapacity(
            static_cast<double>(payload.estimated_link_capacity()));
      }
    }

    if (packet_seq_gen.Compare(packet_ack_number,
                               receiver_.largest_acknowledged_seq_number()) >=
        0) {
      receiver_.set_largest_acknowledged_seq_number(packet_ack_number);
      // available buffer size in packets
      p_session->set_window_flow_size(payload.available_buffer_size());
    }
  }

  void OnNAck(const NAckDatagram& nack_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (Logger::ACTIVE) {
      nack_count_ = nack_count_.load() + 1;
    }

    sender_.UpdateLossListFromNackDgr(nack_dgr);
    congestion_control_.OnLoss(nack_dgr, p_session->packet_seq_gen());
  }

  void OnAckOfAck(const AckOfAckDatagram& ack_of_ack_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    const auto& packet_seq_gen = p_session->packet_seq_gen();
    AckSequenceNumber ack_seq_num = ack_of_ack_dgr.header().additional_info();
    PacketSequenceNumber packet_seq_num(0);
    std::chrono::microseconds rtt(0);
    bool acked = receiver_.AckAck(ack_seq_num, &packet_seq_num, &rtt);

    if (acked) {
      if (Logger::ACTIVE) {
        ack2_count_ = ack2_count_.load() + 1;
      }

      if (packet_seq_gen.Compare(packet_seq_num,
                                 receiver_.largest_ack_number_acknowledged()) >
          0) {
        receiver_.set_largest_ack_number_acknowledged(packet_seq_num);
      }

      p_session->get_p_connection_info()->UpdateRTT(rtt.count());
      uint64_t rtt_var =
          std::abs(p_session->connection_info().rtt().count() - rtt.count());
      p_session->get_p_connection_info()->UpdateRTTVar(rtt_var);

      p_session->get_p_connection_info()->UpdateAckPeriod();
      p_session->get_p_connection_info()->UpdateNAckPeriod();
    }
  }

  void CloseConnection() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    congestion_control_.OnClose();
    auto self = this->shared_from_this();
    ShutdownDatagramPtr p_shutdown_dgr = std::make_shared<ShutdownDatagram>();
    auto shutdown_handler = [self, p_session, p_shutdown_dgr](
                                const boost::system::error_code&, std::size_t) {
      p_session->Unbind();
    };

    p_session->UpdateCacheConnection();

    p_session->AsyncSendControlPacket(
        *p_shutdown_dgr, ShutdownDatagram::Header::SHUTDOWN,
        ShutdownDatagram::Header::NO_ADDITIONAL_INFO, shutdown_handler);
  }

  PacketSequenceNumber GetPacketSequenceValue(
      PacketSequenceNumber seq_num) const {
    return seq_num & 0x7FFFFFFF;
  }

 private:
  std::weak_ptr<SocketSession> p_session_;
  Sender sender_;
  Receiver receiver_;
  bool unqueue_write_op_;
  CongestionControl congestion_control_;
  std::atomic<bool> stop_timers_;
  Timer ack_timer_;
  Timer nack_timer_;
  Timer exp_timer_;
  std::atomic<bool> closed_;
  std::atomic<uint32_t> nack_count_;
  std::atomic<uint32_t> ack_count_;
  std::atomic<uint32_t> ack_sent_count_;
  std::atomic<uint32_t> ack2_count_;
  std::atomic<uint32_t> ack2_sent_count_;
  std::atomic<uint32_t> received_count_;
  std::atomic<uint32_t> packet_received_since_light_ack_;
};

}  // namespace state
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_STATE_H_
