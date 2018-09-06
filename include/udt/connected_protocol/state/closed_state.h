#ifndef UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_

#include <memory>

#include "base_state.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class ClosedState : public BaseState<Protocol> {
 public:
  using Ptr = std::shared_ptr<ClosedState>;

 public:
  static Ptr Create(boost::asio::io_context& io_context) {
    return Ptr(new ClosedState(io_context));
  }

  virtual ~ClosedState() {}

  virtual typename BaseState<Protocol>::type GetType() { return this->CLOSED; }

 private:
  ClosedState(boost::asio::io_context& io_context)
      : BaseState<Protocol>(io_context) {}
};

}  // namespace state
}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_
