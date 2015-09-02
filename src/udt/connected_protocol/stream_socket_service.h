#ifndef UDT_CONNECTED_PROTOCOL_STREAM_SOCKET_SERVICE_H_
#define UDT_CONNECTED_PROTOCOL_STREAM_SOCKET_SERVICE_H_

#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/socket_base.hpp>

#include "udt/common/error/error.h"

#include "udt/connected_protocol/io/connect_op.h"
#include "udt/connected_protocol/io/write_op.h"
#include "udt/connected_protocol/io/read_op.h"

#include "udt/connected_protocol/state/connecting_state.h"

namespace connected_protocol {

#include <boost/asio/detail/push_options.hpp>

template <class Prococol>
class stream_socket_service : public boost::asio::detail::service_base<
                                  stream_socket_service<Prococol>> {
 public:
  typedef Prococol protocol_type;

  typedef std::shared_ptr<typename protocol_type::socket_session>
      implementation_type;

  typedef typename protocol_type::endpoint endpoint_type;
  typedef std::shared_ptr<endpoint_type> p_endpoint_type;
  typedef typename protocol_type::resolver resolver_type;

  typedef implementation_type& native_handle_type;
  typedef native_handle_type native_type;

 private:
  typedef typename protocol_type::next_layer_protocol::socket next_socket_type;
  typedef std::shared_ptr<next_socket_type> p_next_socket_type;
  typedef
      typename protocol_type::next_layer_protocol::endpoint next_endpoint_type;
  typedef typename protocol_type::socket_session socket_session_type;
  typedef std::shared_ptr<socket_session_type> p_socket_session_type;
  typedef typename protocol_type::multiplexer multiplexer;
  typedef std::shared_ptr<multiplexer> p_multiplexer_type;
  typedef typename protocol_type::ShutdownDatagram shutdown_datagram_type;

  typedef typename connected_protocol::state::ConnectingState<protocol_type>
      ConnectingState;
  typedef typename connected_protocol::state::ClosedState<protocol_type>
      ClosedState;

 public:
  explicit stream_socket_service(boost::asio::io_service& io_service)
      : boost::asio::detail::service_base<stream_socket_service>(io_service) {}

  virtual ~stream_socket_service() {}

  void construct(implementation_type& impl) { impl.reset(); }

  void destroy(implementation_type& impl) { impl.reset(); }

  void move_construct(implementation_type& impl, implementation_type& other) {
    impl = std::move(other);
  }

  void move_assign(implementation_type& impl, implementation_type& other) {
    impl = std::move(other);
  }

  boost::system::error_code open(implementation_type& impl,
                                 const protocol_type& protocol,
                                 boost::system::error_code& ec) {
    ec.assign(::common::error::success, ::common::error::get_error_category());
    return ec;
  }

  boost::system::error_code assign(implementation_type& impl,
                                   const protocol_type& protocol,
                                   const native_handle_type& native_socket,
                                   boost::system::error_code& ec) {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return ec;
  }

  bool is_open(const implementation_type& impl) const {
    return impl != nullptr;
  }

  endpoint_type remote_endpoint(const implementation_type& impl,
                                boost::system::error_code& ec) const {
    if (impl && impl->next_remote_endpoint() != next_endpoint_type()) {
      ec.assign(::common::error::success,
                ::common::error::get_error_category());
      return endpoint_type(impl->remote_socket_id,
                           impl->next_remote_endpoint());
    } else {
      ec.assign(::common::error::no_link,
                ::common::error::get_error_category());
      return endpoint_type();
    }
  }

  endpoint_type local_endpoint(const implementation_type& impl,
                               boost::system::error_code& ec) const {
    if (impl && impl->next_local_endpoint() != next_endpoint_type()) {
      ec.assign(::common::error::success,
                ::common::error::get_error_category());
      return endpoint_type(impl->socket_id, impl->next_local_endpoint());
    } else {
      ec.assign(::common::error::no_link,
                ::common::error::get_error_category());
      return endpoint_type();
    }
  }

  boost::system::error_code close(implementation_type& impl,
                                  boost::system::error_code& ec) {
    if (impl) {
      impl->Close();
    }

    impl.reset();
    ec.assign(::common::error::success, ::common::error::get_error_category());
    return ec;
  }

  native_type native(implementation_type& impl) { return impl; }

  native_handle_type native_handle(implementation_type& impl) { return impl; }

  bool at_mark(const implementation_type& impl,
               boost::system::error_code& ec) const {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return false;
  }

  std::size_t available(const implementation_type& impl,
                        boost::system::error_code& ec) const {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return 0;
  }

  boost::system::error_code cancel(implementation_type& impl,
                                   boost::system::error_code& ec) {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return ec;
  }

  boost::system::error_code bind(implementation_type& impl,
                                 const endpoint_type& local_endpoint,
                                 boost::system::error_code& ec) {
    if (impl) {
      ec.assign(::common::error::device_or_resource_busy,
                ::common::error::get_error_category());
      return;
    }

    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());

    return ec;
  }

  boost::system::error_code connect(implementation_type& impl,
                                    const endpoint_type& peer_endpoint,
                                    boost::system::error_code& ec) {
    // todo : connect synchronously
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());

