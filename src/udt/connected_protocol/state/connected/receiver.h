#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_RECEIVER_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_RECEIVER_H_

#include <cstdint>

#include <atomic>
#include <map>
#include <queue>
#include <set>

#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_waitable_timer.hpp>

#include <boost/chrono.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread/mutex.hpp>

#include "udt/common/error/error.h"
#include "udt/connected_protocol/io/buffers.h"
#include "udt/connected_protocol/io/read_op.h"
#include "udt/connected_protocol/sequence_generator.h"
#include "udt/connected_protocol/state/connected/ack_history_window.h"
#include "udt/connected_protocol/state/connected/packet_time_history_window.h"

namespace connected_protocol {
namespace state {
namespace connected {

template <class Protocol>
class Receiver {
 private:
  typedef typename Protocol::clock Clock;
  typedef typename Protocol::timer Timer;
  typedef typename Protocol::time_point TimePoint;

  typedef std::queue<io::basic_pending_stream_read_operation<Protocol> *>
      ReadOpsQueue;
  typedef uint32_t packet_sequence_number_type;
  typedef uint32_t ack_sequence_number_type;
  typedef typename Protocol::socket_session SocketSession;
  typedef typename Protocol::DataDatagram DataDatagram;
  typedef std::shared_ptr<DataDatagram> DataDatagramPtr;
  typedef typename Protocol::AckDatagram AckDatagram;
  typedef std::shared_ptr<AckDatagram> AckDatagramPtr;
  typedef typename Protocol::NAckDatagram NAckDatagram;
  typedef std::shared_ptr<NAckDatagram> NAckDatagramPtr;

  typedef std::map<packet_sequence_number_type, DataDatagram>
      ReceivedDatagramsMap;

 public:
  Receiver(boost::asio::io_service &io_service,
           typename SocketSession::Ptr p_session)
      : mutex_(),
        p_session_(std::move(p_session)),
        lrsn_(0),
        loss_list_(),
        read_ops_mutex_(),
        read_ops_queue_(),
        max_received_size_(8192),
        packets_received_mutex_(),
        packets_received_(),
        packet_history_window_(),
        ack_history_window_(),
        exp_count_(0) {}

  void Init(packet_sequence_number_type initial_packet_seq_num) {
    boost::mutex::scoped_lock lock(mutex_);
    last_exp_reset_timestamp_ = Clock::now();

    lrsn_ = initial_packet_seq_num - 1;
    largest_ack_number_acknowledged_ = initial_packet_seq_num;
    last_ack_number_ = initial_packet_seq_num;
    last_buffer_seq_ = initial_packet_seq_num - 1;
    last_ack_timestamp_ = Clock::now();
    last_ack2_timestamp_ = Clock::now();
    auto &connection_info = p_session_->connection_info;

    if (connection_info.packet_arrival_speed() > 0 &&
        connection_info.estimated_link_capacity() > 0) {
      packet_history_window_.Init(connection_info.packet_arrival_speed(),
                                  connection_info.estimated_link_capacity());
    } else {
      packet_history_window_.Init();
    }
  }

  void Stop() { CloseReadOpsQueue(); }

  void OnDataDatagram(DataDatagram *p_datagram) {
    boost::mutex::scoped_lock lock(mutex_);

    auto &packet_seq_gen = p_session_->packet_seq_gen;
    auto &header = p_datagram->header();
    packet_sequence_number_type packet_seq_num =
        header.packet_sequence_number();

    // Save packet arrival time in receiver history window
    packet_history_window_.OnArrival();

    // Register first packet probe arrival
    if (packet_seq_num % 16 == 0) {
      packet_history_window_.OnFirstProbe();
    }

    // Register second packet probe arrival
    if (packet_seq_num % 16 == 1) {
      packet_history_window_.OnSecondProbe();
    }

    {
      boost::mutex::scoped_lock lock_packets_received(packets_received_mutex_);
      if (!packets_received_.empty()) {
        auto &begin_pair = *(packets_received_.begin());
        auto first_seq_num_received_buffer = begin_pair.first;
        if (packet_seq_gen.SeqLength(first_seq_num_received_buffer,
                                     packet_seq_num) >
            (int32_t)max_received_size_) {
          // drop -> no more buffer space available
          return;
        }
      }
      if (last_buffer_seq_ >= packet_seq_num ||
          packets_received_.count(packet_seq_num)) {
        // packet already processed
        return;
      }
    }

    if (packet_seq_gen.Compare(packet_seq_num,
                               packet_seq_gen.Inc(lrsn_.load())) > 0) {
      uint32_t i = packet_seq_gen.Inc(lrsn_.load());
      while (i != packet_seq_num) {
        loss_list_.insert(i);
        i = packet_seq_gen.Inc(i);
      }

      NAckDatagramPtr p_nack_dgr = std::make_shared<NAckDatagram>();
      if (packet_seq_gen.Inc(lrsn_.load()) !=
          packet_seq_gen.Dec(packet_seq_num)) {
        p_nack_dgr->payload().AddLossRange(packet_seq_gen.Inc(lrsn_.load()),
                                           packet_seq_gen.Dec(packet_seq_num));
      } else {
        p_nack_dgr->payload().AddLossPacket(packet_seq_gen.Inc(lrsn_.load()));
      }
      auto p_session = p_session_;
      // send nack datagram
      p_session_->AsyncSendControlPacket(
          *p_nack_dgr, NAckDatagram::Header::NACK,
          NAckDatagram::Header::NO_ADDITIONAL_INFO,
          [p_session, p_nack_dgr](const boost::system::error_code &,
                                  std::size_t) {});
    } else if (packet_seq_gen.Compare(packet_seq_num, lrsn_.load()) < 0) {
      loss_list_.erase(packet_seq_num);
    }

    if (packet_seq_gen.Compare(packet_seq_num, lrsn_.load()) > 0) {
      lrsn_ = packet_seq_num;
    }

    {
      boost::mutex::scoped_lock lock_packets_received(packets_received_mutex_);
      packets_received_[packet_seq_num] = std::move(*p_datagram);
    }

    p_session_->get_io_service().post(boost::bind(&Receiver::HandleQueues, this,
                                                  boost::system::error_code()));
  }

