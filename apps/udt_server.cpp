#include <iostream>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <boost/system/error_code.hpp>

#include "udt/connected_protocol/logger/file_log.h"
#include "udt/ip/udt.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Command help : ./udt_server [port]\n";
    return 1;
  }

  using udt_protocol = ip::udt<>;
  using Buffer = std::array<uint8_t, 150000>;
  using SocketPtr = std::shared_ptr<udt_protocol::socket>;
  using ReceiveHandler = std::function<void(const boost::system::error_code&,
                                            std::size_t, SocketPtr)>;
  using AcceptHandler =
      std::function<void(const boost::system::error_code&, SocketPtr)>;

  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;

  Buffer r_buffer2;

  SocketPtr p_socket(std::make_shared<udt_protocol::socket>(io_context));
  udt_protocol::acceptor acceptor(io_context);
  udt_protocol::resolver resolver(io_context);
  udt_protocol::endpoint acceptor_endpoint{udt_protocol::v4(),
                                           (unsigned short)std::stoul(argv[1])};

  AcceptHandler accepted;
  ReceiveHandler received_handler;

  accepted = [&](const boost::system::error_code& ec, SocketPtr p_socket) {
    if (ec) {
      std::cout << "Error on accept : " << ec.value() << " " << ec.message()
                << "\n";
      return;
    }

    std::cout << "Accepted\n";
    boost::asio::async_read(*p_socket, boost::asio::buffer(r_buffer2),
                            boost::bind(received_handler, boost::placeholders::_1, boost::placeholders::_2, p_socket));

    SocketPtr p_new_socket(std::make_shared<udt_protocol::socket>(io_context));
    acceptor.async_accept(*p_new_socket,
                          boost::bind(accepted, boost::placeholders::_1, p_new_socket));
  };

  received_handler = [&](const boost::system::error_code& ec,
                         std::size_t length, SocketPtr p_socket) {
    if (ec) {
      std::cerr << "Error on receive ec : " << ec.value() << " " << ec.message()
                << "\n";
      return;
    }
    std::cout << "Received : " << length << "\n";
    boost::asio::async_read(*p_socket, boost::asio::buffer(r_buffer2),
                            boost::bind(received_handler, boost::placeholders::_1, boost::placeholders::_2, p_socket));
  };

  boost::system::error_code ec;

  acceptor.open(acceptor_endpoint.protocol());
  acceptor.bind(acceptor_endpoint, ec);
  acceptor.listen(100, ec);

  acceptor.async_accept(*p_socket, boost::bind(accepted, boost::placeholders::_1, p_socket));

  std::vector<std::shared_ptr<std::thread>> threads;
  for (uint16_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
    threads.push_back(
        std::make_shared<std::thread>([&io_context]() { io_context.run(); }));
  }
  for (auto& thread : threads) {
    thread->join();
  }
}
