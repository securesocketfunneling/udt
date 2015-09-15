#ifndef UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_

#include <memory>

#include "udt/connected_protocol/state/base_state.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class ClosedState : public BaseState<Protocol> {
 public:
  using Ptr = std::shared_ptr<ClosedState>;

 public:
  static Ptr Create(boost::asio::io_service& io_service) {
    return Ptr(new ClosedState(io_service));
  }

  virtual ~ClosedState() {}

  virtual typename BaseState<Protocol>::type GetType() { return this->CLOSED; }

 private:
  ClosedState(boost::asio::io_service& io_service)
      : BaseState<Protocol>(io_service) {}
};

}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_
