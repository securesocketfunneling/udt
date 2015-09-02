#ifndef UDT_CONNECTED_PROTOCOL_LOGGER_LOG_ENTRY_H_
#define UDT_CONNECTED_PROTOCOL_LOGGER_LOG_ENTRY_H_

#include <cstdint>

namespace connected_protocol {
namespace logger {

struct LogEntry {
  // local data
  double sending_period;
  double cc_window_flow_size;
  double local_arrival_speed;
  double local_estimated_link_capacity;
  long long ack_period;
  uint32_t nack_count;
  uint32_t ack_count;
  uint32_t ack_sent_count;
  uint32_t ack2_count;
  uint32_t ack2_sent_count;
  uint32_t multiplexer_sent_count;
  uint32_t flow_sent_count;
  uint32_t received_count;
  uint32_t packets_to_send_count;
  // remote data
  uint32_t remote_window_flow_size;
  double remote_arrival_speed;
  double remote_estimated_link_capacity;
  long long rtt;
  long long rtt_var;
};

}  // logger
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_LOGGER_LOG_ENTRY_H_
