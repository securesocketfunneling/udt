#ifndef UDT_CONNECTED_PROTOCOL_CONGESTION_CONGESTION_CONTROL_H_
#define UDT_CONNECTED_PROTOCOL_CONGESTION_CONGESTION_CONTROL_H_

#include <memory>
#include <chrono>

#include "../sequence_generator.h"
#include "../cache/connection_info.h"

namespace connected_protocol {
namespace congestion {

template <class Protocol>
class CongestionControl {
 private:
  using Clock = typename Protocol::clock;
  using TimePoint = typename Protocol::time_point;

 public:
  using ConnectionInfo = connected_protocol::cache::ConnectionInfo;
  using PacketSequenceNumber = uint32_t;
  using SocketSession = typename Protocol::socket_session;
  using SendDatagram = typename Protocol::SendDatagram;
  using DataDatagram = typename Protocol::DataDatagram;
  using AckDatagram = typename Protocol::AckDatagram;
  using NAckDatagram = typename Protocol::NAckDatagram;

  CongestionControl(ConnectionInfo *p_connection_info)
      : p_connection_info_(p_connection_info),
        window_flow_size_(16),
        max_window_size_(0),
        sending_period_(0.0),
        slow_start_phase_(true),
        loss_phase_(false),
        last_ack_number_(0),
        avg_nack_num_(1),
        nack_count_(1),
        dec_count_(1),
        last_dec_sending_period_(1.0),
        last_send_seq_num_(0),
        dec_random_(1),
        last_update_(Clock::now()) {}

  void Init(PacketSequenceNumber init_packet_seq_num,
            uint32_t max_window_size) {
    last_dec_seq_num_ = init_packet_seq_num - 1;
    last_ack_number_ = init_packet_seq_num - 1;
    max_window_size_ = max_window_size;
    last_update_ = Clock::now();
  }

  void OnPacketSent(const SendDatagram &datagram) {}

  void OnAck(const AckDatagram &ack_dgr,
             const SequenceGenerator &packet_seq_gen) {
    double syn_interval =
        static_cast<double>(p_connection_info_->syn_interval());
    double rtt = static_cast<double>(p_connection_info_->rtt().count());
    double packet_data_size =
        static_cast<double>(p_connection_info_->packet_data_size());
    double estimated_link_capacity =
        p_connection_info_->estimated_link_capacity();
    double packet_arrival_speed = p_connection_info_->packet_arrival_speed();
    PacketSequenceNumber ack_number(
        GetSequenceNumber(ack_dgr.payload().max_packet_sequence_number()));

    TimePoint current_time = Clock::now();

    if (std::chrono::duration_cast<std::chrono::microseconds>(current_time -
                                                                  last_update_)
            .count() < p_connection_info_->syn_interval()) {
      return;
    }

    last_update_ = current_time;

    if (slow_start_phase_.load()) {
      int32_t diff_length =
          packet_seq_gen.SeqLength(last_ack_number_.load(), ack_number);
      window_flow_size_ =
          window_flow_size_.load() + (static_cast<double>(diff_length));
      last_ack_number_ = ack_number;
      if (window_flow_size_.load() > max_window_size_) {
        slow_start_phase_ = false;
        if (packet_arrival_speed > 0) {
          sending_period_ = (1000000.0 / packet_arrival_speed);
        } else {
          sending_period_ = ((rtt + syn_interval) / window_flow_size_);
        }
      }
    } else {
      UpdateWindowFlowSize();
    }

    if (slow_start_phase_.load()) {
      p_connection_info_->set_window_flow_size(window_flow_size_.load());
      p_connection_info_->set_sending_period(sending_period_.load());
      return;
    }

    if (loss_phase_.load()) {
      loss_phase_ = false;
      return;
    }

    double min_inc = 0.01;
    double inc(0.0);

    double B = estimated_link_capacity - (1000000.0 / (sending_period_.load()));
    if ((sending_period_.load() > last_dec_sending_period_.load()) &&
        ((estimated_link_capacity / 9) < B)) {
      B = estimated_link_capacity / 9;
    }
    if (B <= 0) {
      inc = min_inc;
    } else {
      inc = pow(10.0, ceil(log10(B * packet_data_size * 8.0))) * 0.0000015 /
            packet_data_size;
      if (inc < min_inc) {
        inc = min_inc;
      }
    }
    sending_period_ = (sending_period_.load() * syn_interval) /
                      (sending_period_.load() * inc + syn_interval);

    p_connection_info_->set_sending_period(sending_period_.load());
  }

