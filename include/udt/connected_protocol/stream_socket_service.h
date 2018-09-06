#ifndef UDT_CONNECTED_PROTOCOL_STREAM_SOCKET_SERVICE_H_
#define UDT_CONNECTED_PROTOCOL_STREAM_SOCKET_SERVICE_H_

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/use_future.hpp>

#include "../common/error/error.h"

#include "io/connect_op.h"
#include "io/write_op.h"
#include "io/read_op.h"

#include "state/connecting_state.h"

namespace connected_protocol {

#include <boost/asio/detail/push_options.hpp>

template <class Protocol>
class stream_socket_service : public boost::asio::detail::service_base<
                                  stream_socket_service<Protocol>> {
 public:
  using protocol_type = Protocol;

  struct implementation_type {
    implementation_type()
        : p_multiplexer(nullptr), p_session(nullptr), timeout(60) {}

    std::shared_ptr<typename protocol_type::multiplexer> p_multiplexer;
    std::shared_ptr<typename protocol_type::socket_session> p_session;
    int timeout;
  };

  using endpoint_type = typename protocol_type::endpoint;

  using native_handle_type = implementation_type&;
  using native_type = native_handle_type;

 private:
  using next_endpoint_type =
      typename protocol_type::next_layer_protocol::endpoint;
  using multiplexer = typename protocol_type::multiplexer;

  using ConnectingState =
      typename connected_protocol::state::ConnectingState<protocol_type>;
  using ClosedState =
      typename connected_protocol::state::ClosedState<protocol_type>;

 public:
  explicit stream_socket_service(boost::asio::io_context& io_context)
      : boost::asio::detail::service_base<stream_socket_service>(io_context) {}

  virtual ~stream_socket_service() = default;

  void construct(implementation_type& impl) {
    impl.p_session.reset();
    impl.p_multiplexer.reset();
  }

  void destroy(implementation_type& impl) {
    impl.p_session.reset();
    impl.p_multiplexer.reset();
  }

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

  bool is_open(const implementation_type& impl) const {
    return impl.p_multiplexer != nullptr || impl.p_session != nullptr;
  }

  endpoint_type remote_endpoint(const implementation_type& impl,
                                boost::system::error_code& ec) const {
    if (impl.p_session &&
        impl.p_session->next_remote_endpoint() != next_endpoint_type()) {
      ec.assign(::common::error::success,
                ::common::error::get_error_category());
      return endpoint_type(impl.p_session->remote_socket_id(),
                           impl.p_session->next_remote_endpoint());
    } else {
      ec.assign(::common::error::no_link,
                ::common::error::get_error_category());
      return endpoint_type();
    }
  }

  endpoint_type local_endpoint(const implementation_type& impl,
                               boost::system::error_code& ec) const {
    if (impl.p_session &&
        impl.p_session->next_local_endpoint() != next_endpoint_type()) {
      ec.assign(::common::error::success,
                ::common::error::get_error_category());
      return endpoint_type(impl.p_session->socket_id(),
                           impl.p_session->next_local_endpoint());
    } else {
      ec.assign(::common::error::no_link,
                ::common::error::get_error_category());
      return endpoint_type();
    }
  }

  boost::system::error_code close(implementation_type& impl,
                                  boost::system::error_code& ec) {
    if (impl.p_session) {
      impl.p_session->Close();
    }

    impl.p_session.reset();
    impl.p_multiplexer.reset();
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
    if (impl.p_session || impl.p_multiplexer) {
      ec.assign(::common::error::device_or_resource_busy,
                ::common::error::get_error_category());
      return ec;
    }

    impl.p_multiplexer = protocol_type::multiplexers_manager_.GetMultiplexer(
        this->get_io_context(), local_endpoint.next_layer_endpoint(), ec);

    return ec;
  }

  boost::system::error_code connect(implementation_type& impl,
                                    const endpoint_type& peer_endpoint,
                                    boost::system::error_code& ec) {
    try {
      ec.clear();
      auto future_value =
          async_connect(impl, peer_endpoint, boost::asio::use_future);
      future_value.get();
      ec.assign(::common::error::success,
                ::common::error::get_error_category());
    } catch (const std::system_error& e) {
      ec.assign(e.code().value(), ::common::error::get_error_category());
    }
    return ec;
  }


  template <typename ConnectHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler, void(boost::system::error_code))
      async_connect(implementation_type& impl,
                    const endpoint_type& peer_endpoint,
                    BOOST_ASIO_MOVE_ARG(ConnectHandler) handler) {
      boost::asio::async_completion<ConnectHandler,
                                    void(boost::system::error_code)> init(handler);

    boost::system::error_code ec;
    if (!impl.p_multiplexer) {
      impl.p_multiplexer = protocol_type::multiplexers_manager_.GetMultiplexer(
          this->get_io_context(),
          typename protocol_type::next_layer_protocol::endpoint(), ec);

      if (ec) {
        this->get_io_context().post(
            boost::asio::detail::binder1<ConnectHandler,
                                         boost::system::error_code>(
                handler, ec));
        return init.result.get();
      }
    }

    impl.p_session = impl.p_multiplexer->CreateSocketSession(
        ec, peer_endpoint.next_layer_endpoint());
    if (ec) {
      this->get_io_context().post(boost::asio::detail::binder1<
          ConnectHandler, boost::system::error_code>(handler, ec));
      return init.result.get();
    }

    typedef io::pending_connect_operation<ConnectHandler, protocol_type> connect_op_type;
    typename connect_op_type::ptr p = {
        boost::asio::detail::addressof(handler),
        connect_op_type::ptr::allocate(handler),
        0};

    p.p = new (p.v) connect_op_type(handler);
    impl.p_session->ChangeState(ConnectingState::Create(impl.p_session, p.p));
    p.v = p.p = 0;

    return init.result.get();
  }

