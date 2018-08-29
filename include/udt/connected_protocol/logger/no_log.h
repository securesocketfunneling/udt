#ifndef UDT_CONNECTED_PROTOCOL_LOGGER_NO_LOG_H_
#define UDT_CONNECTED_PROTOCOL_LOGGER_NO_LOG_H_

#include <cstdint>

#include <string>

#include "log_entry.h"

namespace connected_protocol {
namespace logger {

class NoLog {
 public:
  enum : bool { ACTIVE = false };

  enum : int { FREQUENCY = -1 };

 public:
  void Log(const LogEntry& log) {}
};

}  // logger
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_LOGGER_NO_LOG_H_
