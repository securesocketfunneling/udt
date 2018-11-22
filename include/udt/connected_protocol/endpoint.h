#ifndef UDT_CONNECTED_PROTOCOL_ENDPOINT_H_
#define UDT_CONNECTED_PROTOCOL_ENDPOINT_H_

#include <cstdint>
#include <utility>

#include <boost/asio/detail/socket_types.hpp>
#include <boost/asio/ip/address.hpp>

namespace connected_protocol {

template <class TProtocol>
class Endpoint {
 public:
  using protocol_type = TProtocol;
  using data_type = boost::asio::detail::socket_addr_type;
  using NextLayer = typename TProtocol::next_layer_protocol;
  using NextLayerEndpoint = typename NextLayer::endpoint;
  using SocketId = uint32_t;

 public:
  Endpoint() : socket_id_(0), next_layer_endpoint_() {}

  Endpoint(const TProtocol& protocol_type, unsigned short port_num)
      : socket_id_{0},
        next_layer_endpoint_{(protocol_type == TProtocol::v4())
                                 ? NextLayer::v4()
                                 : NextLayer::v6(),
                             port_num} {}

  Endpoint(const boost::asio::ip::address& addr, unsigned short port_num)
      : socket_id_{0}, next_layer_endpoint_{addr, port_num} {}

  Endpoint(SocketId socket_id, const NextLayerEndpoint& next_layer_endpoint)
      : socket_id_(socket_id), next_layer_endpoint_(next_layer_endpoint) {}

  Endpoint(const Endpoint& other)
      : socket_id_(other.socket_id_),
        next_layer_endpoint_(other.next_layer_endpoint_) {}

  Endpoint(Endpoint&& other)
      : socket_id_(std::move(other.socket_id_)),
        next_layer_endpoint_(std::move(other.next_layer_endpoint_)) {}

  Endpoint& operator=(const Endpoint& other) {
    socket_id_ = other.socket_id_;
    next_layer_endpoint_ = other.next_layer_endpoint_;

    return *this;
  }

  Endpoint& operator=(Endpoint&& other) {
    socket_id_ = std::move(other.socket_id_);
    next_layer_endpoint_ = std::move(other.next_layer_endpoint_);

    return *this;
  }

  protocol_type protocol() const {
    if (next_layer_endpoint_.protocol() == NextLayer::v4()) {
      return protocol_type::v4();
    } else {
      return protocol_type::v6();
    }
  }

  SocketId socket_id() const { return socket_id_; }

  void socket_id(SocketId socket_id) { socket_id_ = socket_id; }

  NextLayerEndpoint next_layer_endpoint() const { return next_layer_endpoint_; }

  void next_layer_endpoint(const NextLayerEndpoint& next_layer_endpoint) {
    next_layer_endpoint_ = next_layer_endpoint;
  }

  bool operator==(const Endpoint& rhs) const {
    return socket_id_ == rhs.socket_id_ &&
           next_layer_endpoint_ == rhs.next_layer_endpoint_;
  }

  bool operator<(const Endpoint& rhs) const {
    if (socket_id_ != rhs.socket_id_) {
      return socket_id_ < rhs.socket_id_;
    }

    return next_layer_endpoint_ < rhs.next_layer_endpoint_;
  }

  /// Get the underlying endpoint in the native type.
  data_type* data() { return next_layer_endpoint_.data(); }

  /// Get the underlying endpoint in the native type.
  const data_type* data() const { return next_layer_endpoint_.data(); }

  /// Get the underlying size of the endpoint in the native type.
  std::size_t size() const { return next_layer_endpoint_.size(); }

  /// Set the underlying size of the endpoint in the native type.
  void resize(std::size_t new_size) { next_layer_endpoint_.resize(new_size); }

  /// Get the capacity of the endpoint in the native type.
  std::size_t capacity() const { return next_layer_endpoint_.capacity(); }

 private:
  SocketId socket_id_;
  NextLayerEndpoint next_layer_endpoint_;
};

}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_ENDPOINT_H_
