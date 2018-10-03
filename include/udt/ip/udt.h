#ifndef UDT_IP_UDT_H_
#define UDT_IP_UDT_H_

#include <boost/asio/detail/socket_types.hpp>
#include <boost/asio/ip/udp.hpp>

#include "../connected_protocol/congestion/congestion_control.h"
#include "../connected_protocol/logger/no_log.h"
#include "../connected_protocol/protocol.h"

#define IPPROTO_UDT		IPPROTO_IP		/* SCTP+1 */

namespace ip {

template <class Logger = connected_protocol::logger::NoLog,
          template <class> class CongestionControlAlg =
              connected_protocol::congestion::CongestionControl>
class udt : public connected_protocol::Protocol<udt<>, boost::asio::ip::udp,
                                                Logger, CongestionControlAlg> {
 public:
  /// Construct to represent the IPv4 UDT protocol.
  static udt v4() { return udt(BOOST_ASIO_OS_DEF(AF_INET)); }

  /// Construct to represent the IPv6 UDT protocol.
  static udt v6() { return udt(BOOST_ASIO_OS_DEF(AF_INET6)); }

  /// Obtain an identifier for the type of the protocol.
  int type() const { return BOOST_ASIO_OS_DEF(SOCK_STREAM); }

  /// Obtain an identifier for the protocol.
  int protocol() const { return IPPROTO_UDT; }

  /// Obtain an identifier for the protocol family.
  int family() const { return family_; }

  /// Compare two protocols for equality.
  friend bool operator==(const udt& p1, const udt& p2) {
    return p1.family_ == p2.family_;
  }

  /// Compare two protocols for inequality.
  friend bool operator!=(const udt& p1, const udt& p2) {
    return p1.family_ != p2.family_;
  }

 private:
  // Construct with a specific family.
  explicit udt(int protocol_family) : family_(protocol_family) {}

  int family_;
};

}  // namespace ip

#endif  // UDT_IP_UDT_H_
