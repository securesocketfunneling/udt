#ifndef UDT_CONNECTED_PROTOCOL_MULTIPLEXER_H_
#define UDT_CONNECTED_PROTOCOL_MULTIPLEXER_H_

#include <cstdint>

#include <atomic>
#include <map>
#include <memory>

#include <boost/asio/buffer.hpp>

#include <boost/bind.hpp>
#include <chrono>

#include <boost/optional.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <boost/system/error_code.hpp>

#include "../common/error/error.h"

#include "flow.h"
#include "cache/connection_info.h"

#include "logger/log_entry.h"

namespace connected_protocol {

template <class Protocol>
class Multiplexer : public std::enable_shared_from_this<Multiplexer<Protocol>> {
 private:
  using protocol_type = Protocol;
  using Logger = typename Protocol::logger;
  using NextSocket = typename protocol_type::next_layer_protocol::socket;
  using NextEndpoint = typename protocol_type::next_layer_protocol::endpoint;
  using NextEndpointPtr = std::shared_ptr<NextEndpoint>;
  using SocketSession = typename protocol_type::socket_session;
  using SocketSessionPtr = std::shared_ptr<SocketSession>;
  using AcceptorSession = typename protocol_type::acceptor_session;
  using AcceptorSessionPtr = std::shared_ptr<AcceptorSession>;
  using MultiplexerManager = typename protocol_type::multiplexer_manager;

 private:
  using SocketId = uint32_t;

 private:
  using GenericDatagram = typename protocol_type::GenericReceiveDatagram;
  using GenericDatagramPtr = std::shared_ptr<GenericDatagram>;
  using ConnectionDatagram = typename protocol_type::ConnectionDatagram;
  using ControlDatagram = typename protocol_type::GenericControlDatagram;
  using DataDatagram = typename protocol_type::DataDatagram;
  using FlowPtr = std::shared_ptr<Flow<Protocol>>;

 private:
  using FlowsMap = std::map<NextEndpoint, FlowPtr>;
  using FlowSessionsMap = std::map<SocketId, SocketSessionPtr>;
  using RemoteEndpointFlowMap = std::map<NextEndpoint, FlowSessionsMap>;

 public:
  using Ptr = std::shared_ptr<Multiplexer>;

 public:
  static Ptr Create(MultiplexerManager *p_manager, NextSocket socket) {
    return Ptr(new Multiplexer(p_manager, std::move(socket)));
  }

  ~Multiplexer() {}

  void Start() {
    if (!running_.load()) {
      running_ = true;
      ReadPacket();
    }
  }

  void Stop(boost::system::error_code &ec) {
    if (running_.load()) {
      running_ = false;
      p_worker_.reset();

      {
        boost::mutex::scoped_lock lock_socket(socket_mutex_);
        socket_.shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket_.close(ec);
      }
    }
  }

  boost::asio::io_context &get_io_context() { return socket_.get_io_context(); }

  NextEndpoint local_endpoint(boost::system::error_code &ec) {
    return socket_.local_endpoint(ec);
  }

  SocketSessionPtr CreateSocketSession(boost::system::error_code &ec,
                                       const NextEndpoint &next_remote_endpoint,
                                       SocketId user_socket_id = 0) {
    boost::recursive_mutex::scoped_lock lock_sockets_map(sessions_mutex_);
    SocketId id(0);
    if (user_socket_id == 0) {
      id = GenerateSocketId(next_remote_endpoint);
    } else {
      id = IsSocketIdAvailable(next_remote_endpoint, user_socket_id)
               ? user_socket_id
               : 0;
    }

    if (id == 0) {
      ec.assign(::common::error::address_not_available,
                ::common::error::get_error_category());
      return nullptr;
    }

    SocketSessionPtr p_session(SocketSession::Create(
        this->shared_from_this(), GetFlow(next_remote_endpoint)));

    p_session->set_socket_id(id);
    p_session->set_next_local_endpoint(local_endpoint(ec));
    p_session->set_next_remote_endpoint(next_remote_endpoint);
    remote_endpoint_flow_sessions_[next_remote_endpoint][id] = p_session;
    return p_session;
  }

  void RemoveSocketSession(const NextEndpoint &next_remote_endpoint,
                           SocketId socket_id = 0) {
    boost::recursive_mutex::scoped_lock lock_sockets_map(sessions_mutex_);
    boost::recursive_mutex::scoped_lock lock_flows(flows_mutex_);

    typename RemoteEndpointFlowMap::iterator r_ep_flow_it(
        remote_endpoint_flow_sessions_.find(next_remote_endpoint));
    if (r_ep_flow_it == remote_endpoint_flow_sessions_.end()) {
      return;
    }

    r_ep_flow_it->second.erase(socket_id);
    if (r_ep_flow_it->second.empty()) {
      RemoveFlow(next_remote_endpoint);
      remote_endpoint_flow_sessions_.erase(next_remote_endpoint);
    }
    if (remote_endpoint_flow_sessions_.empty() && !p_acceptor_) {
      p_manager_->CleanMultiplexer(socket_.local_endpoint());
    }
  }

