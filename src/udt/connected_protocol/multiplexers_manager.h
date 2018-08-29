#ifndef UDT_CONNECTED_PROTOCOL_MULTIPLEXERS_MANAGER_H_
#define UDT_CONNECTED_PROTOCOL_MULTIPLEXERS_MANAGER_H_

#include <map>
#include <memory>

#include <boost/log/trivial.hpp>
#include <boost/thread/mutex.hpp>

#include "udt/connected_protocol/multiplexer.h"

namespace connected_protocol {

template <class Protocol>
class MultiplexerManager {
 public:
  using NextSocket = typename Protocol::next_layer_protocol::socket;
  using NextLayerEndpoint = typename Protocol::next_layer_protocol::endpoint;

 private:
  using MultiplexerPtr = typename Multiplexer<Protocol>::Ptr;
  using MultiplexersMap = std::map<NextLayerEndpoint, MultiplexerPtr>;

  // TODO move multiplexers management in service
 public:
  MultiplexerManager() : mutex_(), multiplexers_() {}
  
  MultiplexerPtr GetMultiplexer(boost::asio::io_context &io_context,
                                   const NextLayerEndpoint &next_local_endpoint,
                                   boost::system::error_code &ec) {
    boost::mutex::scoped_lock lock(mutex_);
    auto multiplexer_it = multiplexers_.find(next_local_endpoint);
    if (multiplexer_it == multiplexers_.end()) {
      NextSocket next_layer_socket(io_context);
      next_layer_socket.open(next_local_endpoint.protocol());
      // Empty endpoint will bind the socket to an available port
      next_layer_socket.bind(next_local_endpoint, ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error)
            << "Could not bind multiplexer on local endpoint";
        return nullptr;
      }

      MultiplexerPtr p_multiplexer =
          Multiplexer<Protocol>::Create(this, std::move(next_layer_socket));

      boost::system::error_code ec;

      multiplexers_[p_multiplexer->local_endpoint(ec)] = p_multiplexer;
      p_multiplexer->Start();

      return p_multiplexer;
    }

    return multiplexer_it->second;
  }

  void CleanMultiplexer(const NextLayerEndpoint &next_local_endpoint) {
    boost::mutex::scoped_lock lock(mutex_);
    if (multiplexers_.find(next_local_endpoint) != multiplexers_.end()) {
      boost::system::error_code ec;
      multiplexers_[next_local_endpoint]->Stop(ec);
      multiplexers_.erase(next_local_endpoint);
      // @todo should ec be swallowed here?
    }
  }

 private:
  boost::mutex mutex_;
  MultiplexersMap multiplexers_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_MULTIPLEXERS_MANAGER_H_
