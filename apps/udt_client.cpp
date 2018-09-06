#include <iostream>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <boost/system/error_code.hpp>

#include <udt/ip/udt.h>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Command help : ./udt_client [host] [port]\n";
    return 1;
  }

  using udt_protocol = ip::udt<>;
  using Buffer = std::array<uint8_t, 100000>;
  using SendHandler =
      std::function<void(const boost::system::error_code&, std::size_t)>;
  using ReceiveHandler =
      std::function<void(const boost::system::error_code&, std::size_t)>;
  using ConnectHandler = std::function<void(const boost::system::error_code&)>;

  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;

  Buffer buffer1;
  Buffer r_buffer1;

  udt_protocol::socket socket(io_context);
  udt_protocol::resolver resolver(io_context);

  udt_protocol::resolver::query client_udt_query(argv[1], argv[2]);

  auto remote_endpoint_it = resolver.resolve(client_udt_query, resolve_ec);

  if (resolve_ec) {
    std::cerr << "Wrong arguments provided\n";
    return 1;
  }

  udt_protocol::endpoint remote_endpoint(*remote_endpoint_it);

  ConnectHandler connected;
  SendHandler sent_handler;
  ReceiveHandler received_handler;

  connected = [&](const boost::system::error_code& ec) {
    if (!ec) {
      std::cout << "Connected";
      boost::asio::async_write(socket, boost::asio::buffer(buffer1),
                               sent_handler);
    } else {
      std::cout << "Error on connection : " << ec.value() << " " << ec.message()
                << "\n";
    }
  };

  sent_handler = [&](const boost::system::error_code& ec, std::size_t length) {
    if (ec) {
      std::cout << "Error on sent : " << ec.value() << " " << ec.message()
                << "\n";
      return;
    }

    std::cout << "Sent : " << length << "\n";
    boost::asio::async_write(socket, boost::asio::buffer(r_buffer1),
                             sent_handler);
  };

  socket.async_connect(remote_endpoint, connected);

  std::vector<std::shared_ptr<std::thread>> threads;
  for (uint16_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
    threads.push_back(
        std::make_shared<std::thread>([&io_context]() { io_context.run(); }));
  }
  for (auto& thread : threads) {
    thread->join();
  }
}