    return ec;
  }

  template <typename ConnectHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler, void(boost::system::error_code))
      async_connect(implementation_type& impl,
                    const endpoint_type& peer_endpoint,
                    BOOST_ASIO_MOVE_ARG(ConnectHandler) handler) {
    boost::asio::detail::async_result_init<ConnectHandler,
                                           void(boost::system::error_code)>
        init(std::forward<ConnectHandler>(handler));

    boost::system::error_code ec;
    p_multiplexer_type p_multiplexer =
        protocol_type::multiplexers_manager_.CreateMultiplexer(
            this->get_io_service(),
            typename protocol_type::next_layer_protocol::endpoint(), ec);

    if (ec) {
      this->get_io_service().post(boost::asio::detail::binder1<
          decltype(init.handler), boost::system::error_code>(init.handler, ec));
      return init.result.get();
    }

    impl = p_multiplexer->CreateSocketSession(
        ec, peer_endpoint.next_layer_endpoint());
    if (ec) {
      this->get_io_service().post(boost::asio::detail::binder1<
          decltype(init.handler), boost::system::error_code>(init.handler, ec));
      return init.result.get();
    }

    typedef io::pending_connect_operation<decltype(init.handler), protocol_type>
        connect_op_type;
    typename connect_op_type::ptr p = {
        boost::asio::detail::addressof(init.handler),
        boost_asio_handler_alloc_helpers::allocate(sizeof(connect_op_type),
                                                   init.handler),
        0};

    // todo move op in state
    p.p = new (p.v) connect_op_type(init.handler);
    impl->connection_op = p.p;
    p.v = p.p = 0;

    impl->ChangeState(ConnectingState::Create(impl));

    return init.result.get();
  }

  /// Set a socket option.
  template <typename SettableSocketOption>
  boost::system::error_code set_option(implementation_type& impl,
                                       const SettableSocketOption& option,
                                       boost::system::error_code& ec) {
    if (option.name(protocol_type::v4()) == protocol_type::TIMEOUT_DELAY) {
      boost::recursive_mutex::scoped_lock lock(impl->mutex);
      impl->timeout_delay = option.value();
    }

    return ec;
  }

  /// Get a socket option.
  template <typename GettableSocketOption>
  boost::system::error_code get_option(const implementation_type& impl,
                                       GettableSocketOption& option,
                                       boost::system::error_code& ec) const {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());

    return ec;
  }

  template <typename ConstBufferSequence>
  std::size_t send(implementation_type& impl,
                   const ConstBufferSequence& buffers,
                   boost::asio::socket_base::message_flags flags,
                   boost::system::error_code& ec) {
    // todo : send synchronously
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());

    return ec;
  }

  template <typename ConstBufferSequence, typename WriteHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
                                void(boost::system::error_code, std::size_t))
      async_send(implementation_type& impl, const ConstBufferSequence& buffers,
                 boost::asio::socket_base::message_flags flags,
                 BOOST_ASIO_MOVE_ARG(WriteHandler) handler) {
    boost::asio::detail::async_result_init<
        WriteHandler, void(boost::system::error_code, std::size_t)>
        init(std::forward<WriteHandler>(handler));

    if (!impl) {
      this->get_io_service().post(
          boost::asio::detail::binder2<decltype(init.handler),
                                       boost::system::error_code, std::size_t>(
              init.handler,
              boost::system::error_code(::common::error::not_connected,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    if (boost::asio::buffer_size(buffers) == 0) {
      this->get_io_service().post(
          boost::asio::detail::binder2<decltype(init.handler),
                                       boost::system::error_code, std::size_t>(
              init.handler,
              boost::system::error_code(::common::error::success,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    typedef io::pending_write_operation<ConstBufferSequence,
                                        decltype(init.handler)> write_op_type;
    typename write_op_type::ptr p = {
        boost::asio::detail::addressof(init.handler),
        boost_asio_handler_alloc_helpers::allocate(sizeof(write_op_type),
                                                   init.handler),
        0};

    p.p = new (p.v) write_op_type(buffers, std::move(init.handler));

    impl->PushWriteOp(p.p);

    p.v = p.p = 0;

    return init.result.get();
  }

  template <typename MutableBufferSequence>
  std::size_t receive(implementation_type& impl,
                      const MutableBufferSequence& buffers,
                      boost::asio::socket_base::message_flags flags,
                      boost::system::error_code& ec) {
    // todo : receive synchronously
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());

    return ec;
  }

  template <typename MutableBufferSequence, typename ReadHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
                                void(boost::system::error_code, std::size_t))
      async_receive(implementation_type& impl,
                    const MutableBufferSequence& buffers,
                    boost::asio::socket_base::message_flags flags,
                    BOOST_ASIO_MOVE_ARG(ReadHandler) handler) {
    boost::asio::detail::async_result_init<
        ReadHandler, void(boost::system::error_code, std::size_t)>
        init(std::forward<ReadHandler>(handler));

    if (!impl) {
      this->get_io_service().post(
          boost::asio::detail::binder2<decltype(init.handler),
                                       boost::system::error_code, std::size_t>(
              init.handler,
              boost::system::error_code(::common::error::not_connected,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    if (boost::asio::buffer_size(buffers) == 0) {
      this->get_io_service().post(
          boost::asio::detail::binder2<decltype(init.handler),
                                       boost::system::error_code, std::size_t>(
              init.handler,
              boost::system::error_code(::common::error::success,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    typedef io::pending_stream_read_operation<MutableBufferSequence,
                                              decltype(init.handler),
                                              protocol_type> read_op_type;
    typename read_op_type::ptr p = {
        boost::asio::detail::addressof(init.handler),
        boost_asio_handler_alloc_helpers::allocate(sizeof(read_op_type),
                                                   init.handler),
        0};

    p.p = new (p.v) read_op_type(buffers, std::move(init.handler));

    impl->PushReadOp(p.p);

    p.v = p.p = 0;

    return init.result.get();
  }

  boost::system::error_code shutdown(
      implementation_type& impl, boost::asio::socket_base::shutdown_type what,
      boost::system::error_code& ec) {
    ec.assign(::common::error::success, ::common::error::get_error_category());
    return ec;
  }

 private:
  void shutdown_service() {}
};

#include <boost/asio/detail/pop_options.hpp>

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STREAM_SOCKET_SERVICE_H_