  /// Set a socket option.
  template <typename SettableSocketOption>
  boost::system::error_code set_option(implementation_type& impl,
                                       const SettableSocketOption& option,
                                       boost::system::error_code& ec) {
// NOTE: Don't understand this code.
//    if (!impl.p_session) {
//      impl.timeout = option.value();
//      ec.assign(::common::error::success,
//                ::common::error::get_error_category());
//      return ec;
//    }

    if (option.name(protocol_type::v4()) == protocol_type::TIMEOUT_DELAY) {
      impl.p_session->set_timeout_delay((uint32_t)(*option.data(protocol_type::v4())));
    } else {
      ec.assign(::common::error::function_not_supported,
                ::common::error::get_error_category());
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
    try {
      ec.clear();
      auto future_value =
          async_send(impl, buffers, flags, boost::asio::use_future);
      return future_value.get();
    } catch (const std::system_error& e) {
      ec.assign(e.code().value(), ::common::error::get_error_category());
      return 0;
    }
  }

  template <typename ConstBufferSequence, typename WriteHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
                                void(boost::system::error_code, std::size_t))
      async_send(implementation_type& impl, const ConstBufferSequence& buffers,
                 boost::asio::socket_base::message_flags flags,
                 BOOST_ASIO_MOVE_ARG(WriteHandler) handler) {
    boost::asio::async_completion<
        WriteHandler, void(boost::system::error_code, std::size_t)>
        init(handler);

    if (!impl.p_session) {
      this->get_io_context().post(
          boost::asio::detail::binder2<WriteHandler,
                                       boost::system::error_code, std::size_t>(
              handler,
              boost::system::error_code(::common::error::not_connected,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    if (boost::asio::buffer_size(buffers) == 0) {
      this->get_io_context().post(
          boost::asio::detail::binder2<WriteHandler,
                                       boost::system::error_code, std::size_t>(
              handler,
              boost::system::error_code(::common::error::success,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    using write_op_type = io::pending_write_operation<ConstBufferSequence, WriteHandler>;
    typename write_op_type::ptr p = {
        boost::asio::detail::addressof(handler),
        write_op_type::ptr::allocate(handler),
        0};

    p.p = new (p.v) write_op_type(buffers, handler);

    impl.p_session->PushWriteOp(p.p);

    p.v = p.p = 0;

    return init.result.get();
  }

  template <typename MutableBufferSequence>
  std::size_t receive(implementation_type& impl,
                      const MutableBufferSequence& buffers,
                      boost::asio::socket_base::message_flags flags,
                      boost::system::error_code& ec) {
    try {
      ec.clear();
      auto future_value =
          async_receive(impl, buffers, flags, boost::asio::use_future);
      return future_value.get();
    } catch (const std::system_error& e) {
      ec.assign(e.code().value(), ::common::error::get_error_category());
      return 0;
    }
  }

  template <typename MutableBufferSequence, typename ReadHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
                                void(boost::system::error_code, std::size_t))
      async_receive(implementation_type& impl,
                    const MutableBufferSequence& buffers,
                    boost::asio::socket_base::message_flags flags,
                    BOOST_ASIO_MOVE_ARG(ReadHandler) handler) {
    boost::asio::async_completion<
        ReadHandler, void(boost::system::error_code, std::size_t)>
        init(handler);

    if (!impl.p_session) {
      this->get_io_context().post(
          boost::asio::detail::binder2<ReadHandler,
                                       boost::system::error_code, std::size_t>(
              handler,
              boost::system::error_code(::common::error::not_connected,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    if (boost::asio::buffer_size(buffers) == 0) {
      this->get_io_context().post(
          boost::asio::detail::binder2<ReadHandler,
                                       boost::system::error_code, std::size_t>(
              handler,
              boost::system::error_code(::common::error::success,
                                        ::common::error::get_error_category()),
              0));
      return init.result.get();
    }

    using read_op_type = io::pending_stream_read_operation<
        MutableBufferSequence, ReadHandler, protocol_type>;
    typename read_op_type::ptr p = {
        boost::asio::detail::addressof(handler),
        read_op_type::ptr::allocate(handler),
        0};

    p.p = new (p.v) read_op_type(buffers, handler);

    impl.p_session->PushReadOp(p.p);

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
  void shutdown() {}
};

#include <boost/asio/detail/pop_options.hpp>

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_STREAM_SOCKET_SERVICE_H_
