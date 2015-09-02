#ifndef UDT_CONNECTED_PROTOCOL_COMMON_OBSERVER_H_
#define UDT_CONNECTED_PROTOCOL_COMMON_OBSERVER_H_

#include <memory>

namespace connected_protocol {
namespace common {
template <class TSubject>
class Observer {
 public:
  typedef std::shared_ptr<Observer> Ptr;
  typedef TSubject Subject;

 public:
  virtual void Notify(Subject*) = 0;
};
}  // common
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_COMMON_OBSERVER_H_
