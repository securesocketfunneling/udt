#ifndef UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_
#define UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_

#include <memory>

#include "udt/connected_protocol/state/base_state.h"

namespace connected_protocol {
namespace state {

template <class Protocol>
class ClosedState : public BaseState<Protocol> {
 public:
  typedef std::shared_ptr<ClosedState> Ptr;
  typedef typename Protocol::socket_session SocketSession;
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;

 public:
  static Ptr Create(boost::asio::io_service& io_service) {
    return Ptr(new ClosedState(io_service));
  }

  virtual typename BaseState<Protocol>::type GetType() { return this->CLOSED; }

  virtual boost::asio::io_service& get_io_service() { return io_service_; }

 private:
  ClosedState(boost::asio::io_service& io_service) : io_service_(io_service) {}

 private:
  boost::asio::io_service& io_service_;
};

}  // state
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STATE_CLOSED_STATE_H_
