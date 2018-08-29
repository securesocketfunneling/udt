#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_SENDER_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_SENDER_H_

#include <cstdint>

#include <map>
#include <queue>
#include <set>
#include <unordered_set>

#include <boost/asio/io_context.hpp>
#include <boost/asio/buffers_iterator.hpp>

#include <chrono>

#include <boost/thread/mutex.hpp>

#include "../../../common/error/error.h"
#include "../../io/write_op.h"
#include "../../../queue/async_queue.h"

namespace connected_protocol {
namespace state {
namespace connected {

template <class Protocol, class ConnectedState>
class Sender {
 private:
  using WriteOpsQueue =
      typename queue::basic_async_queue<io::basic_pending_write_operation *>;
  using PacketSequenceNumber = uint32_t;

 private:
  using Clock = typename Protocol::clock;
  using Timer = typename Protocol::timer;
  using TimePoint = typename Protocol::time_point;
  using CongestionControl = typename Protocol::congestion_control;
  using SocketSession = typename Protocol::socket_session;

 private:
  using SendDatagram = typename Protocol::SendDatagram;
  using SendDatagramPtr = std::unique_ptr<SendDatagram>;
  using NAckDatagram = typename Protocol::NAckDatagram;
  using NAckDatagramPtr = std::shared_ptr<NAckDatagram>;
  using LossPacketsSet = std::unordered_set<PacketSequenceNumber>;
  using NackPacketsMap = std::map<PacketSequenceNumber, SendDatagramPtr>;

 public:
  Sender(typename SocketSession::Ptr p_session)
      : p_session_(p_session),
        p_state_(nullptr),
        max_send_size_(8192),
        write_ops_mutex_(),
        write_ops_queue_(p_session->get_io_context()),
        unqueue_write_op_(false),
        loss_packets_mutex_(),
        loss_packets_(),
        nack_packets_mutex_(),
        nack_packets_(),
        last_ack_number_(0),
        sending_time_mutex_(),
        next_sending_packet_time_(0),
        packets_to_send_mutex_(),
        packets_to_send_() {}

  void Init(typename ConnectedState::Ptr p_state,
            CongestionControl *p_congestion_control) {
    p_congestion_control_ = p_congestion_control;
    p_state_ = p_state;
    StartUnqueueWriteOp();
  }

  void Stop() {
    StopUnqueueWriteOp();
    CloseWriteOpsQueue();
    p_state_.reset();
  }

  bool HasNackPackets() {
    boost::mutex::scoped_lock lock_nack_packets(nack_packets_mutex_);
    return !nack_packets_.empty();
  }

  void UpdateLossListFromNackDgr(const NAckDatagram &nack_dgr) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    auto nack_loss_list = nack_dgr.payload().GetLossPackets();
    std::size_t loss_list_size = nack_loss_list.size();

    {
      boost::mutex::scoped_lock lock_loss_packets(loss_packets_mutex_);

      for (uint32_t i = 0; i < loss_list_size; ++i) {
        PacketSequenceNumber current_seq = nack_loss_list[i];
        // first interval seq
        if (IsInterval(current_seq)) {
          PacketSequenceNumber first_range =
              GetPacketSequenceValue(current_seq);
          ++i;
          if (i < loss_list_size && !IsInterval(nack_loss_list[i])) {
            const auto &packet_seq_gen = p_session->packet_seq_gen();
            PacketSequenceNumber second_range =
                GetPacketSequenceValue(nack_loss_list[i]);
            PacketSequenceNumber j = first_range;
            while (j != second_range) {
              loss_packets_.insert(j);
              j = packet_seq_gen.Inc(j);
            }
          }
        } else {
          loss_packets_.insert(GetPacketSequenceValue(current_seq));
        }
      }

      if (loss_packets_.empty()) {
        return;
      }
    }

    p_session->AsyncSendPackets();
  }

