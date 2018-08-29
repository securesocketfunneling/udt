#ifndef UDT_CONNECTED_PROTOCOL_RESOLVER_H_
#define UDT_CONNECTED_PROTOCOL_RESOLVER_H_

#include <cstdint>

#include <vector>

#include <boost/asio/io_context.hpp>

#include <boost/system/error_code.hpp>

#include "udt/common/error/error.h"

#include "udt/connected_protocol/resolver_query.h"

namespace connected_protocol {

template <class Protocol>
class Resolver {
 private:
  class EndpointIterator {
   public:
    EndpointIterator() : endpoints_(1), index_(0) {}
    EndpointIterator(std::vector<typename Protocol::endpoint> endpoints)
        : endpoints_(endpoints), index_(0) {}

    typename Protocol::endpoint& operator*() { return endpoints_[index_]; }
    typename Protocol::endpoint* operator->() { return &endpoints_[index_]; }

    typename Protocol::endpoint& operator++() {
      ++index_;
      return endpoints_[index_];
    }

    typename Protocol::endpoint operator++(int) {
      ++index_;
      return endpoints_[index_ - 1];
    }

    typename Protocol::endpoint& operator--() {
      --index_;
      return endpoints_[index_];
    }

    typename Protocol::endpoint operator--(int) {
      --index_;
      return endpoints_[index_ + 1];
    }

   private:
    std::vector<typename Protocol::endpoint> endpoints_;
    std::size_t index_;
  };

  using NextLayer = typename Protocol::next_layer_protocol;
  using NextLayerEndpoint = typename NextLayer::endpoint;

 public:
  using protocol_type = Protocol;
  using endpoint_type = typename Protocol::endpoint;
  using query = ResolverQuery<Protocol>;
  using iterator = EndpointIterator;

 public:
  Resolver(boost::asio::io_context& io_context) : io_context_(io_context) {}

  iterator resolve(const query& q, boost::system::error_code& ec) {
    typename NextLayer::resolver next_layer_resolver(io_context_);
    auto next_layer_iterator =
        next_layer_resolver.resolve(q.next_layer_query(), ec);
    if (ec) {
      return iterator();
    }

    std::vector<endpoint_type> result;
    result.emplace_back(q.socket_id(), NextLayerEndpoint(*next_layer_iterator));
    ec.assign(::common::error::success, ::common::error::get_error_category());

    return iterator(result);
  }

 private:
  boost::asio::io_context& io_context_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_RESOLVER_H_
