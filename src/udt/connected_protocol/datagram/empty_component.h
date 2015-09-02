#ifndef UDT_CONNECTED_PROTOCOL_DATAGRAM_EMPTY_COMPONENT_H_
#define UDT_CONNECTED_PROTOCOL_DATAGRAM_EMPTY_COMPONENT_H_

#include "udt/connected_protocol/io/buffers.h"

namespace connected_protocol {
namespace datagram {

class EmptyComponent {
 public:
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef io::fixed_mutable_buffer_sequence MutableBuffers;
  enum { size = 0 };

 public:
  EmptyComponent() {}
  ~EmptyComponent() {}

  ConstBuffers GetConstBuffers() const { return ConstBuffers(); }
  void GetConstBuffers(ConstBuffers* p_buffers) const {}

  MutableBuffers GetMutableBuffers() { return MutableBuffers(); }
  void GetMutableBuffers(MutableBuffers* p_buffers) {}
};

}  // datagram
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_DATAGRAM_EMPTY_COMPONENT_H_