  void UpdateLossListFromNackPackets() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    {
      boost::mutex::scoped_lock lock_nack_packets(nack_packets_mutex_);
      boost::mutex::scoped_lock lock_loss_packets(loss_packets_mutex_);

      if (nack_packets_.empty()) {
        return;
      }

      auto nack_packet_it = nack_packets_.begin();

      while (nack_packet_it != nack_packets_.end()) {
        const auto &seq_num = nack_packet_it->first;
        if (!nack_packet_it->second->is_acked()) {
          loss_packets_.insert(seq_num);
          ++nack_packet_it;
        } else {
          if (!nack_packet_it->second->is_pending_send()) {
            nack_packet_it = nack_packets_.erase(nack_packet_it);
          } else {
            ++nack_packet_it;
          }
        }
      }
    }

    p_session->AsyncSendPackets();
  }

  bool HasLossPackets() {
    boost::mutex::scoped_lock lock(loss_packets_mutex_);
    return !loss_packets_.empty();
  }

  bool HasPacketToSend() {
    boost::mutex::scoped_lock lock_packets_to_send(packets_to_send_mutex_);
    boost::mutex::scoped_lock lock(loss_packets_mutex_);
    return !packets_to_send_.empty() || !loss_packets_.empty();
  }

  std::chrono::nanoseconds NextScheduledPacketTime() {
    boost::mutex::scoped_lock lock_sending_time(sending_time_mutex_);
    return next_sending_packet_time_;
  }

  SendDatagram *NextScheduledPacket() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return nullptr;
    }

    TimePoint start_gen(Clock::now());

    {
      boost::mutex::scoped_lock lock_nack_packets(nack_packets_mutex_);
      boost::mutex::scoped_lock lock_loss_packets(loss_packets_mutex_);

      // Loss packet first
      while (!loss_packets_.empty()) {
        PacketSequenceNumber packet_loss_number = *(loss_packets_.begin());
        loss_packets_.erase(packet_loss_number);
        // nack_packets can be updated and the loss packet is not lost anymore
        // so check if it is in
        auto nack_packet_it = nack_packets_.find(packet_loss_number);
        if (nack_packet_it != nack_packets_.end()) {
          SendDatagram *p_datagram = nack_packet_it->second.get();
          if (!p_datagram->is_acked()) {
            p_datagram->set_pending_send(true);
            UpdateNextSendingPacketTime(p_datagram, start_gen);
            return p_datagram;
          } else {
            if (!p_datagram->is_pending_send()) {
              nack_packets_.erase(packet_loss_number);
            }
          }
        }
      }
    }

    SendDatagramPtr p_unique_datagram = nullptr;
    {
      boost::mutex::scoped_lock lock_nack_packets(nack_packets_mutex_);
      boost::mutex::scoped_lock lock_packets_to_send(packets_to_send_mutex_);

      PacketSequenceNumber seq_num = p_session->packet_seq_gen().current();
      if (!packets_to_send_.empty()) {
        // Too many datagram not acked, wait an ack to continue to send =>
        // congestion policy update value except pair packet
        if ((seq_num % 16 != 1) &&
            nack_packets_.size() >=
                std::min(p_congestion_control_->window_flow_size(),
                         p_session->window_flow_size())) {
          return nullptr;
        }

        p_unique_datagram = std::move(packets_to_send_.front());

        // Update datagram metadata
        p_unique_datagram->header().set_timestamp((uint32_t)(
            std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now() - p_session->start_timestamp())
                .count()));
        p_unique_datagram->header().set_packet_sequence_number(seq_num);
        p_unique_datagram->set_pending_send(true);
        p_congestion_control_->UpdateLastSendSeqNum(seq_num);
        p_session->get_p_packet_seq_gen()->Next();
        packets_to_send_.pop();
      }

      if (!p_unique_datagram.get()) {
        return nullptr;
      }

      UpdateNextSendingPacketTime(p_unique_datagram.get(), start_gen);

      // Save packet as not acked
      nack_packets_[seq_num] = std::move(p_unique_datagram);

