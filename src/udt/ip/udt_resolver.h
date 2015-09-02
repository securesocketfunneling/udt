#ifndef UDT_IP_UDT_RESOLVER_H_
#define UDT_IP_UDT_RESOLVER_H_

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include "udt/connected_protocol/resolver.h"

#include "udt/ip/udt_query.h"

namespace ip {

template <class UDTProtocol>
class UDTResolver : public connected_protocol::Resolver<UDTProtocol> {
 public:
  typedef UDTQuery<UDTProtocol> query;

 public:
  UDTResolver(boost::asio::io_service& io_service)
      : connected_protocol::Resolver<UDTProtocol>(io_service) {}
};

}  // ip

#endif  // UDT_IP_UDT_RESOLVER_H_
