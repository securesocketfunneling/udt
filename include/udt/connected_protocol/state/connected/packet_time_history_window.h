#ifndef UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_PACKET_TIME_HISTORY_WINDOW_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_PACKET_TIME_HISTORY_WINDOW_H_

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <numeric>

#include <boost/circular_buffer.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

namespace connected_protocol {
namespace state {
namespace connected {

class PacketTimeHistoryWindow {
 private:
  using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
  using MicrosecUnit = int_least64_t;
  using CircularBuffer = boost::circular_buffer<MicrosecUnit>;

 public:
  PacketTimeHistoryWindow(uint32_t max_arrival_size = 16,
                          uint32_t max_probe_size = 64)
      : arrival_mutex_(),
        max_packet_arrival_speed_size_(max_arrival_size),
        arrival_interval_history_(max_arrival_size),
        last_arrival_(std::chrono::high_resolution_clock::now()),
        probe_mutex_(),
        max_probe_interval_size_(max_probe_size),
        probe_interval_history_(max_probe_size),
        first_probe_arrival_(std::chrono::high_resolution_clock::now()) {}

  void Init(double packet_arrival_speed = 1000000.0,
            double estimated_capacity = 1000.0) {
    MicrosecUnit packet_interval(
        static_cast<MicrosecUnit>(ceil(1000000 / packet_arrival_speed)));
    for (uint32_t i = 0; i < max_packet_arrival_speed_size_; ++i) {
      arrival_interval_history_.push_back(packet_interval);
    }
    MicrosecUnit probe_interval(
        static_cast<MicrosecUnit>(ceil(1000000 / estimated_capacity)));
    for (uint32_t i = 0; i < max_probe_interval_size_; ++i) {
      probe_interval_history_.push_back(probe_interval);
    }
  }

  void OnArrival() {
    boost::lock_guard<boost::mutex> lock_arrival(arrival_mutex_);
    TimePoint arrival_time(
        std::chrono::high_resolution_clock::now());
    MicrosecUnit delta(DeltaTime(arrival_time, last_arrival_));
    arrival_interval_history_.push_back(delta);
    last_arrival_ = arrival_time;
  }

  void OnFirstProbe() {
    boost::lock_guard<boost::mutex> lock_probe(probe_mutex_);
    first_probe_arrival_ = std::chrono::high_resolution_clock::now();
  }

  void OnSecondProbe() {
    boost::lock_guard<boost::mutex> lock_probe(probe_mutex_);
    TimePoint arrival_time(
        std::chrono::high_resolution_clock::now());

    probe_interval_history_.push_back(
        DeltaTime(arrival_time, first_probe_arrival_));
  }

  /// @return packets per second
  double GetPacketArrivalSpeed() {
    boost::lock_guard<boost::mutex> lock_arrival(arrival_mutex_);
    // copy values
    std::vector<MicrosecUnit> sorted_values(arrival_interval_history_.begin(),
                                            arrival_interval_history_.end());

    std::sort(sorted_values.begin(), sorted_values.end());
    MicrosecUnit median(sorted_values[sorted_values.size() / 2]);
    MicrosecUnit low_value(median / 8);
    MicrosecUnit upper_value(median * 8);
    auto it = std::copy_if(
        sorted_values.begin(), sorted_values.end(), sorted_values.begin(),
        [low_value, upper_value](MicrosecUnit current_value) {
          return current_value > low_value && current_value < upper_value;
        });
    // todo change resize
    sorted_values.resize(std::distance(sorted_values.begin(), it));

    if (sorted_values.size() > 8) {
      double sum(
          std::accumulate(sorted_values.begin(), sorted_values.end(), 0.0));
      return (1000000.0 * sorted_values.size()) / sum;
    } else {
      return 0;
    }
  }

  /// @return packets per second
  double GetEstimatedLinkCapacity() {
    boost::lock_guard<boost::mutex> lock_probe(probe_mutex_);
    // copy values
    std::vector<MicrosecUnit> sorted_values(probe_interval_history_.begin(),
                                            probe_interval_history_.end());

    std::sort(sorted_values.begin(), sorted_values.end());
    MicrosecUnit median(sorted_values[sorted_values.size() / 2]);
    MicrosecUnit low_value(median / 8);
    MicrosecUnit upper_value(median * 8);
    auto it = std::copy_if(
        sorted_values.begin(), sorted_values.end(), sorted_values.begin(),
        [low_value, upper_value](MicrosecUnit current_value) {
          return current_value > low_value && current_value < upper_value;
        });
    // todo change resize
    sorted_values.resize(std::distance(sorted_values.begin(), it));

    double sum(
        std::accumulate(sorted_values.begin(), sorted_values.end(), 0.0));

    if (sum == 0) {
      return 0;
    }

    return (1000000.0 * sorted_values.size()) / sum;
  }

 private:
  MicrosecUnit DeltaTime(const TimePoint &t1, const TimePoint &t2) {
    return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t2)
        .count();
  }

 private:
  // delta in micro seconds
  boost::mutex arrival_mutex_;
  uint32_t max_packet_arrival_speed_size_;
  CircularBuffer arrival_interval_history_;
  TimePoint last_arrival_;
  // delta in micro seconds
  boost::mutex probe_mutex_;
  uint32_t max_probe_interval_size_;
  CircularBuffer probe_interval_history_;
  TimePoint first_probe_arrival_;
};

}  // connected
}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CONNECTED_PACKET_TIME_HISTORY_WINDOW_H_
