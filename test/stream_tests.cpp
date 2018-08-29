#include <catch2/catch.hpp>

#include "udt/ip/udt.h"

#include <cstdint>
#include <array>
#include <list>
#include <map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/spawn.hpp>

#include <boost/asio/socket_base.hpp>

#include <boost/system/error_code.hpp>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include "tests_helpers.h"

/// Test multiple connections on same acceptor
template <class StreamProtocol>
void TestMultipleConnections(
    const typename StreamProtocol::resolver::query& client_local_query,
    const typename StreamProtocol::resolver::query& client_query,
    const typename StreamProtocol::resolver::query& acceptor_query,
    uint64_t max_connections, int max_listening) {
  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;

  std::promise<bool> accepted_ok;
  std::atomic<bool> accepted_strike(false);
  std::atomic<uint64_t> count_acceptor(0);
  std::promise<bool> connected_ok;
  std::atomic<bool> connected_strike(false);
  std::atomic<uint64_t> count_connected(0);

  tests::helpers::AcceptHandler accepted;
  tests::helpers::ConnectHandler connected;

  typename StreamProtocol::acceptor acceptor(io_context);
  typename StreamProtocol::resolver resolver(io_context);

  std::vector<typename StreamProtocol::socket> connected_sockets;
  std::vector<typename StreamProtocol::socket> accepted_sockets;
  for (int i = 0; i < max_connections; ++i) {
    connected_sockets.emplace_back(io_context);
  }
  for (int i = 0; i < max_listening; ++i) {
    accepted_sockets.emplace_back(io_context);
  }

  auto acceptor_endpoint_it = resolver.resolve(acceptor_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());

  typename StreamProtocol::endpoint acceptor_endpoint(*acceptor_endpoint_it);

  auto remote_endpoint_it = resolver.resolve(client_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint remote_endpoint(*remote_endpoint_it);

  auto local_endpoint_it = resolver.resolve(client_local_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint local_endpoint(*local_endpoint_it);

  accepted = [&](const boost::system::error_code& ec) {
    REQUIRE(0 == ec.value());
    if (ec) {
      accepted_strike = true;
      accepted_ok.set_value(false);
    }
    if (!accepted_strike) {
      ++count_acceptor;
      if (count_acceptor == max_listening) {
        accepted_ok.set_value(true);
      }
    }
  };

  connected = [&](const boost::system::error_code& ec) {
    REQUIRE(0 == ec.value());
    if (ec) {
      connected_strike = true;
      connected_ok.set_value(false);
    }
    if (!connected_strike) {
      ++count_connected;
      if (count_connected == max_connections) {
        connected_ok.set_value(true);
      }
    }
  };

  boost::system::error_code ec;

  acceptor.open();
  acceptor.bind(acceptor_endpoint, ec);
  REQUIRE(0 == ec.value());
  acceptor.listen(max_listening, ec);
  REQUIRE(0 == ec.value());

  std::cout << " * Async connect " << max_connections
            << " sockets to a single acceptor" << std::endl;

  for (int i = 0; i < max_connections; ++i) {
    boost::system::error_code bind_ec;
    connected_sockets[i].bind(local_endpoint, bind_ec);
    REQUIRE(0 == ec.value());
    connected_sockets[i].async_connect(remote_endpoint, connected);
  }

    std::vector<std::shared_ptr<std::thread>> threads;
    for (uint16_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
        threads.push_back(std::make_shared<std::thread>([&io_context]() { io_context.run(); }));
    }

  REQUIRE(connected_ok.get_future().get() == true);

  std::cout << " * Async accept " << max_listening << " sockets on the acceptor"
            << std::endl;

  for (int i = 0; i < max_listening; ++i) {
    acceptor.async_accept(accepted_sockets[i], accepted);
  }

  REQUIRE(accepted_ok.get_future().get());

  boost::system::error_code acceptor_close_ec;
  acceptor.close(acceptor_close_ec);
  REQUIRE(0 == acceptor_close_ec.value());

  std::cout << " * Closing accepted sockets" << std::endl;
  for (int i = 0; i < max_listening; ++i) {
    boost::system::error_code close_ec;
    accepted_sockets[i].close(close_ec);
    REQUIRE(0 == close_ec.value());
  }

  std::cout << " * Closing connected sockets" << std::endl;
  for (int i = 0; i < max_connections; ++i) {
    boost::system::error_code close_ec;
    connected_sockets[i].close(close_ec);
    REQUIRE(0 == close_ec.value());
  }

  for (auto &thread : threads) {
    thread->join();
  }
}

/// Test a stream protocol
/// Bind an acceptor to the endpoint defined by
///   acceptor_parameters
/// Connect a client socket to an endpoint defined
///   by client_parameters
/// Send data in ping pong mode
template <class StreamProtocol>
void TestStreamProtocol(
    const typename StreamProtocol::resolver::query& client_query,
    const typename StreamProtocol::resolver::query& acceptor_query,
    uint64_t max_packets) {
  std::cout << ">>>> Stream Test" << std::endl;
  using Buffer = std::array<uint8_t, 165400>;
  using HalfBuffer = std::array<uint8_t, 82700>;
  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;

  uint64_t count1 = 0;
  uint64_t count2 = 0;

  Buffer buffer1;
  Buffer buffer2;
  HalfBuffer r_buffer1;
  HalfBuffer r_buffer2;
  tests::helpers::ResetBuffer(&buffer1, 100, false);
  tests::helpers::ResetBuffer(&buffer2, 200, false);
  tests::helpers::ResetBuffer(&r_buffer1, 0);
  tests::helpers::ResetBuffer(&r_buffer2, 0);

  typename StreamProtocol::socket socket1(io_context);
  typename StreamProtocol::socket socket2(io_context);
  typename StreamProtocol::acceptor acceptor(io_context);
  typename StreamProtocol::resolver resolver(io_context);

  auto acceptor_endpoint_it = resolver.resolve(acceptor_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());

  typename StreamProtocol::endpoint acceptor_endpoint(*acceptor_endpoint_it);

  auto remote_endpoint_it = resolver.resolve(client_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint remote_endpoint(*remote_endpoint_it);

  boost::mutex first_received_mutex1;
  std::atomic<bool> first_received_socket_1(true);

  boost::mutex first_received_mutex2;
  std::atomic<bool> first_received_socket_2(true);

  tests::helpers::AcceptHandler accepted;
  tests::helpers::ConnectHandler connected;
  tests::helpers::SendHandler sent_handler1;
  tests::helpers::SendHandler sent_handler2;
  tests::helpers::ReceiveHandler received_handler1;
  tests::helpers::ReceiveHandler received_handler2;

  accepted = [&](const boost::system::error_code& ec) {
    std::cout << " * Accepted" << std::endl;
    REQUIRE(0 == ec.value());

    boost::system::error_code endpoint_ec;
    socket2.local_endpoint(endpoint_ec);
    REQUIRE(0 == endpoint_ec.value());

    socket2.remote_endpoint(endpoint_ec);
    REQUIRE(0 == endpoint_ec.value());

    boost::asio::async_read(socket2, boost::asio::buffer(r_buffer2),
                            received_handler2);
  };

  connected = [&](const boost::system::error_code& ec) {
    std::cout << " * Connected" << std::endl;
    REQUIRE(0 == ec.value());

    boost::system::error_code endpoint_ec;
    socket1.local_endpoint(endpoint_ec);
    REQUIRE(0 == endpoint_ec.value());

    socket1.remote_endpoint(endpoint_ec);
    REQUIRE(0 == endpoint_ec.value());

    boost::asio::async_write(socket1, boost::asio::buffer(buffer1),
                             sent_handler1);
  };

  received_handler1 = [&](const boost::system::error_code& ec,
                          std::size_t length) {
    REQUIRE(0 == ec.value());

    {
      boost::system::error_code endpoint_ec;
      auto local_ep = socket1.local_endpoint(endpoint_ec);
      auto remote_ep = socket2.remote_endpoint(endpoint_ec);
      REQUIRE(local_ep.socket_id() == remote_ep.socket_id());
    }

    {
      boost::system::error_code endpoint_ec;
      auto local_ep = socket2.local_endpoint(endpoint_ec);
      auto remote_ep = socket1.remote_endpoint(endpoint_ec);
      REQUIRE(local_ep.socket_id() == remote_ep.socket_id());
    }

    {
      boost::mutex::scoped_lock lock_first_received1(first_received_mutex1);
      if (first_received_socket_1) {
        REQUIRE(tests::helpers::CheckHalfBuffers(buffer2, r_buffer1, true));
        tests::helpers::ResetBuffer(&r_buffer1, 0);
        first_received_socket_1 = false;
        boost::asio::async_read(socket1, boost::asio::buffer(r_buffer1),
                                received_handler1);
        return;
      }
      first_received_socket_1 = true;
    }

    REQUIRE(tests::helpers::CheckHalfBuffers(buffer2, r_buffer1, false));
    ++count1;
    if (count1 < max_packets) {
      boost::asio::async_write(socket1, boost::asio::buffer(buffer1),
                               sent_handler1);
    } else {
      REQUIRE(max_packets == count1);
      boost::system::error_code close_ec;

      std::cout << " * Closing acceptor" << std::endl;
      acceptor.close(close_ec);
      REQUIRE_FALSE(acceptor.is_open());

      std::cout << " * Closing socket1" << std::endl;
      socket1.shutdown(boost::asio::socket_base::shutdown_both, close_ec);
      socket1.close(close_ec);
      REQUIRE_FALSE(socket1.is_open());

      std::cout << " * Closing socket2" << std::endl;
      socket2.shutdown(boost::asio::socket_base::shutdown_both, close_ec);
      socket2.close(close_ec);
      REQUIRE_FALSE(socket2.is_open());
    }
  };

  received_handler2 = [&](const boost::system::error_code& ec,
                          std::size_t length) {
    REQUIRE(0 == ec.value());

    {
      boost::mutex::scoped_lock lock_first_received2(first_received_mutex2);
      if (first_received_socket_2) {
        REQUIRE(tests::helpers::CheckHalfBuffers(buffer1, r_buffer2, true));
        tests::helpers::ResetBuffer(&r_buffer1, 0);
        boost::asio::async_read(socket2, boost::asio::buffer(r_buffer2),
                                received_handler2);
        first_received_socket_2 = false;
        return;
      }

      first_received_socket_2 = true;
    }
    REQUIRE(tests::helpers::CheckHalfBuffers(buffer1, r_buffer2, false));
    ++count2;
    if (count2 < max_packets) {
      boost::asio::async_write(socket2, boost::asio::buffer(buffer2),
                               sent_handler2);
    } else {
      boost::asio::async_write(
          socket2, boost::asio::buffer(buffer2),
          [](const boost::system::error_code&, std::size_t) {});
      REQUIRE(max_packets == count2);
    }
  };

  sent_handler1 = [&](const boost::system::error_code& ec, std::size_t length) {
    REQUIRE(0 == ec.value());
    tests::helpers::ResetBuffer(&r_buffer1, 0);
    boost::asio::async_read(socket1, boost::asio::buffer(r_buffer1),
                            received_handler1);
  };

  sent_handler2 = [&](const boost::system::error_code& ec, std::size_t length) {
    REQUIRE(0 == ec.value());
    tests::helpers::ResetBuffer(&r_buffer2, 0);
    boost::asio::async_read(socket2, boost::asio::buffer(r_buffer2),
                            received_handler2);
  };

  boost::system::error_code ec;

  acceptor.open();
  acceptor.bind(acceptor_endpoint, ec);
  REQUIRE(0 == ec.value());
  acceptor.listen(100, ec);
  REQUIRE(0 == ec.value());
  acceptor.async_accept(socket2, accepted);

  std::cout << " * Transfering " << buffer1.size() << " * " << max_packets
            << " = " << buffer1.size() * max_packets
            << " bytes in both direction" << std::endl;

  socket1.async_connect(remote_endpoint, connected);

  std::vector<std::shared_ptr<std::thread>> threads;
  for (uint16_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
    threads.push_back(std::make_shared<std::thread>([&io_context]() { io_context.run(); }));
  }
  for (auto &thread : threads) {
    thread->join();
  }
}

/// Test a stream protocol future interface
template <class StreamProtocol>
void TestStreamProtocolFuture(
    const typename StreamProtocol::resolver::query& client_query,
    const typename StreamProtocol::resolver::query& acceptor_query) {
  std::cout << ">>>> Future Test" << std::endl;
  using Buffer = std::array<uint8_t, 500>;
  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;
  auto p_worker = std::unique_ptr<boost::asio::io_context::work>(
      new boost::asio::io_context::work(io_context));

  Buffer buffer1;
  Buffer buffer2;
  Buffer r_buffer1;
  Buffer r_buffer2;
  tests::helpers::ResetBuffer(&buffer1, 1);
  tests::helpers::ResetBuffer(&buffer2, 2);
  tests::helpers::ResetBuffer(&r_buffer1, 0);
  tests::helpers::ResetBuffer(&r_buffer2, 0);

  typename StreamProtocol::socket socket1(io_context);
  typename StreamProtocol::socket socket2(io_context);
  typename StreamProtocol::acceptor acceptor(io_context);
  typename StreamProtocol::resolver resolver(io_context);

  auto acceptor_endpoint_it = resolver.resolve(acceptor_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint acceptor_endpoint(*acceptor_endpoint_it);

  auto remote_endpoint_it = resolver.resolve(client_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint remote_endpoint(*remote_endpoint_it);

  boost::system::error_code ec;

  acceptor.open();
  acceptor.bind(acceptor_endpoint, ec);
  REQUIRE(0 == ec.value());
  acceptor.listen(100, ec);
  REQUIRE(0 == ec.value());

  auto lambda2 = [&]() {
    try {
      auto accepted = acceptor.async_accept(socket2, boost::asio::use_future);
      accepted.get();
      auto read = boost::asio::async_read(
          socket2, boost::asio::buffer(r_buffer2), boost::asio::use_future);
      read.get();
      auto written = boost::asio::async_write(
          socket2, boost::asio::buffer(buffer2), boost::asio::use_future);
      written.get();
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  };

  auto lambda1 = [&]() {
    try {
      auto connected =
          socket1.async_connect(remote_endpoint, boost::asio::use_future);
      connected.get();
      auto written = boost::asio::async_write(
          socket1, boost::asio::buffer(buffer1), boost::asio::use_future);
      written.get();
      auto read = boost::asio::async_read(
          socket1, boost::asio::buffer(r_buffer1), boost::asio::use_future);
      read.get();

      socket1.shutdown(boost::asio::socket_base::shutdown_both);
      socket1.close();
      REQUIRE_FALSE(socket1.is_open());

      socket2.shutdown(boost::asio::socket_base::shutdown_both);
      socket2.close();
      REQUIRE_FALSE(socket2.is_open());

      acceptor.close();
      REQUIRE_FALSE(acceptor.is_open());

      p_worker.reset();
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  };

  boost::thread_group threads;
  threads.create_thread(lambda2);
  threads.create_thread(lambda1);
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_context]() { io_context.run(); });
  }
  threads.join_all();
}

/// Test a stream protocol spawn interface
template <class StreamProtocol>
void TestStreamProtocolSpawn(
    const typename StreamProtocol::resolver::query& client_query,
    const typename StreamProtocol::resolver::query& acceptor_query) {
  std::cout << ">>>> Spawn Test" << std::endl;
  using Buffer = std::array<uint8_t, 500>;
  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;

  Buffer buffer1;
  Buffer buffer2;
  Buffer r_buffer1;
  Buffer r_buffer2;
  tests::helpers::ResetBuffer(&buffer1, 1);
  tests::helpers::ResetBuffer(&buffer2, 2);
  tests::helpers::ResetBuffer(&r_buffer1, 0);
  tests::helpers::ResetBuffer(&r_buffer2, 0);

  typename StreamProtocol::socket socket1(io_context);
  typename StreamProtocol::socket socket2(io_context);
  typename StreamProtocol::acceptor acceptor(io_context);
  typename StreamProtocol::resolver resolver(io_context);

  auto acceptor_endpoint_it = resolver.resolve(acceptor_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());

  typename StreamProtocol::endpoint acceptor_endpoint(*acceptor_endpoint_it);

  auto remote_endpoint_it = resolver.resolve(client_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint remote_endpoint(*remote_endpoint_it);

  boost::system::error_code ec;

  acceptor.open();
  acceptor.bind(acceptor_endpoint, ec);
  REQUIRE(0 == ec.value());
  acceptor.listen(100, ec);
  REQUIRE(0 == ec.value());

  auto lambda2 = [&](boost::asio::yield_context yield) {
    try {
      acceptor.async_accept(socket2, yield);
      boost::asio::async_read(socket2, boost::asio::buffer(r_buffer2), yield);
      boost::asio::async_write(socket2, boost::asio::buffer(buffer2), yield);
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  };

  auto lambda1 = [&](boost::asio::yield_context yield) {
    try {
      socket1.async_connect(remote_endpoint, yield);
      boost::asio::async_write(socket1, boost::asio::buffer(buffer1), yield);
      boost::asio::async_read(socket1, boost::asio::buffer(r_buffer1), yield);

      socket1.shutdown(boost::asio::socket_base::shutdown_both);
      socket1.close();
      REQUIRE_FALSE(socket1.is_open());

      socket2.shutdown(boost::asio::socket_base::shutdown_both);
      socket2.close();
      REQUIRE_FALSE(socket2.is_open());

      acceptor.close();
      REQUIRE_FALSE(acceptor.is_open());
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  };

  boost::thread_group threads;
  boost::asio::spawn(io_context, lambda2);
  boost::asio::spawn(io_context, lambda1);
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_context]() { io_context.run(); });
  }
  threads.join_all();
}

/// Test a stream protocol synchronous interface
template <class StreamProtocol>
void TestStreamProtocolSynchronous(
    const typename StreamProtocol::resolver::query& client_query,
    const typename StreamProtocol::resolver::query& acceptor_query) {
  std::cout << ">>>> Synchronous Test" << std::endl;
  using Buffer = std::array<uint8_t, 500>;
  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;
  auto p_worker = std::unique_ptr<boost::asio::io_context::work>(
      new boost::asio::io_context::work(io_context));

  Buffer buffer1;
  Buffer buffer2;
  Buffer r_buffer1;
  Buffer r_buffer2;
  tests::helpers::ResetBuffer(&buffer1, 1);
  tests::helpers::ResetBuffer(&buffer2, 2);
  tests::helpers::ResetBuffer(&r_buffer1, 0);
  tests::helpers::ResetBuffer(&r_buffer2, 0);

  typename StreamProtocol::socket socket1(io_context);
  typename StreamProtocol::socket socket2(io_context);
  typename StreamProtocol::acceptor acceptor(io_context);
  typename StreamProtocol::resolver resolver(io_context);

  auto acceptor_endpoint_it = resolver.resolve(acceptor_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());

  typename StreamProtocol::endpoint acceptor_endpoint(*acceptor_endpoint_it);

  auto remote_endpoint_it = resolver.resolve(client_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());
  typename StreamProtocol::endpoint remote_endpoint(*remote_endpoint_it);

  boost::system::error_code ec;

  acceptor.open();
  acceptor.bind(acceptor_endpoint, ec);
  REQUIRE(0 == ec.value());
  acceptor.listen(100, ec);
  REQUIRE(0 == ec.value());

  auto lambda2 = [&]() {
    try {
      acceptor.accept(socket2);
      boost::asio::read(socket2, boost::asio::buffer(r_buffer2));
      boost::asio::write(socket2, boost::asio::buffer(buffer2));
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  };

  auto lambda1 = [&]() {
    try {
      socket1.connect(remote_endpoint);
      boost::asio::write(socket1, boost::asio::buffer(buffer1));
      boost::asio::read(socket1, boost::asio::buffer(r_buffer1));

      socket1.shutdown(boost::asio::socket_base::shutdown_both);
      socket1.close();
      REQUIRE_FALSE(socket1.is_open());

      socket2.shutdown(boost::asio::socket_base::shutdown_both);
      socket2.close();
      REQUIRE_FALSE(socket2.is_open());

      acceptor.close();
      REQUIRE_FALSE(acceptor.is_open());

      p_worker.reset();
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  };

  boost::thread_group threads;
  threads.create_thread(std::move(lambda2));
  threads.create_thread(std::move(lambda1));
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_context]() { io_context.run(); });
  }
  threads.join_all();
}

/// Check connection failure to an endpoint not listening
template <class StreamProtocol>
void TestStreamErrorConnectionProtocol(
    const typename StreamProtocol::resolver::query&
        no_connection_client_query) {
  boost::asio::io_context io_context;
  boost::system::error_code resolve_ec;

  typename StreamProtocol::socket socket_client_no_connection(io_context);
  typename StreamProtocol::resolver resolver(io_context);

  auto remote_no_connect_endpoint_it =
      resolver.resolve(no_connection_client_query, resolve_ec);
  REQUIRE(0 == resolve_ec.value());

  typename StreamProtocol::endpoint remote_no_connect_endpoint(
      *remote_no_connect_endpoint_it);

  tests::helpers::ConnectHandler connection_error_handler =
      [](const boost::system::error_code& ec) {
        REQUIRE(0 != ec.value());
      };

  socket_client_no_connection.async_connect(remote_no_connect_endpoint,
                                            connection_error_handler);

  boost::thread_group threads;
  for (uint16_t i = 1; i <= boost::thread::hardware_concurrency(); ++i) {
    threads.create_thread([&io_context]() { io_context.run(); });
  }
  threads.join_all();
}

using udt_protocol = ip::udt<>;

TEST_CASE("UDTTestMultipleConnections") {
    udt_protocol::resolver::query acceptor_udt_query(boost::asio::ip::udp::v4(), "9000");
    udt_protocol::resolver::query client_local_udt_query("127.0.0.1", "8000");
    udt_protocol::resolver::query client_udt_query("127.0.0.1", "9000");

    // TODO: fix compilation
    //TestMultipleConnections<udt_protocol>(client_local_udt_query, client_udt_query, acceptor_udt_query, 200, 100);
}

TEST_CASE("UDTProtocolLightExchangeTest") {
  udt_protocol::resolver::query acceptor_udt_query(boost::asio::ip::udp::v4(), "9000");
  udt_protocol::resolver::query client_udt_query("127.0.0.1", "9000");

    // TODO: fix compilation
//  TestStreamProtocol<udt_protocol>(client_udt_query, acceptor_udt_query, 1);
//  TestStreamProtocolFuture<udt_protocol>(client_udt_query, acceptor_udt_query);
//  TestStreamProtocolSpawn<udt_protocol>(client_udt_query, acceptor_udt_query);
//  TestStreamProtocolSynchronous<udt_protocol>(client_udt_query, acceptor_udt_query);
}