  void OnLoss(const NAckDatagram &nack_dgr,
              const connected_protocol::SequenceGenerator &seq_gen) {
    std::vector<uint32_t> loss_list(nack_dgr.payload().GetLossPackets());
    PacketSequenceNumber first_loss_list_seq = GetSequenceNumber(loss_list[0]);

    double syn_interval =
        static_cast<double>(p_connection_info_->syn_interval());
    double rtt = static_cast<double>(p_connection_info_->rtt().count());
    double packet_arrival_speed = p_connection_info_->packet_arrival_speed();

    if (slow_start_phase_.load()) {
      slow_start_phase_ = false;

      if (packet_arrival_speed > 0) {
        sending_period_ = (1000000.0 / packet_arrival_speed);
        return;
      }
      sending_period_ = (window_flow_size_.load() / (rtt + syn_interval));
    }

    loss_phase_ = true;

    if (seq_gen.Compare(first_loss_list_seq, last_dec_seq_num_.load()) > 0) {
      last_dec_sending_period_ = sending_period_.load();
      sending_period_ = sending_period_.load() * 1.125;

      avg_nack_num_ = static_cast<uint32_t>(
          ceil(avg_nack_num_.load() * 0.875 + nack_count_.load() * 0.125));
      nack_count_ = 1;
      dec_count_ = 1;
      last_dec_seq_num_ = last_send_seq_num_.load();
      srand(last_dec_seq_num_.load());
      dec_random_ = static_cast<uint32_t>(ceil(
          avg_nack_num_.load() * (static_cast<double>(rand()) / RAND_MAX)));
      if (dec_random_ < 1) {
        dec_random_ = 1;
      }
    } else {
      nack_count_ = nack_count_.load() + 1;
      if (dec_count_.load() < 5 &&
          0 == (nack_count_.load() % dec_random_.load())) {
        sending_period_ = sending_period_.load() * 1.125;
        last_dec_seq_num_ = last_send_seq_num_.load();
      }
      dec_count_ = dec_count_.load() + 1;
    }
  }

  void OnPacketReceived(const DataDatagram &datagram) {}

  void OnTimeout() {}

  void OnClose() {}

  void UpdateLastSendSeqNum(PacketSequenceNumber last_send_seq_num) {
    last_send_seq_num_ = last_send_seq_num;
  }

  void UpdateWindowFlowSize() {
    double syn_interval =
        static_cast<double>(p_connection_info_->syn_interval());
    double rtt = static_cast<double>(p_connection_info_->rtt().count());
    double packet_arrival_speed = p_connection_info_->packet_arrival_speed();

    window_flow_size_ =
        (packet_arrival_speed / 1000000.0) * (rtt + syn_interval) + 16;

    p_connection_info_->set_window_flow_size(window_flow_size_.load());
  }

  std::chrono::nanoseconds sending_period() const {
    return std::chrono::nanoseconds(
        static_cast<long long>(ceil(sending_period_.load() * 1000)));
  }

  uint32_t window_flow_size() const {
    return static_cast<uint32_t>(ceil(window_flow_size_.load()));
  }

 private:
  PacketSequenceNumber GetSequenceNumber(PacketSequenceNumber seq_num) {
    return seq_num & 0x7FFFFFFF;
  }

 private:
  ConnectionInfo *p_connection_info_;
  std::atomic<double> window_flow_size_;
  uint32_t max_window_size_;
  // in nanosec
  std::atomic<double> sending_period_;
  std::atomic<bool> slow_start_phase_;
  std::atomic<bool> loss_phase_;
  std::atomic<PacketSequenceNumber> last_ack_number_;
  std::atomic<uint32_t> avg_nack_num_;
  std::atomic<uint32_t> nack_count_;
  std::atomic<uint32_t> dec_count_;
  std::atomic<PacketSequenceNumber> last_dec_seq_num_;
  // in nanosec
  std::atomic<double> last_dec_sending_period_;
  std::atomic<PacketSequenceNumber> last_send_seq_num_;
  std::atomic<uint32_t> dec_random_;
  TimePoint last_update_;
};

}  // congestion
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_CONGESTION_CONGESTION_CONTROL_H_
