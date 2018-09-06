#ifndef UDT_CONNECTED_PROTOCOL_FLOW_H_
#define UDT_CONNECTED_PROTOCOL_FLOW_H_

#include <cstdint>

#include <chrono>
#include <memory>
#include <set>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/bind.hpp>

#include <boost/system/error_code.hpp>

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include "logger/log_entry.h"

namespace connected_protocol {

template <class Protocol>
class Flow : public std::enable_shared_from_this<Flow<Protocol>> {
 public:
  using NextEndpointType = typename Protocol::next_layer_protocol::endpoint;
  using Datagram = typename Protocol::SendDatagram;
  using DatagramPtr = std::shared_ptr<Datagram>;

  struct DatagramAddressPair {
    DatagramPtr p_datagram;
    NextEndpointType remote_endpoint;
  };

 private:
  using Timer = typename Protocol::timer;
  using Clock = typename Protocol::clock;
  using TimePoint = typename Protocol::time_point;
  using Logger = typename Protocol::logger;
  using SocketSession = typename Protocol::socket_session;

  struct CompareSessionPacketSendingTime {
    bool operator()(std::weak_ptr<SocketSession> p_lhs,
                    std::weak_ptr<SocketSession> p_rhs) const {
      auto p_shared_lhs = p_lhs.lock();
      auto p_shared_rhs = p_rhs.lock();
      if (!p_shared_lhs || !p_shared_rhs) {
        return true;
      }

      return p_shared_lhs->NextScheduledPacketTime() <
             p_shared_rhs->NextScheduledPacketTime();
    }
  };

  using SocketsContainer =
      std::set<std::weak_ptr<SocketSession>, CompareSessionPacketSendingTime>;

 public:
  using Ptr = std::shared_ptr<Flow>;

 public:
  static Ptr Create(boost::asio::io_context& io_context) {
    return Ptr(new Flow(io_context));
  }

  ~Flow() {}

  void RegisterNewSocket(typename SocketSession::Ptr p_session) {
    {
      boost::lock_guard<boost::recursive_mutex> lock(mutex_);
      auto inserted = socket_sessions_.insert(p_session);
      if (inserted.second) {
        // relaunch timer since there is a new socket
        StopPullSocketQueue();
      }
    }

    StartPullingSocketQueue();
  }

  void Log(connected_protocol::logger::LogEntry* p_log) {
    p_log->flow_sent_count = sent_count_.load();
  }

  void ResetLog() { sent_count_ = 0; }

 private:
  Flow(boost::asio::io_context& io_context)
      : io_context_(io_context),
        mutex_(),
        socket_sessions_(),
        next_packet_timer_(io_context),
        pulling_(false),
        sent_count_(0) {}

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
      boost::lock_guard<boost::recursive_mutex> lock_socket_sessions(mutex_);
      if (socket_sessions_.empty()) {
        this->StopPullSocketQueue();
        return;
      }

      p_next_socket_expired_it = socket_sessions_.begin();

      auto p_next_socket_expired = (*p_next_socket_expired_it).lock();
      if (!p_next_socket_expired) {
        io_context_.post(
            boost::bind(&Flow::PullSocketQueue, this->shared_from_this()));
        return;
      }

      auto next_scheduled_packet_interval =
          p_next_socket_expired->NextScheduledPacketTime();

      if (next_scheduled_packet_interval.count() <= 0) {
        // Resend immediatly
        boost::system::error_code ec;
        ec.assign(::common::error::success,
                  ::common::error::get_error_category());
        io_context_.post(boost::bind(&Flow::WaitPullSocketHandler,
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
      StopPullSocketQueue();
      return;
    }

    typename SocketSession::Ptr p_session;
    Datagram* p_datagram;
    {
      boost::lock_guard<boost::recursive_mutex> lock_socket_sessions(mutex_);

      if (socket_sessions_.empty()) {
        StopPullSocketQueue();
        return;
      }

      typename SocketsContainer::iterator p_session_it;
      p_session_it = socket_sessions_.begin();

      p_session = (*p_session_it).lock();

      if (!p_session) {
        socket_sessions_.erase(p_session_it);
        PullSocketQueue();
        return;
      }
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
          p_datagram,
          [](const boost::system::error_code& ec, std::size_t length) {});
    }

    PullSocketQueue();
  }

 private:
  boost::asio::io_context& io_context_;

  boost::recursive_mutex mutex_;
  // sockets list
  SocketsContainer socket_sessions_;

  Timer next_packet_timer_;
  std::atomic<bool> pulling_;
  std::atomic<uint32_t> sent_count_;
};

}  // namespace connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_FLOW_H_