  void StoreAck(ack_sequence_number_type ack_seq_num,
                packet_sequence_number_type ack_number, bool light_ack) {
    ack_history_window_.StoreAck(ack_seq_num, ack_number);
    if (!light_ack) {
      last_ack_timestamp_ = Clock::now();
    }
  }

  bool AckAck(ack_sequence_number_type ack_seq_num,
              packet_sequence_number_type *p_packet_seq_num,
              boost::chrono::microseconds *p_rtt) {
    return ack_history_window_.Acknowledge(ack_seq_num, p_packet_seq_num,
                                           p_rtt);
  }

  // @return buffer size in bytes
  uint32_t AvailableReceiveBufferSize() {
    boost::mutex::scoped_lock lock(packets_received_mutex_);
    return max_received_size_ - packets_received_.size();
  }

  double GetPacketArrivalSpeed() {
    return packet_history_window_.GetPacketArrivalSpeed();
  }

  double GetEstimatedLinkCapacity() {
    return packet_history_window_.GetEstimatedLinkCapacity();
  }

  void IncExpCounter() { exp_count_ = exp_count_.load() + 1; }

  void ResetExpCounter() {
    boost::mutex::scoped_lock lock_exp(mutex_);
    exp_count_ = 1;
    last_exp_reset_timestamp_ = Clock::now();
  }

  uint64_t exp_count() { return exp_count_.load(); }

  bool HasTimeout() {
    boost::mutex::scoped_lock lock(mutex_);
    return exp_count_.load() > 16 &&
           boost::chrono::duration_cast<boost::chrono::seconds>(
               Clock::now() - last_exp_reset_timestamp_)
                   .count() > 10;
  }

  void PushReadOp(io::basic_pending_stream_read_operation<Protocol> *read_op) {
    {
      boost::mutex::scoped_lock lock_read_ops(read_ops_mutex_);
      read_ops_queue_.push(read_op);
    }
    p_session_->get_io_service().post(boost::bind(&Receiver::HandleQueues, this,
                                                  boost::system::error_code()));
  }

  packet_sequence_number_type AckNumber(
      const SequenceGenerator &packet_seq_gen) {
    boost::mutex::scoped_lock lock(mutex_);
    if (loss_list_.empty()) {
      return packet_seq_gen.Inc(lrsn_.load());
    } else {
      return *(loss_list_.begin());
    }
  }

  void set_largest_acknowledged_seq_number(
      packet_sequence_number_type largest_acknowledged_seq_number) {
    boost::mutex::scoped_lock lock(mutex_);
    largest_acknowledged_seq_number_ = largest_acknowledged_seq_number;
  }

  packet_sequence_number_type largest_acknowledged_seq_number() {
    boost::mutex::scoped_lock lock(mutex_);
    return largest_acknowledged_seq_number_;
  }

  void set_largest_ack_number_acknowledged(
      packet_sequence_number_type largest_ack_number_acknowledged) {
    boost::mutex::scoped_lock lock(mutex_);
    largest_ack_number_acknowledged_ = largest_ack_number_acknowledged;
  }

  packet_sequence_number_type largest_ack_number_acknowledged() {
    boost::mutex::scoped_lock lock(mutex_);
    return largest_ack_number_acknowledged_;
  }

  void set_last_ack2_seq_number(ack_sequence_number_type last_ack2_seq_number) {
    boost::mutex::scoped_lock lock(mutex_);
    last_ack2_seq_number_ = last_ack2_seq_number;
    last_ack2_timestamp_ = Clock::now();
  }

