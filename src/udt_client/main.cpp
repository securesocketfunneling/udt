#include <boost/asio/io_service.hpp>

#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>

#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "udt/ip/udt.h"
#include "udt/connected_protocol/logger/file_log.h"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    BOOST_LOG_TRIVIAL(error) << "Command help : ./udt_client [host] [port]";
    return 1;
  }

  typedef ip::udt<> udt_protocol;
  typedef std::array<uint8_t, 100000> Buffer;
  typedef std::function<void(const boost::system::error_code&, std::size_t)>
      SendHandler;
  typedef std::function<void(const boost::system::error_code&, std::size_t)>
      ReceiveHandler;
  typedef std::function<void(const boost::system::error_code&)> ConnectHandler;

  boost::asio::io_service io_service;
  boost::system::error_code resolve_ec;

  Buffer buffer1;
  Buffer r_buffer1;

  udt_protocol::socket socket(io_service);
  udt_protocol::resolver resolver(io_service);

  udt_protocol::resolver::query client_udt_query(argv[1], argv[2]);

  auto remote_endpoint_it = resolver.resolve(client_udt_query, resolve_ec);

  if (resolve_ec) {
    BOOST_LOG_TRIVIAL(error) << "Wrong arguments provided" << std::endl;
    return 1;
  }

  udt_protocol::endpoint remote_endpoint(*remote_endpoint_it);

  ConnectHandler connected;
  SendHandler sent_handler;
  ReceiveHandler received_handler;

  connected = [&](const boost::system::error_code& ec) {
    if (!ec) {
      BOOST_LOG_TRIVIAL(trace) << "Connected" << std::endl;
      boost::asio::async_write(socket, boost::asio::buffer(buffer1),
                               sent_handler);
    }
  };

  sent_handler = [&](const boost::system::error_code& ec, std::size_t length) {
    if (ec) {
      BOOST_LOG_TRIVIAL(trace) << "sent ec : " << ec.value() << " "
                               << ec.message();
      return;
    }

    boost::asio::async_write(socket, boost::asio::buffer(r_buffer1),
                             sent_handler);
  };

  socket.async_connect(remote_endpoint, connected);

  boost::thread_group threads;
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_service]() { io_service.run(); });
  }
  threads.join_all();
}