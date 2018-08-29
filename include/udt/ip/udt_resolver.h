#ifndef UDT_IP_UDT_RESOLVER_H_
#define UDT_IP_UDT_RESOLVER_H_

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include "../connected_protocol/resolver.h"

#include "udt_query.h"

namespace ip {

template <class UDTProtocol>
class UDTResolver : public connected_protocol::Resolver<UDTProtocol> {
 public:
  using query = UDTQuery<UDTProtocol>;

 public:
  UDTResolver(boost::asio::io_context& io_context)
      : connected_protocol::Resolver<UDTProtocol>(io_context) {}
};

}  // ip

#endif  // UDT_IP_UDT_RESOLVER_H_
