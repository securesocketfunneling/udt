#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_ACK_HISTORY_WINDOW_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_ACK_HISTORY_WINDOW_H_

#include <cstdint>

#include <algorithm>
#include <map>
#include <numeric>
#include <queue>

#include <boost/asio/io_context.hpp>
#include <boost/asio/high_resolution_timer.hpp>

#include <chrono>

#include <boost/thread/mutex.hpp>

namespace connected_protocol {
namespace state {
namespace connected {
class AckHistoryWindow {
 public:
  using PacketSequenceNumber = uint32_t;
  using AckSequenceNumber = uint32_t;
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

 public:
  AckHistoryWindow(uint32_t size = 1024)
      : mutex_(),
        current_index_(0),
        oldest_index_(0),
        packet_sequence_numbers_(size),
        ack_sequence_numbers_(size),
        ack_timestamps_(size) {}

  void StoreAck(AckSequenceNumber ack_num, PacketSequenceNumber packet_num) {
    boost::mutex::scoped_lock lock(mutex_);
    uint32_t window_size =
        static_cast<uint32_t>(packet_sequence_numbers_.size());
    ack_sequence_numbers_[current_index_] = ack_num;
    packet_sequence_numbers_[current_index_] = packet_num;
    ack_timestamps_[current_index_] = Clock::now();
    current_index_ = (current_index_ + 1) % window_size;
    if (current_index_ == oldest_index_) {
      oldest_index_ = (oldest_index_ + 1) % window_size;
    }
  }

  bool Acknowledge(AckSequenceNumber ack_seq_num,
                   PacketSequenceNumber* p_packet_seq_num,
                   std::chrono::microseconds* p_rtt) {
    boost::mutex::scoped_lock lock(mutex_);
    uint32_t window_size =
        static_cast<uint32_t>(packet_sequence_numbers_.size());
    if (current_index_ >= oldest_index_) {
      for (uint32_t i = oldest_index_, newest_index = current_index_;
           i < newest_index; ++i) {
        if (ack_sequence_numbers_[i] == ack_seq_num) {
          *p_packet_seq_num = packet_sequence_numbers_[i];
          *p_rtt = std::chrono::duration_cast<std::chrono::microseconds>(
              Clock::now() - ack_timestamps_[i]);

          // Update last ack seq number ever ever being acknowledge
          if (i + 1 == current_index_) {
            oldest_index_ = current_index_ = 0;
            packet_sequence_numbers_[current_index_] = 0;
          } else {
            oldest_index_ = (i + 1) % window_size;
          }

          return true;
        }
      }
      // Ack seq number overwritten
      return false;
    } else {
      for (uint32_t i = oldest_index_, n = current_index_ + window_size; i < n;
           ++i) {
        if (ack_sequence_numbers_[i % window_size] == ack_seq_num) {
          i %= window_size;
          *p_packet_seq_num = packet_sequence_numbers_[i];
          *p_rtt = std::chrono::duration_cast<std::chrono::microseconds>(
              Clock::now() - ack_timestamps_[i]);

          // Update last ack seq number ever ever being acknowledge
          if (i == current_index_) {
            oldest_index_ = current_index_ = 0;
            packet_sequence_numbers_[current_index_] = 0;
          } else {
            oldest_index_ = (i + 1) % window_size;
          }

          return true;
        }
      }
      // Ack seq number overwritten
      return false;
    }
  }

 private:
  boost::mutex mutex_;
  uint32_t current_index_;
  uint32_t oldest_index_;
  std::vector<PacketSequenceNumber> packet_sequence_numbers_;
  std::vector<AckSequenceNumber> ack_sequence_numbers_;
  std::vector<TimePoint> ack_timestamps_;
};

}  // connected
}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_ACK_HISTORY_WINDOW_H_
