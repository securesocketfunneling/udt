#ifndef UDT_CONNECTED_PROTOCOL_LOGGER_FILE_LOG_H_
#define UDT_CONNECTED_PROTOCOL_LOGGER_FILE_LOG_H_

#include <cstdint>

#include <fstream>
#include <sstream>
#include <string>

#include "log_entry.h"

namespace connected_protocol {
namespace logger {

template <int FrequencyValue = 1000>
class FileLog {
 public:
  FileLog() : file_() {
    std::stringstream ss;
    ss << "session_log" << this << ".log";
    file_.open(ss.str());
  }

  enum : bool { ACTIVE = true };

  enum : int { FREQUENCY = FrequencyValue };

 public:
  void Log(const LogEntry& log) {
    if (file_.is_open()) {
      std::stringstream log_text_stream;
      log_text_stream << log.sending_period << " ";
      log_text_stream << log.cc_window_flow_size << " ";
      log_text_stream << log.remote_arrival_speed << " ";
      log_text_stream << log.remote_estimated_link_capacity << " ";
      log_text_stream << log.rtt << " ";
      log_text_stream << log.rtt_var << " ";
      log_text_stream << log.ack_period << " ";
      log_text_stream << log.nack_count << " ";
      log_text_stream << log.ack_count << " ";
      log_text_stream << log.ack_sent_count << " ";
      log_text_stream << log.ack2_count << " ";
      log_text_stream << log.ack2_sent_count << " ";
      log_text_stream << log.multiplexer_sent_count << " ";
      log_text_stream << log.flow_sent_count << " ";
      log_text_stream << log.received_count << " ";
      log_text_stream << log.local_arrival_speed << " ";
      log_text_stream << log.local_estimated_link_capacity << " ";
      log_text_stream << log.remote_window_flow_size << std::endl;
      std::string log_text(log_text_stream.str());
      file_.write(log_text.c_str(), log_text.size());
      file_.flush();
    }
  }

 private:
  std::ofstream file_;
};

}  // namespace logger
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_LOGGER_FILE_LOG_H_
