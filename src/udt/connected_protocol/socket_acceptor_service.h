#ifndef UDT_CONNECTED_PROTOCOL_SOCKET_ACCEPTOR_SERVICE_H_
#define UDT_CONNECTED_PROTOCOL_SOCKET_ACCEPTOR_SERVICE_H_

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_service.hpp>

#include <boost/thread/thread.hpp>
#include <boost/system/error_code.hpp>

#include "udt/common/error/error.h"

#include "udt/connected_protocol/io/accept_op.h"

namespace connected_protocol {

#include <boost/asio/detail/push_options.hpp>

template <class Prococol>
class socket_acceptor_service : public boost::asio::detail::service_base<
                                    socket_acceptor_service<Prococol>> {
 public:
  typedef Prococol protocol_type;
  typedef typename protocol_type::endpoint endpoint_type;
  typedef std::shared_ptr<endpoint_type> p_endpoint_type;
  typedef typename protocol_type::resolver resolver_type;

  typedef typename protocol_type::next_layer_protocol::socket next_socket_type;
  typedef std::shared_ptr<next_socket_type> p_next_socket_type;
  typedef typename protocol_type::acceptor_session acceptor_session_type;
  typedef std::shared_ptr<acceptor_session_type> p_acceptor_session_type;
  typedef typename protocol_type::multiplexer multiplexer;
  typedef std::shared_ptr<multiplexer> p_multiplexer_type;

  typedef p_acceptor_session_type implementation_type;

  typedef implementation_type& native_handle_type;
  typedef native_handle_type native_type;

 public:
  explicit socket_acceptor_service(boost::asio::io_service& io_service)
      : boost::asio::detail::service_base<socket_acceptor_service>(io_service) {
  }

  virtual ~socket_acceptor_service() {}

  void construct(implementation_type& impl) {}

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
    if (!impl) {
      impl = std::make_shared<acceptor_session_type>();
      ec.assign(::common::error::success,
                ::common::error::get_error_category());
    } else {
      ec.assign(::common::error::device_or_resource_busy,
                ::common::error::get_error_category());
    }
    return ec;
  }

  boost::system::error_code assign(implementation_type& impl,
                                   const protocol_type& protocol,
                                   const native_handle_type& native_socket,
                                   boost::system::error_code& ec) {
    impl = native_socket;
    ec.assign(::common::error::success, ::common::error::get_error_category());
    return ec;
  }

  bool is_open(const implementation_type& impl) const {
    return impl != nullptr;
  }

  endpoint_type local_endpoint(const implementation_type& impl,
                               boost::system::error_code& ec) const {
    if (!is_open(impl)) {
      ec.assign(::common::error::broken_pipe,
                ::common::error::get_error_category());

      return endpoint_type();
    }

    ec.assign(::common::error::success, ::common::error::get_error_category());
    return endpoint_type(0, impl->next_local_endpoint(ec));
  }

  boost::system::error_code close(implementation_type& impl,
                                  boost::system::error_code& ec) {
    if (!is_open(impl)) {
      ec.assign(::common::error::broken_pipe,
                ::common::error::get_error_category());
      return ec;
    }

    impl->Close();

    impl.reset();

    return ec;
  }

  native_type native(implementation_type& impl) { return impl; }

  native_handle_type native_handle(implementation_type& impl) { return impl; }

  template <typename SettableSocketOption>
  boost::system::error_code set_option(implementation_type& impl,
                                       const SettableSocketOption& option,
                                       boost::system::error_code& ec) {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return ec;
  }

  template <typename GettableSocketOption>
  boost::system::error_code get_option(const implementation_type& impl,
                                       GettableSocketOption& option,
                                       boost::system::error_code& ec) const {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return ec;
  }

  template <typename IoControlCommand>
  boost::system::error_code io_control(implementation_type& impl,
                                       IoControlCommand& command,
                                       boost::system::error_code& ec) {
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return ec;
  }

  boost::system::error_code bind(implementation_type& impl,
                                 const endpoint_type& endpoint,
                                 boost::system::error_code& ec) {
    if (!is_open(impl)) {
      ec.assign(::common::error::broken_pipe,
                ::common::error::get_error_category());

      return ec;
    }

    p_multiplexer_type p_multiplexer =
        protocol_type::multiplexers_manager_.CreateMultiplexer(
            this->get_io_service(), endpoint.next_layer_endpoint(), ec);
    if (ec) {
      return ec;
    }

    p_multiplexer->SetAcceptor(ec, impl);

    return ec;
  }

  boost::system::error_code listen(implementation_type& impl, int backlog,
                                   boost::system::error_code& ec) {
    impl->Listen();
    return ec;
  }

  template <typename Protocol1, typename SocketService>
  boost::system::error_code accept(
      implementation_type& impl,
      boost::asio::basic_socket<Protocol1, SocketService>& peer,
      endpoint_type* p_peer_endpoint, boost::system::error_code& ec,
      typename std::enable_if<boost::thread_detail::is_convertible<
          protocol_type, Protocol1>::value>::type* = 0) {
    // todo : accept synchronously
    ec.assign(::common::error::function_not_supported,
              ::common::error::get_error_category());
    return ec;
  }

  template <typename Protocol1, typename SocketService, typename AcceptHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(AcceptHandler, void(boost::system::error_code))
      async_accept(implementation_type& impl,
                   boost::asio::basic_socket<Protocol1, SocketService>& peer,
                   endpoint_type* p_peer_endpoint,
                   BOOST_ASIO_MOVE_ARG(AcceptHandler) handler,
                   typename std::enable_if<boost::thread_detail::is_convertible<
                       protocol_type, Protocol1>::value>::type* = 0) {
    boost::asio::detail::async_result_init<AcceptHandler,
                                           void(boost::system::error_code)>
        init(BOOST_ASIO_MOVE_CAST(AcceptHandler)(handler));

    if (!is_open(impl)) {
      this->get_io_service().post(
          boost::asio::detail::binder1<decltype(init.handler),
                                       boost::system::error_code>(
              init.handler, boost::system::error_code(
                                ::common::error::broken_pipe,
                                ::common::error::get_error_category())));
      return init.result.get();
    }

    typedef io::pending_accept_operation<decltype(init.handler), protocol_type>
        op;
    typename op::ptr p = {
        boost::asio::detail::addressof(init.handler),
        boost_asio_handler_alloc_helpers::allocate(sizeof(op), init.handler),
        0};

    p.p = new (p.v) op(peer, nullptr, init.handler);

    impl->PushAcceptOp(p.p);

    p.v = p.p = 0;

    return init.result.get();
  }

 private:
  void shutdown_service() {}
};

#include <boost/asio/detail/pop_options.hpp>

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_SOCKET_ACCEPTOR_SERVICE_H_
