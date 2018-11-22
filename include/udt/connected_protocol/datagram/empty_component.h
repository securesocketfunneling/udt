#ifndef UDT_CONNECTED_PROTOCOL_DATAGRAM_EMPTY_COMPONENT_H_
#define UDT_CONNECTED_PROTOCOL_DATAGRAM_EMPTY_COMPONENT_H_

#include "../io/buffers.h"

namespace connected_protocol {
namespace datagram {

class EmptyComponent {
 public:
  using ConstBuffers = io::fixed_const_buffer_sequence;
  using MutableBuffers = io::fixed_mutable_buffer_sequence;
  enum { size = 0 };

 public:
  EmptyComponent() {}
  ~EmptyComponent() {}

  ConstBuffers GetConstBuffers() const { return ConstBuffers(); }
  void GetConstBuffers(ConstBuffers* p_buffers) const {}

  MutableBuffers GetMutableBuffers() { return MutableBuffers(); }
  void GetMutableBuffers(MutableBuffers* p_buffers) {}
};

}  // namespace datagram
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_DATAGRAM_EMPTY_COMPONENT_H_
