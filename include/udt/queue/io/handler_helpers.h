#ifndef UDT_QUEUE_IO_HANDLER_HELPERS_H_
#define UDT_QUEUE_IO_HANDLER_HELPERS_H_

#include <boost/asio/detail/bind_handler.hpp>

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif  // defined(_MSC_VER) && (_MSC_VER >= 1200)

namespace queue {
namespace io {

template <class Poster, class Handler, class... Args>
void PostHandler(Poster& poster, Handler&& handler, Args&&... args) {
  poster.post(boost::asio::detail::bind_handler(std::forward<Handler>(handler),
                                                std::forward<Args>(args)...));
}

}  // io
}  // queue

#endif  // UDT_QUEUE_IO_HANDLER_HELPERS_H_