      return nack_packets_[seq_num].get();
    }
  }

  void AckPackets(PacketSequenceNumber seq_number) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    seq_number = GetPacketSequenceValue(seq_number);
    const auto &packet_seq_gen = p_session->packet_seq_gen();

    {
      boost::mutex::scoped_lock lock_nack_packets(nack_packets_mutex_);
      boost::mutex::scoped_lock lock_loss_packets(loss_packets_mutex_);
      // remove packet whose seq_number < seq_number from sent_packets
      PacketSequenceNumber current_seq_num = packet_seq_gen.Dec(seq_number);
      auto p_nack_packet_it = nack_packets_.find(current_seq_num);

      while (p_nack_packet_it != nack_packets_.end()) {
        loss_packets_.erase(current_seq_num);
        current_seq_num = packet_seq_gen.Dec(current_seq_num);
        p_nack_packet_it->second->set_acked(true);
        if (!p_nack_packet_it->second->is_pending_send()) {
          nack_packets_.erase(p_nack_packet_it);
        }
        p_nack_packet_it = nack_packets_.find(current_seq_num);
      }
    }
  }

  void PushWriteOp(io::basic_pending_write_operation *write_op) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    boost::system::error_code ec;
    write_ops_queue_.push(write_op, ec);
    if (ec) {
      auto do_complete = [write_op, ec]() { write_op->complete(ec, 0); };
      p_session->get_io_context().post(do_complete);
    }
  }

 private:
  void UpdateNextSendingPacketTime(SendDatagram *p_datagram,
                                   const TimePoint &start_gen) {
    std::chrono::nanoseconds gen_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() -
                                                                 start_gen);
    if (p_datagram->header().packet_sequence_number() % 16 == 0 ||
        !loss_packets_.empty()) {
      // every 16n packet, send a new one immediatly to evaluate link capacity
      // resend immediatly if there is loss packets
      next_sending_packet_time_ = std::chrono::nanoseconds(0);
    } else {
      std::chrono::nanoseconds next_interval =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              p_congestion_control_->sending_period() - gen_time);
      if (next_interval.count() > 0) {
        boost::mutex::scoped_lock lock_sending_time(sending_time_mutex_);
        next_sending_packet_time_ = next_interval;
      } else {
        boost::mutex::scoped_lock lock_sending_time(sending_time_mutex_);
        next_sending_packet_time_ = std::chrono::nanoseconds(0);
      }
    }
  }

  void CloseWriteOpsQueue() {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    // Unqueue write ops queue and callback with error code
    io::basic_pending_write_operation *p_write_op;
    boost::system::error_code ec;
    for (;;) {
      p_write_op = write_ops_queue_.get(ec);
      if (ec) {
        break;
      }
      auto do_complete = [p_write_op]() {
        boost::system::error_code ec(::common::error::operation_canceled,
                                     ::common::error::get_error_category());
        p_write_op->complete(ec, 0);
      };
      p_session->get_io_context().dispatch(std::move(do_complete));
    }
    write_ops_queue_.close(ec);
  }

  void StartUnqueueWriteOp() {
    if (unqueue_write_op_) {
      return;
    }
    unqueue_write_op_ = true;
    UnqueueWriteOp();
  }

  void StopUnqueueWriteOp() { unqueue_write_op_ = false; }

  void UnqueueWriteOp() {
    write_ops_queue_.async_get(
        boost::bind(&Sender::ProcessWriteOp, this, _1, _2));
  }

  void ProcessWriteOp(const boost::system::error_code &ec,
                      io::basic_pending_write_operation *p_write_op) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return;
    }

    if (ec) {
      // TODO error processing
      unqueue_write_op_ = false;
      return;
    }

    std::size_t total_copy(ProcessWriteOpBuffers(p_write_op->const_buffers()));

    // Execute handler
    auto do_complete = [p_write_op, total_copy]() {
      p_write_op->complete(
          boost::system::error_code(::common::error::success,
                                    ::common::error::get_error_category()),
          total_copy);
    };
    p_session->get_io_context().post(do_complete);

    p_session->AsyncSendPackets();

    UnqueueWriteOp();
  }

  /// @return size of processed data
  std::size_t ProcessWriteOpBuffers(
      const io::fixed_const_buffer_sequence &write_buffers) {
    auto p_session = p_session_.lock();
    if (!p_session) {
      return 0;
    }

    std::size_t copy_length = 0;
    std::size_t packet_created = 0;
    std::size_t total_copy = 0;
    bool add_error = false;
    uint32_t message_seq_number = p_session->get_p_message_seq_gen()->Next();

    auto user_buf_current_it = boost::asio::buffers_begin(write_buffers);

    auto user_buf_end_it = boost::asio::buffers_end(write_buffers);

    // generate datagrams
    SendDatagram *p_current_datagram = nullptr;
    SendDatagram *p_previous_datagram = nullptr;
    while ((user_buf_current_it != user_buf_end_it) && !add_error) {
      SendDatagramPtr p_unique_current_datagram =
          std::unique_ptr<SendDatagram>(new SendDatagram());
      copy_length = 0;
      p_current_datagram = p_unique_current_datagram.get();
      auto &header = p_current_datagram->header();
      auto &payload = p_current_datagram->payload();
      payload.SetSize(p_session->connection_info().packet_data_size() -
                      SendDatagram::Header::size);
      auto payload_buf = payload.GetMutableBuffers();
      auto current_payload_it = boost::asio::buffers_begin(payload_buf);
      auto end_payload_it = boost::asio::buffers_end(payload_buf);

      // Copy user buffer in payload buf
      while ((user_buf_current_it != user_buf_end_it) &&
             (current_payload_it != end_payload_it)) {
        *current_payload_it = *user_buf_current_it;

        ++copy_length;
        ++current_payload_it;
        ++user_buf_current_it;
      }
      payload.SetSize(static_cast<uint32_t>(copy_length));
      // complete packet header
      header.set_message_number(message_seq_number);
      header.set_destination_socket(p_session->remote_socket_id());

      add_error = !(AddPacket(std::move(p_unique_current_datagram)));

      if ((add_error && packet_created == 0)) {
        return 0;
      }

      if (add_error) {
        if (packet_created == 1) {
          p_previous_datagram->header().set_message_position(
              SendDatagram::Header::ONLY_ONE_PACKET);
        } else {
          p_previous_datagram->header().set_message_position(
              SendDatagram::Header::LAST);
        }
      } else {
        total_copy += copy_length;
        if (user_buf_current_it != user_buf_end_it) {
          // no more data to copy
          if (packet_created == 1) {
            header.set_message_position(SendDatagram::Header::ONLY_ONE_PACKET);
          } else {
            header.set_message_position(SendDatagram::Header::LAST);
          }
        } else {
          header.set_message_position(SendDatagram::Header::MIDDLE);
        }
      }

      packet_created++;
      p_previous_datagram = p_current_datagram;
    }

    return total_copy;
  }

  boost::asio::const_buffer SubBuffer(const boost::asio::const_buffer &buffer,
                                      std::size_t end_offset) {
    const uint8_t *buffer_data =
        boost::asio::buffer_cast<const uint8_t *>(buffer);
    return boost::asio::buffer(buffer_data, end_offset);
  }

  bool AddPacket(SendDatagramPtr p_unique_datagram) {
    boost::mutex::scoped_lock lock_packets_to_send(packets_to_send_mutex_);
    if (packets_to_send_.size() > max_send_size_) {
      return false;
    }

    packets_to_send_.push(std::move(p_unique_datagram));
    return true;
  }

  bool IsInterval(PacketSequenceNumber seq_num) const {
    return 0 != (seq_num & 0x80000000);
  }

  PacketSequenceNumber GetPacketSequenceValue(
      PacketSequenceNumber seq_num) const {
    return seq_num & 0x7FFFFFFF;
  }

 private:
  std::weak_ptr<SocketSession> p_session_;
  typename ConnectedState::Ptr p_state_;
  uint32_t max_send_size_;
  boost::mutex write_ops_mutex_;
  WriteOpsQueue write_ops_queue_;
  bool unqueue_write_op_;

  // packets loss set, sorted by seq_number increased order
  boost::mutex loss_packets_mutex_;
  std::set<PacketSequenceNumber> loss_packets_;

  // packets not ack
  boost::mutex nack_packets_mutex_;
  NackPacketsMap nack_packets_;
  std::atomic<PacketSequenceNumber> last_ack_number_;

  // timepoint of the next sending packet
  boost::mutex sending_time_mutex_;
  std::chrono::nanoseconds next_sending_packet_time_;

  boost::mutex packets_to_send_mutex_;
  std::queue<SendDatagramPtr> packets_to_send_;

  CongestionControl *p_congestion_control_;
};

}  // connected
}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_SENDER_H_
