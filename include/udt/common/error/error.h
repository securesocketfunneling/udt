#ifndef UDT_COMMON_ERROR_ERROR_H_
#define UDT_COMMON_ERROR_ERROR_H_

#include <boost/system/error_code.hpp>
#include <boost/asio/error.hpp>
#include <string>

namespace common {
namespace error {

enum errors {
  address_in_use = boost::system::errc::address_in_use,
  address_not_available = boost::system::errc::address_not_available,
  bad_address = boost::system::errc::bad_address,
  broken_pipe = boost::system::errc::broken_pipe,
  buffer_is_full_error = 11000,
  connection_aborted = boost::system::errc::connection_aborted,
  device_or_resource_busy = boost::system::errc::device_or_resource_busy,
  function_not_supported = boost::system::errc::function_not_supported,
  identifier_removed = boost::system::errc::identifier_removed,
  interrupted = boost::system::errc::interrupted,
  io_error = boost::system::errc::io_error,
  no_link = boost::system::errc::no_link,
  not_connected = boost::system::errc::not_connected,
  operation_canceled = boost::system::errc::operation_canceled,
  success = boost::system::errc::success
};

namespace detail {
class error_category : public boost::system::error_category {
 public:
  const char* name() const BOOST_SYSTEM_NOEXCEPT { return "common error"; }

  std::string message(int value) const {
    switch (value) {
      case error::address_in_use:
        return "address in use";
      case error::address_not_available:
        return "address not available";
      case error::bad_address:
        return "bad address";
      case error::broken_pipe:
        return "broken pipe";
      case error::buffer_is_full_error:
        return "buffer is full";
      case error::connection_aborted:
        return "connection aborted";
      case error::device_or_resource_busy:
        return "device or resource busy";
      case error::function_not_supported:
        return "function not supported";
      case error::identifier_removed:
        return "identifier removed";
      case error::interrupted:
        return "connection interrupted";
      case error::io_error:
        return "io_error";
      case error::no_link:
        return "no link";
      case error::not_connected:
        return "not connected";
      case error::operation_canceled:
        return "operation canceled";
      case error::success:
        return "success";
    }
    return "common error";
  }
};

}  // detail

inline const boost::system::error_category& get_error_category() {
  static detail::error_category instance;
  return instance;
}

}  // error
}  // common

#endif  // UDT_COMMON_ERROR_ERROR_H_
