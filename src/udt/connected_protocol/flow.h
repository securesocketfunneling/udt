#ifndef UDT_CONNECTED_PROTOCOL_FLOW_H_
#define UDT_CONNECTED_PROTOCOL_FLOW_H_

#include <cstdint>

#include <chrono>
#include <set>
#include <memory>

#include <boost/asio/buffer.hpp>

#include <boost/log/trivial.hpp>

#include <boost/bind.hpp>

#include <boost/system/error_code.hpp>

#include "udt/connected_protocol/logger/log_entry.h"

namespace connected_protocol {

template <class Protocol>
class Flow : public std::enable_shared_from_this<Flow<Protocol>> {
 public:
  typedef typename Protocol::next_layer_protocol::endpoint NextEndpointType;
  typedef typename Protocol::SendDatagram Datagram;
  typedef std::shared_ptr<Datagram> DatagramPtr;

  struct DatagramAddressPair {
    DatagramPtr p_datagram;
    NextEndpointType remote_endpoint;
  };

 private:
  typedef typename Protocol::timer Timer;
  typedef typename Protocol::clock Clock;
  typedef typename Protocol::time_point TimePoint;
  typedef typename Protocol::logger Logger;
  typedef typename Protocol::socket_session SocketSession;

  struct CompareSessionPacketSendingTime {
    bool operator()(typename SocketSession::Ptr p_lhs,
                    typename SocketSession::Ptr p_rhs) {
      return p_lhs->NextScheduledPacketTime() <
             p_rhs->NextScheduledPacketTime();
    }
  };

  typedef std::set<typename SocketSession::Ptr, CompareSessionPacketSendingTime>
      SocketsContainer;

 public:
  typedef std::shared_ptr<Flow> Ptr;

 public:
  static Ptr Create(boost::asio::io_service& io_service) {
    return Ptr(new Flow(io_service));
  }

  void RegisterNewSocket(typename SocketSession::Ptr p_session) {
    {
      boost::mutex::scoped_lock lock(mutex_);
      socket_sessions_.insert(p_session);
    }

    StartPullingSocketQueue();
  }

  void Log(connected_protocol::logger::LogEntry* p_log) {
    p_log->flow_sent_count = sent_count_.load();
  }

  void ResetLog() { sent_count_ = 0; }

 private:
  Flow(boost::asio::io_service& io_service)
      : io_service_(io_service),
        mutex_(),
        socket_sessions_(),
        next_packet_timer_(io_service),
        pulling_(false) {}

  void StartPullingSocketQueue() {
    if (pulling_.load()) {
      return;
    }
    pulling_ = true;
    PullSocketQueue();
  }

  void PullSocketQueue() {
    typename SocketsContainer::iterator p_next_socket_expired_it;

    if (!pulling_.load()) {
      return;
    }

    {
      boost::mutex::scoped_lock lock_socket_sessions(mutex_);
      if (socket_sessions_.empty()) {
        this->StopPullSocketQueue();
        return;
      }

      p_next_socket_expired_it = socket_sessions_.begin();

      auto next_scheduled_packet_interval =
          (*p_next_socket_expired_it)->NextScheduledPacketTime();

      if (next_scheduled_packet_interval.count() <= 0) {
        // Resend immediatly
        boost::system::error_code ec;
        ec.assign(::common::error::success,
                  ::common::error::get_error_category());
        io_service_.post(boost::bind(&Flow::WaitPullSocketHandler,
                                     this->shared_from_this(), ec));
        return;
      }

      next_packet_timer_.expires_from_now(next_scheduled_packet_interval);
      next_packet_timer_.async_wait(boost::bind(&Flow::WaitPullSocketHandler,
                                                this->shared_from_this(), _1));
    }
  }

  // When no data is available
  void StopPullSocketQueue() {
    pulling_ = false;
    boost::system::error_code ec;
    next_packet_timer_.cancel(ec);
  }

  void WaitPullSocketHandler(const boost::system::error_code& ec) {
    if (ec) {
      BOOST_LOG_TRIVIAL(trace) << "Error on pull flow";
      StopPullSocketQueue();
      return;
    }

    typename SocketSession::Ptr p_session;
    Datagram* p_datagram;
    {
      boost::mutex::scoped_lock lock_socket_sessions(mutex_);

      if (socket_sessions_.empty()) {
        StopPullSocketQueue();
        return;
      }

      typename SocketsContainer::iterator p_session_it;
      p_session_it = socket_sessions_.begin();
      p_session = *p_session_it;
      socket_sessions_.erase(p_session_it);

      p_datagram = p_session->NextScheduledPacket();

      if (p_session->HasPacketToSend()) {
        socket_sessions_.insert(p_session);
      }
    }

    if (p_datagram && p_session) {
      if (Logger::ACTIVE) {
        sent_count_ = sent_count_.load() + 1;
      }
      auto self = this->shared_from_this();
      p_session->AsyncSendPacket(
          p_datagram, [p_datagram](const boost::system::error_code& ec,
                                   std::size_t length) {});
    }

    PullSocketQueue();
  }

 private:
  boost::asio::io_service& io_service_;

  boost::mutex mutex_;

  // sockets list
  SocketsContainer socket_sessions_;

  Timer next_packet_timer_;

  std::atomic<bool> pulling_;

  std::atomic<uint32_t> sent_count_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_FLOW_H_
