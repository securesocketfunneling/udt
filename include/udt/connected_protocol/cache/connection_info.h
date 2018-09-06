#ifndef UDT_CONNECTED_PROTOCOL_CACHE_CONNECTION_INFO_H_
#define UDT_CONNECTED_PROTOCOL_CACHE_CONNECTION_INFO_H_

#include <cstdint>

#include <atomic>
#include <memory>

#include <chrono>

namespace connected_protocol {
namespace cache {

class ConnectionInfo {
 public:
  using Ptr = std::shared_ptr<ConnectionInfo>;

 private:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

 public:
  ConnectionInfo(long long syn_interval = 10000)
      : syn_interval_(syn_interval),
        packet_data_size_(1500),
        packet_arrival_speed_(0),
        estimated_link_capacity_(0),
        rtt_(10 * syn_interval),
        rtt_var_(5 * syn_interval),
        sending_period_(0.0),
        window_flow_size_(16),
        ack_period_(syn_interval),
        nack_period_(4 * syn_interval),
        exp_period_(500000),
        last_update_(Clock::now()) {}

  ConnectionInfo& operator=(const ConnectionInfo& other) {
    syn_interval_ = other.syn_interval_.load();
    packet_data_size_ = other.packet_data_size_.load();
    packet_arrival_speed_ = other.packet_arrival_speed_.load();
    estimated_link_capacity_ = other.estimated_link_capacity_.load();
    rtt_ = other.rtt_.load();
    rtt_var_ = other.rtt_var_.load();
    sending_period_ = other.sending_period_.load();
    window_flow_size_ = other.window_flow_size_.load();
    ack_period_ = other.ack_period_.load();
    nack_period_ = other.nack_period_.load();
    exp_period_ = other.exp_period_.load();
    last_update_ = Clock::now();

    return *this;
  }

  void Update(const ConnectionInfo& other) {
    syn_interval_ = other.syn_interval_.load();
    packet_data_size_ = other.packet_data_size_.load();
    packet_arrival_speed_ = other.packet_arrival_speed_.load();
    estimated_link_capacity_ = other.estimated_link_capacity_.load();
    rtt_ = other.rtt_.load();
    rtt_var_ = other.rtt_var_.load();
    sending_period_ = other.sending_period_.load();
    window_flow_size_ = other.window_flow_size_.load();
    ack_period_ = other.ack_period_.load();
    nack_period_ = other.nack_period_.load();
    exp_period_ = other.exp_period_.load();
    last_update_ = Clock::now();
  }

  long long syn_interval() const { return syn_interval_; }

  void set_packet_data_size(uint32_t packet_data_size) {
    packet_data_size_ = packet_data_size;
    UpdateTime();
  }

  uint32_t packet_data_size() const { return packet_data_size_.load(); }

  void UpdatePacketArrivalSpeed(double packet_arrival_speed_value) {
    packet_arrival_speed_ =
        (packet_arrival_speed_.load() * 7 + packet_arrival_speed_value) / 8.0;
    UpdateTime();
  }

  double packet_arrival_speed() const { return packet_arrival_speed_.load(); }

  void UpdateEstimatedLinkCapacity(double estimated_link_value) {
    estimated_link_capacity_ =
        (estimated_link_capacity_.load() * 7 + estimated_link_value) / 8.0;
    UpdateTime();
  }

  double estimated_link_capacity() const {
    return estimated_link_capacity_.load();
  }

  void set_rtt(uint64_t rtt) {
    rtt_ = rtt;
    UpdateTime();
  }

  void UpdateRTT(uint64_t rtt_value) {
    rtt_ = (rtt_.load() * 7 + rtt_value) / 8;
    UpdateTime();
  }

  std::chrono::microseconds rtt() const {
    return std::chrono::microseconds(rtt_.load());
  }

  void UpdateRTTVar(uint64_t rtt_var_value) {
    rtt_var_ = (rtt_var_.load() * 3 + rtt_var_value) / 4;
    UpdateTime();
  }

  std::chrono::microseconds rtt_var() const {
    return std::chrono::microseconds(rtt_var_.load());
  }

  std::chrono::microseconds ack_period() const {
    return std::chrono::microseconds(ack_period_.load());
  }

  void UpdateAckPeriod() {
    ack_period_ = (4 * rtt_.load() + rtt_var_.load() + syn_interval_.load());
  }

  std::chrono::microseconds nack_period() const {
    return std::chrono::microseconds(nack_period_.load());
  }

  void UpdateNAckPeriod() {
    nack_period_ = (4 * rtt_.load() + rtt_var_.load() + syn_interval_.load());
  }

  void UpdateExpPeriod(uint64_t continuous_timeout) {
    exp_period_ = continuous_timeout *
                  (4 * rtt_.load() + rtt_var_.load() + syn_interval_.load());
  }

  std::chrono::microseconds exp_period() const {
    return std::chrono::microseconds(exp_period_.load());
  }

  void set_sending_period(double sending_period) {
    sending_period_ = sending_period;
    UpdateTime();
  }

  double sending_period() const { return sending_period_.load(); }

  void set_window_flow_size(double window_flow_size) {
    window_flow_size_ = window_flow_size;
    UpdateTime();
  }

  double window_flow_size() const { return window_flow_size_.load(); }

  void UpdateTime() { last_update_ = Clock::now(); }

  bool operator<(const ConnectionInfo& rhs) {
    return last_update_ < rhs.last_update_;
  }

 private:
  // syn interval in microsec
  std::atomic<long long> syn_interval_;
  // packet size in bytes
  std::atomic<uint32_t> packet_data_size_;
  // packet per second
  std::atomic<double> packet_arrival_speed_;
  // packet per second
  std::atomic<double> estimated_link_capacity_;
  // round time trip in microsec
  std::atomic<uint64_t> rtt_;
  // in microsec
  std::atomic<uint64_t> rtt_var_;
  // time between each packet to send in nanosec
  std::atomic<double> sending_period_;
  // window flow size in packets
  std::atomic<double> window_flow_size_;
  // ack period in microsec
  std::atomic<uint64_t> ack_period_;
  // nack period in microsec
  std::atomic<uint64_t> nack_period_;
  // exp period in microsec
  std::atomic<uint64_t> exp_period_;
  // update time
  TimePoint last_update_;
};

}  // namespace cache
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_CACHE_CONNECTION_INFO_H_
