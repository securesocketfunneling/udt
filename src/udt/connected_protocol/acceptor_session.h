#ifndef UDT_CONNECTED_PROTOCOL_ACCEPTOR_SESSION_H_
#define UDT_CONNECTED_PROTOCOL_ACCEPTOR_SESSION_H_

#include <chrono>
#include <memory>

#include <boost/asio/detail/op_queue.hpp>

#include <boost/log/trivial.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/sha1.hpp>

#include "udt/common/error/error.h"

#include "udt/connected_protocol/io/accept_op.h"
#include "udt/connected_protocol/common/observer.h"

#include "udt/connected_protocol/state/base_state.h"
#include "udt/connected_protocol/state/accepting_state.h"
#include "udt/connected_protocol/state/closed_state.h"

namespace connected_protocol {

template <class Protocol>
class AcceptorSession
    : public common::Observer<typename Protocol::socket_session> {
 private:
  typedef typename Protocol::time_point TimePoint;
  typedef typename Protocol::clock Clock;

 private:
  typedef typename Protocol::socket_session SocketSession;
  typedef std::shared_ptr<SocketSession> SocketSessionPtr;
  typedef typename Protocol::next_layer_protocol::endpoint NextLayerEndpoint;
  typedef std::shared_ptr<NextLayerEndpoint> NextLayerEndpointPtr;
  typedef typename Protocol::ConnectionDatagram ConnectionDatagram;
  typedef std::shared_ptr<ConnectionDatagram> ConnectionDatagramPtr;
  typedef typename state::ClosedState<Protocol> ClosedState;
  typedef typename state::AcceptingState<Protocol> AcceptingState;
  typedef typename common::Observer<SocketSession> Observer;
  typedef typename io::basic_pending_accept_operation<Protocol> AcceptOp;
  typedef boost::asio::detail::op_queue<AcceptOp> AcceptOpQueue;

  typedef std::shared_ptr<typename Protocol::multiplexer> MultiplexerPtr;

  typedef uint32_t socket_id_type;
  typedef std::map<socket_id_type, SocketSessionPtr> RemoteSessionsMap;

 public:
  AcceptorSession()
      : p_multiplexer_(nullptr),
        mutex_(),
        accept_ops_(),
        connecting_sessions_(),
        connected_sessions_(),
        previous_connected_sessions_(),
        listening_(false),
        start_time_point_(Clock::now()) {}

  ~AcceptorSession() { StopListen(); }

  void set_p_multiplexer(MultiplexerPtr p_multiplexer) {
    p_multiplexer_ = std::move(p_multiplexer);
  }

  bool IsListening() { return listening_; }

  void Listen() { listening_ = true; }

  void StopListen() { listening_ = false; }

  NextLayerEndpoint next_local_endpoint(boost::system::error_code& ec) {
    if (!p_multiplexer_) {
      ec.assign(::common::error::no_link,
                ::common::error::get_error_category());
      return;
    }

    return p_multiplexer_->local_endpoint(ec);
  }

  void Close() {
    if (!p_multiplexer_) {
      return;
    }

    StopListen();

    for (auto& previous_connected_session_pair : previous_connected_sessions_) {
      previous_connected_session_pair.second->RemoveObserver(this);
    }
    for (auto& connected_session_pair : connected_sessions_) {
      connected_session_pair.second->RemoveObserver(this);
    }
    for (auto& connecting_session_pair : connecting_sessions_) {
      connecting_session_pair.second->RemoveObserver(this);
    }

    boost::system::error_code ec(::common::error::interrupted,
                                 ::common::error::get_error_category());
    Accept(ec);
    p_multiplexer_->RemoveAcceptor();
  }

  void PushAcceptOp(AcceptOp* p_accept_op) {
    {
      boost::recursive_mutex::scoped_lock lock_accept_ops(mutex_);
      accept_ops_.push(p_accept_op);
    }

    boost::system::error_code ec;
    Accept(ec);

    // @todo: should ec be swallowed here?
  }

  void PushConnectionDgr(ConnectionDatagramPtr p_connection_dgr,
                         NextLayerEndpointPtr p_remote_endpoint) {
    if (!p_multiplexer_) {
      return;
    }

    boost::recursive_mutex::scoped_lock lock_sessions(mutex_);
    if (!listening_) {
      // Not listening, drop packet
      return;
    }

    auto& header = p_connection_dgr->header();
    auto& payload = p_connection_dgr->payload();

    uint32_t receive_cookie = payload.syn_cookie();
    uint32_t destination_socket = header.destination_socket();

    auto remote_socket_id = payload.socket_id();

    SocketSessionPtr p_socket_session(GetSession(remote_socket_id));

    if (!p_socket_session) {
      // First handshake packet
      if (receive_cookie == 0 && destination_socket == 0) {
        HandleFirstHandshakePacket(p_connection_dgr, *p_remote_endpoint);
        return;
      }

      uint32_t server_cookie = GetSynCookie(*p_remote_endpoint);
      if (receive_cookie != server_cookie) {
        // Drop datagram -> cookies not equal
        return;
      }

      // New connection
      boost::system::error_code ec;
      p_socket_session =
          p_multiplexer_->CreateSocketSession(ec, *p_remote_endpoint);
      if (ec) {
        BOOST_LOG_TRIVIAL(trace) << "Error on socket session creation";
      }

      p_socket_session->remote_socket_id = remote_socket_id;
      p_socket_session->syn_cookie = server_cookie;
      p_socket_session->AddObserver(this);
      p_socket_session->ChangeState(AcceptingState::Create(p_socket_session));

      connecting_sessions_[remote_socket_id] = p_socket_session;
    }

    p_socket_session->PushConnectionDgr(p_connection_dgr);
  }

  void Accept(boost::system::error_code& ec = boost::system::error_code()) {
    if (!p_multiplexer_) {
      return;
    }

    boost::recursive_mutex::scoped_lock lock_sessions_accept_ops(mutex_);

    if (!ec) {
      if (!connected_sessions_.empty() && !accept_ops_.empty()) {
        auto p_connected_pair = connected_sessions_.begin();
        auto p_socket_session = p_connected_pair->second;

        auto op = std::move(accept_ops_.front());
        accept_ops_.pop();

        auto& peer_socket = op->peer();
        auto& impl = peer_socket.native_handle();
        impl = p_socket_session;
        auto do_complete = [op, p_socket_session, ec]() { op->complete(ec); };

        previous_connected_sessions_[p_connected_pair->first] =
            p_socket_session;
        connected_sessions_.erase(p_connected_pair->first);
        p_multiplexer_->get_io_service().post(std::move(do_complete));
        return;
      }
    } else {
      while (!accept_ops_.empty()) {
        auto op = std::move(accept_ops_.front());
        accept_ops_.pop();

        auto do_complete = [op, ec]() { op->complete(ec); };

        p_multiplexer_->get_io_service().post(std::move(do_complete));
      }
    }
  }

  virtual void Notify(typename Protocol::socket_session* p_subject) {
    if (p_subject->GetState() ==
        connected_protocol::state::BaseState<Protocol>::CONNECTED) {
      boost::recursive_mutex::scoped_lock lock_sessions(mutex_);
      auto connecting_it =
          connecting_sessions_.find(p_subject->remote_socket_id);

      if (connecting_it != connecting_sessions_.end()) {
        boost::system::error_code ec;
        connected_sessions_.insert(*connecting_it);
        connecting_sessions_.erase(connecting_it->first);
        Accept(ec);
        // @todo: should ec be swallowed here?
      }

      return;
    }

    if (p_subject->GetState() ==
        connected_protocol::state::BaseState<Protocol>::CLOSED) {
      boost::recursive_mutex::scoped_lock lock_sessions(mutex_);
      connecting_sessions_.erase(p_subject->remote_socket_id);
      connected_sessions_.erase(p_subject->remote_socket_id);
      previous_connected_sessions_.erase(p_subject->remote_socket_id);

      return;
    }
  }

 private:
  SocketSessionPtr GetSession(socket_id_type remote_socket_id) {
    boost::recursive_mutex::scoped_lock lock(mutex_);
    auto connecting_it = connecting_sessions_.find(remote_socket_id);
    if (connecting_it != connecting_sessions_.end()) {
      return connecting_it->second;
    }
    auto connected_it = connected_sessions_.find(remote_socket_id);
    if (connected_it != connected_sessions_.end()) {
      return connected_it->second;
    }
    auto previous_connected_it =
        previous_connected_sessions_.find(remote_socket_id);
    if (previous_connected_it != previous_connected_sessions_.end()) {
      return previous_connected_it->second;
    }

    return nullptr;
  }

  void HandleFirstHandshakePacket(
      ConnectionDatagramPtr p_connection_dgr,
      const NextLayerEndpoint& next_remote_endpoint) {
    if (!p_multiplexer_) {
      return;
    }

    auto& header = p_connection_dgr->header();
    auto& payload = p_connection_dgr->payload();

    header.set_destination_socket(payload.socket_id());
    payload.set_version(0);
    payload.set_socket_type(ConnectionDatagram::Payload::STREAM);
    payload.set_initial_packet_sequence_number(0);
    payload.set_syn_cookie(GetSynCookie(next_remote_endpoint));
    payload.set_maximum_packet_size(Protocol::MTU);
    payload.set_maximum_window_flow_size(Protocol::MAXIMUM_WINDOW_FLOW_SIZE);
    payload.set_socket_id(0);

    p_multiplexer_->AsyncSendControlPacket(
        *p_connection_dgr, next_remote_endpoint,
        [p_connection_dgr](const boost::system::error_code&, std::size_t) {});
  }

  uint32_t GetSynCookie(const NextLayerEndpoint& next_remote_endpoint) {
    // detail namespace used here
    boost::uuids::detail::sha1 sha1;

    uint32_t hash[5];
    std::stringstream text_str;
    text_str << next_remote_endpoint.address().to_string() << ":"
             << boost::chrono::duration_cast<boost::chrono::minutes>(
                    start_time_point_.time_since_epoch())
                    .count();
    std::string text(text_str.str());
    sha1.process_bytes(text.c_str(), text.size());
    sha1.get_digest(hash);

    // Get first 32 bits of hash
    return hash[0];
  }

 private:
  MultiplexerPtr p_multiplexer_;
  boost::recursive_mutex mutex_;
  AcceptOpQueue accept_ops_;
  RemoteSessionsMap connecting_sessions_;
  RemoteSessionsMap connected_sessions_;
  RemoteSessionsMap previous_connected_sessions_;
  bool listening_;
  TimePoint start_time_point_;
};

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_ACCEPTOR_SESSION_H_