  void set_last_ack_number(packet_sequence_number_type last_ack_number) {
    boost::mutex::scoped_lock lock(mutex_);
    last_ack_number_ = last_ack_number;
  }

  packet_sequence_number_type last_ack_number() {
    boost::mutex::scoped_lock lock(mutex_);
    return last_ack_number_;
  }

  TimePoint last_ack_timestamp() {
    boost::mutex::scoped_lock lock(mutex_);
    return last_ack_timestamp_;
  }

 private:
  void HandleQueues(boost::system::error_code &ec) {
    boost::mutex::scoped_lock packet_received_lock(packets_received_mutex_);
    boost::mutex::scoped_lock read_ops_lock_(read_ops_mutex_);

    if (read_ops_queue_.empty() || packets_received_.empty()) {
      return;
    }

    if (ec) {
      CloseReadOpsQueue();
      return;
    }

    auto &packet_seq_gen = p_session_->packet_seq_gen;

    io::fixed_const_buffer_sequence packets_buffer;

    auto begin_it(packets_received_.begin());
    auto end_it(packets_received_.end());
    auto current_packet_it(begin_it);
    auto next_packet_it(begin_it);

    if (last_buffer_seq_ != packet_seq_gen.Dec(current_packet_it->first)) {
      // wait the next seq number
      return;
    }

    while (current_packet_it != end_it) {
      current_packet_it->second.payload().GetConstBuffers(&packets_buffer);
      ++next_packet_it;
      // end reached or gap in sequence number
      if (next_packet_it == end_it ||
          (current_packet_it->first !=
           packet_seq_gen.Dec(next_packet_it->first))) {
        break;
      }
      current_packet_it = next_packet_it;
    }

    io::basic_pending_stream_read_operation<Protocol> *read_op =
        read_ops_queue_.front();
    read_ops_queue_.pop();

    std::size_t copied(read_op->fill_buffer(packets_buffer));
    std::size_t offset(copied);
    std::size_t buffer_size(0);
    auto packet_it(begin_it);

    // clean packets_received set
    while (packet_it != end_it && offset > 0) {
      auto &payload = packet_it->second.payload();
      buffer_size = payload.GetSize();
      if (offset >= buffer_size) {
        // packet consumed entirely
        offset -= buffer_size;
        last_buffer_seq_ = packet_it->first;
        packet_it = packets_received_.erase(packet_it);
      } else {
        // partial consuming
        last_buffer_seq_ = packet_seq_gen.Dec(packet_it->first);
        payload.SetOffset(offset);
        offset = 0;
      }
    }

    auto do_complete =
        [read_op, ec, copied]() { read_op->complete(ec, copied); };

    p_session_->get_io_service().post(std::move(do_complete));
  }

  void CloseReadOpsQueue() {
    boost::mutex::scoped_lock lock_read_ops(read_ops_mutex_);
    // Unqueue read ops queue and callback with error code
    io::basic_pending_stream_read_operation<Protocol> *p_read_op;
    while (!read_ops_queue_.empty()) {
      p_read_op = read_ops_queue_.front();
      read_ops_queue_.pop();
      auto do_complete = [p_read_op]() {
        boost::system::error_code ec(::common::error::operation_canceled,
                                     ::common::error::get_error_category());
        p_read_op->complete(ec, 0);
      };
      p_session_->get_io_service().dispatch(std::move(do_complete));
    }
  }

 private:
  // mutex
  boost::mutex mutex_;
  // session
  typename SocketSession::Ptr p_session_;

  // packet largest received sequence number
  std::atomic<packet_sequence_number_type> lrsn_;

  // packets loss list, sorted by seq_number increased order
  std::set<packet_sequence_number_type> loss_list_;

  // Read ops queue
  boost::mutex read_ops_mutex_;
  ReadOpsQueue read_ops_queue_;

  // packets received
  uint32_t max_received_size_;
  boost::mutex packets_received_mutex_;
  ReceivedDatagramsMap packets_received_;
  packet_sequence_number_type last_buffer_seq_;

  // packet history window (arrival time of data packet)
  PacketTimeHistoryWindow packet_history_window_;

  // ack history window
  AckHistoryWindow ack_history_window_;

  // last exp counter reset
  TimePoint last_exp_reset_timestamp_;
  // consecutive expired timeout : timeout > 16
  std::atomic<uint64_t> exp_count_;

  packet_sequence_number_type largest_acknowledged_seq_number_;
  // largest ack number acknowledged by ACK2
  packet_sequence_number_type largest_ack_number_acknowledged_;
  // last ack2 sent back
  ack_sequence_number_type last_ack2_seq_number_;
  // last ack2 timestamp
  TimePoint last_ack2_timestamp_;
  // last ack number
  packet_sequence_number_type last_ack_number_;
  // last ack timestamp
  TimePoint last_ack_timestamp_;
};

}  // connected
}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_RECEIVER_H_
