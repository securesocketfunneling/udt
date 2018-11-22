#ifndef UDT_CONNECTED_PROTOCOL_IO_WRITE_OP_H_
#define UDT_CONNECTED_PROTOCOL_IO_WRITE_OP_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif  // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/buffer.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/fenced_block.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/asio/error.hpp>

#include "buffers.h"
#include "op.h"

#include <boost/asio/detail/push_options.hpp>

namespace connected_protocol {
namespace io {

class basic_pending_write_operation : public basic_pending_sized_io_operation {
 protected:
  using const_buffers_func_type =
      io::fixed_const_buffer_sequence (*)(basic_pending_write_operation*);

 public:
  io::fixed_const_buffer_sequence const_buffers() {
    return const_buffers_func_(this);
  }

 protected:
  basic_pending_write_operation(
      basic_pending_sized_io_operation::func_type func,
      const_buffers_func_type const_buffers_func)
      : basic_pending_sized_io_operation(func),
        const_buffers_func_(const_buffers_func) {}

 protected:
  const_buffers_func_type const_buffers_func_;
};

/// Class to store write operations
/**
 * @tparam Handler The type of the handler to be called upon completion
 */
template <class ConstBufferSequence, class Handler>
class pending_write_operation : public basic_pending_write_operation {
 public:
  BOOST_ASIO_DEFINE_HANDLER_PTR(pending_write_operation);

  /// Constructor
  /**
   * @param p_endpoint The remote endpoint
   * @param handler The handler to call upon completion
   */
  pending_write_operation(ConstBufferSequence buffers, Handler handler)
      : basic_pending_write_operation(
            &pending_write_operation::do_complete,
            &pending_write_operation::do_const_buffers),
        buffers_(std::move(buffers)),
        handler_(std::move(handler)) {}

  /// Implementation of the completion callback
  /**
   * @param base A pointer to the base class
   * @param destroy A boolean to decide if the op should be destroyed
   * @param result_ec The error_code of the operation
   */
  static void do_complete(basic_pending_sized_io_operation* base, bool destroy,
                          const boost::system::error_code& result_ec,
                          std::size_t length) {
    boost::system::error_code ec(result_ec);

    // take ownership of the operation object
    pending_write_operation* o(static_cast<pending_write_operation*>(base));

    ptr p = {boost::asio::detail::addressof(o->handler_), o, o};

    BOOST_ASIO_HANDLER_COMPLETION((o));

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    boost::asio::detail::binder2<Handler, boost::system::error_code,
                                 std::size_t>
        handler(o->handler_, ec, length);
    p.h = boost::asio::detail::addressof(handler.handler_);
    p.reset();

    // Make the upcall if required.
    if (!destroy) {
      boost::asio::detail::fenced_block b(
          boost::asio::detail::fenced_block::half);
      BOOST_ASIO_HANDLER_INVOCATION_BEGIN((handler.arg1_, handler.arg2_));
      boost_asio_handler_invoke_helpers::invoke(handler, handler.handler_);
      BOOST_ASIO_HANDLER_INVOCATION_END;
    }
  }

  static io::fixed_const_buffer_sequence do_const_buffers(
      basic_pending_write_operation* base) {
    pending_write_operation* o(static_cast<pending_write_operation*>(base));
    return o->buffers_;
  }

 private:
  ConstBufferSequence buffers_;
  Handler handler_;
};

}  // namespace io
}  // namespace connected_protocol

#include <boost/asio/detail/pop_options.hpp>

#endif  // UDT_CONNECTED_PROTOCOL_IO_WRITE_OP_H_
