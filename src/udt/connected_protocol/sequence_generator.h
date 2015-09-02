#ifndef UDT_CONNECTED_PROTOCOL_SEQUENCE_GENERATOR_H_
#define UDT_CONNECTED_PROTOCOL_SEQUENCE_GENERATOR_H_

#include <cstdint>
#include <cstdlib>

#include <chrono>

#include <boost/chrono.hpp>
#include <boost/log/trivial.hpp>

#include <boost/thread/mutex.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

namespace connected_protocol {

class SequenceGenerator {
 public:
  typedef uint32_t SeqNumber;

 public:
  SequenceGenerator(SeqNumber max_value)
      : mutex_(), current_(0), max_value_(max_value) {
    boost::random::mt19937 gen(static_cast<SeqNumber>(
        boost::chrono::duration_cast<boost::chrono::nanoseconds>(
            boost::chrono::high_resolution_clock::now().time_since_epoch())
            .count()));
    threshold_compare_ = max_value_ >> 1;
    boost::random::uniform_int_distribution<uint32_t> dist(0, max_value_);
    current_ = dist(gen);
  }

  uint32_t Previous() {
    boost::mutex::scoped_lock lock(mutex_);
    current_ = Dec(current_);
    return current_;
  }

  uint32_t Next() {
    boost::mutex::scoped_lock lock(mutex_);
    current_ = Inc(current_);
    return current_;
  }

  void set_current(uint32_t current) {
    boost::mutex::scoped_lock lock(mutex_);
    if (current > max_value_) {
      current_ = 0;
    } else {
      current_ = current;
    }
  }

  uint32_t current() {
    boost::mutex::scoped_lock lock(mutex_);
    return current_;
  }

  /// @return positive if lhs > rhs, negative if lhs < rhs
  int Compare(SeqNumber lhs, SeqNumber rhs) const {
    return ((uint32_t)abs((int)(lhs - rhs)) < threshold_compare_) ? (lhs - rhs)
                                                                  : (rhs - lhs);
  }

  uint32_t Inc(SeqNumber seq_num) const {
    if (seq_num == max_value_) {
      return 0;
    } else {
      return seq_num + 1;
    }
  }

  uint32_t Dec(SeqNumber seq_num) const {
    if (seq_num == 0) {
      return max_value_;
    } else {
      return seq_num - 1;
    }
  }

  int32_t SeqLength(int32_t first, int32_t last) const {
    return (first <= last) ? (last - first + 1)
                           : (last - first + max_value_ + 2);
  }

  int32_t SeqOffset(int32_t first, int32_t last) const {
    if (abs(first - last) < (int32_t)threshold_compare_) {
      return last - first;
    }

    if (first < last) {
      return last - first - (int32_t)max_value_ - 1;
    }

    return last - first + (int32_t)max_value_ + 1;
  }

 private:
  boost::mutex mutex_;
  SeqNumber current_;
  SeqNumber max_value_;
  SeqNumber threshold_compare_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_SEQUENCE_GENERATOR_H_