  void SetAcceptor(boost::system::error_code &ec,
                   AcceptorSessionPtr p_acceptor) {
    boost::mutex::scoped_lock lock_acceptor(acceptor_mutex_);
    if (p_acceptor_) {
      ec.assign(::common::error::address_in_use,
                ::common::error::get_error_category());
      return;
    }
    p_acceptor_ = p_acceptor;

    p_acceptor->set_p_multiplexer(this->shared_from_this());

    ec.assign(::common::error::success, ::common::error::get_error_category());
  }

  void RemoveAcceptor() {
    boost::recursive_mutex::scoped_lock lock_sessions(sessions_mutex_);
    boost::mutex::scoped_lock lock_acceptor(acceptor_mutex_);

    p_acceptor_.reset();

    if (remote_endpoint_flow_sessions_.empty() && !p_acceptor_) {
      p_manager_->CleanMultiplexer(socket_.local_endpoint());
    }
  }

  // High priority sending : use for control packet only
  template <class Datagram, class Handler>
  void AsyncSendControlPacket(const Datagram &datagram,
                              const NextEndpoint &next_endpoint,
                              Handler handler) {
    auto self = this->shared_from_this();
    auto sent_handler =
        [handler, self](const boost::system::error_code &sent_ec,
                        std::size_t length) { handler(sent_ec, length); };

    {
      boost::mutex::scoped_lock lock_socket(socket_mutex_);
      socket_.async_send_to(datagram.GetConstBuffers(), next_endpoint,
                            sent_handler);
    }
  }

  template <class Datagram, class Handler>
  void AsyncSendDataPacket(Datagram *p_datagram,
                           const NextEndpoint &next_endpoint, Handler handler) {
    if (p_datagram->is_acked()) {
      this->get_io_context().post(
          boost::asio::detail::binder2<decltype(handler),
                                       boost::system::error_code, std::size_t>(
              handler,
              boost::system::error_code(::common::error::identifier_removed,
                                        ::common::error::get_error_category()),
              0));
      return;
    }
    p_datagram->set_pending_send(true);
    auto self = this->shared_from_this();
    auto sent_handler = [p_datagram, handler, self](
        const boost::system::error_code &sent_ec, std::size_t length) {
      p_datagram->set_pending_send(false);
      if (!sent_ec && Logger::ACTIVE) {
        self->sent_count_ = self->sent_count_.load() + 1;
      }
      handler(sent_ec, length);
    };

    {
      boost::mutex::scoped_lock lock_socket(socket_mutex_);
      socket_.async_send_to(p_datagram->GetConstBuffers(), next_endpoint,
                            std::move(sent_handler));
    }
  }

  void Log(connected_protocol::logger::LogEntry *p_log) {
    p_log->multiplexer_sent_count = sent_count_.load();
  }

  void ResetLog() { sent_count_ = 0; }

 private:
  Multiplexer(MultiplexerManager *p_manager, NextSocket socket)
      : p_manager_(p_manager),
        socket_mutex_(),
        socket_(std::move(socket)),
        p_worker_(new boost::asio::io_context::work(socket_.get_io_context())),
        running_(false),
        flows_mutex_(),
        flows_(),
        sessions_mutex_(),
        remote_endpoint_flow_sessions_(),
        acceptor_mutex_(),
        p_acceptor_(nullptr),
        gen_(static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
                .count())),
        sent_count_(0) {}

  void ReadPacket() {
    if (!running_.load() || !socket_.is_open()) {
      return;
    }

    {
      boost::mutex::scoped_lock lock_socket(socket_mutex_);
      auto p_generic_packet = std::make_shared<GenericDatagram>();
      auto p_next_remote_endpoint = std::make_shared<NextEndpoint>();

      socket_.async_receive_from(
          p_generic_packet->GetMutableBuffers(), *p_next_remote_endpoint,
          boost::bind(&Multiplexer::HandlePacket, this->shared_from_this(),
                      p_generic_packet, p_next_remote_endpoint, _1, _2));
    }
  }

