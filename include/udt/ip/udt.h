#ifndef UDT_IP_UDT_H_
#define UDT_IP_UDT_H_

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/detail/socket_types.hpp>

#include "../connected_protocol/protocol.h"
#include "../connected_protocol/logger/no_log.h"
#include "../connected_protocol/congestion/congestion_control.h"
#include "udt_resolver.h"

namespace ip {

template <class Logger = connected_protocol::logger::NoLog,
          template <class> class CongestionControlAlg =
              connected_protocol::congestion::CongestionControl>
class udt {
 public:
  using protocol_type = connected_protocol::Protocol<boost::asio::ip::udp, Logger,
                                       CongestionControlAlg>;

  using endpoint = typename protocol_type::endpoint;
  using socket = typename protocol_type::socket;
  using resolver = UDTResolver<protocol_type>;
  using acceptor = typename protocol_type::acceptor;

  /// Obtain an identifier for the type of the protocol.
  int type() const { return BOOST_ASIO_OS_DEF(SOCK_STREAM); }

  /// Obtain an identifier for the protocol.
  int protocol() const { return BOOST_ASIO_OS_DEF(IPPROTO_IP); }

  /// Obtain an identifier for the protocol family.
  int family() const { return BOOST_ASIO_OS_DEF(AF_INET); }
};

}  // ip

#endif  // UDT_IP_UDT_H_
