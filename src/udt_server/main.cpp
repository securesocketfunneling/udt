#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>

#include "udt/connected_protocol/logger/file_log.h"
#include "udt/ip/udt.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    BOOST_LOG_TRIVIAL(error) << "Command help : ./udt_server [port]";
    return 1;
  }

  typedef ip::udt<> udt_protocol;
  typedef std::array<uint8_t, 150000> Buffer;
  typedef std::shared_ptr<udt_protocol::socket> p_socket_type;
  typedef std::function<void(const boost::system::error_code&, std::size_t,
                             p_socket_type)> ReceiveHandler;
  typedef std::function<void(const boost::system::error_code&, p_socket_type)>
      AcceptHandler;

  boost::asio::io_service io_service;
  boost::system::error_code resolve_ec;

  Buffer r_buffer2;

  p_socket_type p_socket(std::make_shared<udt_protocol::socket>(io_service));
  udt_protocol::acceptor acceptor(io_service);
  udt_protocol::resolver resolver(io_service);

  udt_protocol::resolver::query acceptor_udt_query(boost::asio::ip::udp::v4(),
                                                   argv[1]);
  auto acceptor_endpoint_it = resolver.resolve(acceptor_udt_query, resolve_ec);

  if (resolve_ec) {
    BOOST_LOG_TRIVIAL(error) << "Wrong argument provided" << std::endl;
    return 1;
  }

  udt_protocol::endpoint acceptor_endpoint(*acceptor_endpoint_it);

  AcceptHandler accepted;
  ReceiveHandler received_handler;

  accepted = [&](const boost::system::error_code& ec, p_socket_type p_socket) {
    if (ec) {
      BOOST_LOG_TRIVIAL(trace) << "Error on accept : " << ec.value() << " "
                               << ec.message();
      return;
    }

    BOOST_LOG_TRIVIAL(trace) << "Accepted";
    boost::asio::async_read(*p_socket, boost::asio::buffer(r_buffer2),
                            boost::bind(received_handler, _1, _2, p_socket));

    p_socket_type p_new_socket(
        std::make_shared<udt_protocol::socket>(io_service));
    acceptor.async_accept(*p_new_socket,
                          boost::bind(accepted, _1, p_new_socket));
  };

  received_handler = [&](const boost::system::error_code& ec,
                         std::size_t length, p_socket_type p_socket) {
    if (ec) {
      BOOST_LOG_TRIVIAL(trace) << "Error on receive ec : " << ec.value() << " "
                               << ec.message();
      return;
    }

    boost::asio::async_read(*p_socket, boost::asio::buffer(r_buffer2),
                            boost::bind(received_handler, _1, _2, p_socket));
  };

  boost::system::error_code ec;

  acceptor.open();
  acceptor.bind(acceptor_endpoint, ec);
  acceptor.listen(100, ec);

  acceptor.async_accept(*p_socket, boost::bind(accepted, _1, p_socket));

  boost::thread_group threads;
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_service]() { io_service.run(); });
  }
  threads.join_all();
}