  void HandlePacket(GenericDatagramPtr p_generic_packet,
                    NextEndpointPtr p_next_remote_endpoint,
                    const boost::system::error_code &ec, std::size_t length) {
    if (!running_.load()) {
      return;
    }

    p_generic_packet->payload().SetSize(static_cast<uint32_t>(length) -
                                        GenericDatagram::Header::size);

    if (ec) {
      ReadPacket();
      return;
    }

    auto &header = p_generic_packet->header();
    auto p_socket_session_optional(
        GetSocketSession(*p_next_remote_endpoint, header.GetSocketId()));

    if (header.IsDataPacket()) {
      if (!p_socket_session_optional) {
        // Drop datagram if no session found
        p_generic_packet.reset();
        ReadPacket();
        return;
      }

      DataDatagram data_datagram;
      boost::asio::buffer_copy(data_datagram.header().GetMutableBuffers(),
                               p_generic_packet->header().GetConstBuffers());
      auto &data_payload = data_datagram.payload();
      auto &received_payload = p_generic_packet->payload();
      data_payload = std::move(received_payload);
      p_generic_packet.reset();
      // Forward DataDatagram
      (*p_socket_session_optional)->PushDataDgr(&data_datagram);
      ReadPacket();
      return;
    }

    if (header.IsControlPacket()) {
      ControlDatagram control_datagram;
      boost::asio::buffer_copy(control_datagram.GetMutableBuffers(),
                               p_generic_packet->GetConstBuffers());
      control_datagram.payload().SetSize(p_generic_packet->payload().GetSize());
      if (control_datagram.header().IsType(
              ControlDatagram::Header::CONNECTION)) {
        auto p_connection_datagram = std::make_shared<ConnectionDatagram>();
        boost::asio::buffer_copy(p_connection_datagram->GetMutableBuffers(),
                                 control_datagram.GetConstBuffers());
        p_generic_packet.reset();

        if (p_socket_session_optional) {
          (*p_socket_session_optional)
              ->PushConnectionDgr(p_connection_datagram);
          ReadPacket();
          return;
        }

        {
          boost::mutex::scoped_lock lock(acceptor_mutex_);
          // Check if acceptor exists
          if (p_acceptor_) {
            p_acceptor_->PushConnectionDgr(p_connection_datagram,
                                           p_next_remote_endpoint);
          }
        }

        ReadPacket();
        // Drop connection datagram
      } else {
        if (!p_socket_session_optional) {
          // Drop datagram
          ReadPacket();
          return;
        }
        // Forward ControlDatagram
        (*p_socket_session_optional)->PushControlDgr(&control_datagram);
        ReadPacket();
      }
    }
  }

  bool IsSocketIdAvailable(const NextEndpoint &next_remote_endpoint,
                           SocketId socket_id) {
    boost::recursive_mutex::scoped_lock lock_sockets_map(sessions_mutex_);
    typename RemoteEndpointFlowMap::const_iterator r_ep_flow_it(
        remote_endpoint_flow_sessions_.find(next_remote_endpoint));
    if (r_ep_flow_it == remote_endpoint_flow_sessions_.end()) {
      return true;
    }

    typename FlowSessionsMap::const_iterator session_it(
        r_ep_flow_it->second.find(socket_id));
    return session_it == r_ep_flow_it->second.end();
  }

  SocketId GenerateSocketId(const NextEndpoint &next_remote_endpoint) {
    boost::recursive_mutex::scoped_lock lock_sockets_map(sessions_mutex_);
    boost::random::uniform_int_distribution<uint32_t> dist(
        1, std::numeric_limits<uint32_t>::max());
    uint32_t rand_id;
    for (int i = 0; i < 100; ++i) {
      rand_id = dist(gen_);
      if (IsSocketIdAvailable(next_remote_endpoint, rand_id)) {
        return rand_id;
      }
    }

    return 0;
  }

  boost::optional<SocketSessionPtr> GetSocketSession(
      const NextEndpoint &next_remote_endpoint, SocketId socket_id) {
    boost::recursive_mutex::scoped_lock lock_sessions(sessions_mutex_);
    boost::optional<SocketSessionPtr> session_optional;

    typename RemoteEndpointFlowMap::const_iterator r_ep_flow_it(
        remote_endpoint_flow_sessions_.find(next_remote_endpoint));
    if (r_ep_flow_it != remote_endpoint_flow_sessions_.end()) {
      typename FlowSessionsMap::const_iterator session_it(
          r_ep_flow_it->second.find(socket_id));
      if (session_it != r_ep_flow_it->second.end()) {
        session_optional.reset(session_it->second);
      }
    }

    return session_optional;
  }

  FlowPtr GetFlow(const NextEndpoint &next_remote_endpoint) {
    boost::recursive_mutex::scoped_lock lock_flows(flows_mutex_);
    typename FlowsMap::const_iterator flow_it(
        flows_.find(next_remote_endpoint));
    if (flow_it != flows_.end()) {
      return flow_it->second;
    }

    FlowPtr p_flow(Flow<Protocol>::Create(get_io_context()));
    flows_[next_remote_endpoint] = p_flow;

    return p_flow;
  }

  void RemoveFlow(const NextEndpoint &next_remote_endpoint) {
    boost::recursive_mutex::scoped_lock lock_flows(flows_mutex_);
    typename FlowsMap::iterator flow_it(flows_.find(next_remote_endpoint));
    if (flow_it == flows_.end()) {
      return;
    }

    flows_.erase(flow_it);
  }

 private:
  MultiplexerManager *p_manager_;
  boost::mutex socket_mutex_;
  NextSocket socket_;
  std::unique_ptr<boost::asio::io_context::work> p_worker_;
  std::atomic<bool> running_;
  boost::recursive_mutex flows_mutex_;
  FlowsMap flows_;
  boost::recursive_mutex sessions_mutex_;
  RemoteEndpointFlowMap remote_endpoint_flow_sessions_;
  boost::mutex acceptor_mutex_;
  AcceptorSessionPtr p_acceptor_;
  boost::random::mt19937 gen_;
  std::atomic<uint32_t> sent_count_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_MULTIPLEXER_H_
