#ifndef UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_DATAGRAM_H_
#define UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_DATAGRAM_H_

#include <cstdint>

#include <vector>

#include <boost/asio/buffer.hpp>

#include "udt/connected_protocol/io/buffers.h"

namespace connected_protocol {
namespace datagram {

template <class THeader, class TPayload>
class basic_Datagram {
 public:
  using Header = THeader;
  using Payload = TPayload;

 public:
  using ConstBuffers = io::fixed_const_buffer_sequence;
  using MutableBuffers = io::fixed_mutable_buffer_sequence;

 public:
  enum { size = Header::size + Payload::size };

 public:
  basic_Datagram()
      : header_(), payload_(), pending_send_(false), acked_(false) {}

  basic_Datagram(Header header, Payload payload)
      : header_(std::move(header)), payload_(std::move(payload)) {}

  template <class OtherPayload>
  basic_Datagram(basic_Datagram<Header, OtherPayload> datagram)
      : header_(std::move(datagram.header)),
        payload_(std::move(datagram.payload)) {}

  basic_Datagram& operator=(const basic_Datagram& other) {
    header_ = other.header_;
    payload_ = other.payload_;
    pending_send_ = other.pending_send_;
    acked_ = other.acked_;

    return *this;
  }

  basic_Datagram& operator=(basic_Datagram&& other) {
    header_ = std::move(other.header_);
    payload_ = std::move(other.payload_);
    pending_send_ = other.pending_send_.load();
    acked_ = other.acked_.load();

    return *this;
  }

  ~basic_Datagram() {}

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    header_.GetConstBuffers(&buffers);
    payload_.GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    header_.GetConstBuffers(p_buffers);
    payload_.GetConstBuffers(p_buffers);
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    header_.GetMutableBuffers(&buffers);
    payload_.GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers* p_buffers) {
    header_.GetMutableBuffers(p_buffers);
    payload_.GetMutableBuffers(p_buffers);
  }

  Header& header() { return header_; }
  const Header& header() const { return header_; }

  Payload& payload() { return payload_; }
  const Payload& payload() const { return payload_; }

  void set_pending_send(bool pending_send) { pending_send_ = pending_send; };

  bool is_pending_send() const { return pending_send_.load(); }

  void set_acked(bool acked) { acked_ = acked; }

  bool is_acked() { return acked_.load(); }

 private:
  Header header_;
  Payload payload_;
  std::atomic<bool> pending_send_;
  std::atomic<bool> acked_;
};

}  // datagram
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_DATAGRAM_H_
