#ifndef UDT_CONNECTED_PROTOCOL_PROTOCOL_H_
#define UDT_CONNECTED_PROTOCOL_PROTOCOL_H_

#include <cstdint>

#include <string>

#include <boost/asio/io_service.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/detail/socket_option.hpp>
#include <boost/asio/detail/socket_types.hpp>

#include <boost/chrono.hpp>

#include "udt/connected_protocol/cache/connections_info_manager.h"
#include "udt/connected_protocol/congestion/congestion_control.h"

#include "udt/connected_protocol/datagram/basic_datagram.h"
#include "udt/connected_protocol/datagram/basic_header.h"
#include "udt/connected_protocol/datagram/basic_payload.h"
#include "udt/connected_protocol/datagram/empty_component.h"

#include "udt/connected_protocol/endpoint.h"
#include "udt/connected_protocol/flow.h"
#include "udt/connected_protocol/logger/no_log.h"
#include "udt/connected_protocol/multiplexer.h"
#include "udt/connected_protocol/multiplexers_manager.h"
#include "udt/connected_protocol/resolver.h"

#include "udt/connected_protocol/socket_session.h"
#include "udt/connected_protocol/acceptor_session.h"

#include "udt/connected_protocol/stream_socket_service.h"
#include "udt/connected_protocol/socket_acceptor_service.h"

namespace connected_protocol {

template <class NextLayer, class Logger = logger::NoLog,
          template <class> class CongestionControlAlg =
              congestion::CongestionControl>
class Protocol {
 private:
  typedef typename NextLayer::socket next_socket_type;
  typedef typename NextLayer::endpoint next_endpoint_type;

 public:
  typedef NextLayer next_layer_protocol;

  // Sessions
  typedef SocketSession<Protocol> socket_session;
  typedef AcceptorSession<Protocol> acceptor_session;
  typedef uint32_t endpoint_context_type;

  // Clock
  typedef boost::chrono::high_resolution_clock clock;
  typedef clock::time_point time_point;
  typedef boost::asio::basic_waitable_timer<clock> timer;

  // Socket options
  enum socket_options { TIMEOUT_DELAY };

  enum : uint32_t {
    MTU = 1500,
    MAXIMUM_WINDOW_FLOW_SIZE = 25600,
    MAX_PACKET_SEQUENCE_NUMBER = 0x7FFFFFFF,
    MAX_ACK_SEQUENCE_NUMBER = 0x1FFFFFFF,
    MAX_MSG_SEQUENCE_NUMBER = 0x1FFFFFFF
  };

  enum : uint32_t { PACKET_SIZE_CORRECTION = 28 };

  typedef boost::asio::detail::socket_option::integer<
      BOOST_ASIO_OS_DEF(SOL_SOCKET), TIMEOUT_DELAY> timeout_option_type;

  typedef Endpoint<Protocol> endpoint;

  typedef Resolver<Protocol> resolver;

  typedef MultiplexerManager<Protocol> multiplexer_manager;
  typedef Multiplexer<Protocol> multiplexer;
  typedef Flow<Protocol> flow;

  typedef CongestionControlAlg<Protocol> congestion_control;

  typedef Logger logger;

  typedef boost::asio::basic_stream_socket<
      Protocol, stream_socket_service<Protocol>> socket;
  typedef boost::asio::basic_socket_acceptor<
      Protocol, socket_acceptor_service<Protocol>> acceptor;

  // Datagram types
  typedef datagram::EmptyComponent EmptyPayload;

  // Datagram header types
  typedef datagram::basic_GenericHeader GenericHeader;
  typedef datagram::basic_DataHeader DataHeader;
  typedef datagram::basic_ControlHeader ControlHeader;

  // Datagram payload types
  typedef datagram::basic_ConnectionPayload ConnectionPayload;
  typedef datagram::basic_AckPayload AckPayload;
  typedef datagram::basic_NAckPayload<MTU - GenericHeader::size> NAckPayload;
  typedef datagram::basic_MessageDropRequestPayload MessageDropRequestPayload;
  typedef datagram::BufferPayload<MTU - GenericHeader::size>
      GenericReceivePayload;
  typedef datagram::ConstBufferSequencePayload<MTU - GenericHeader::size>
      SendPayload;

  // Generic datagram type
  typedef datagram::basic_Datagram<GenericHeader, GenericReceivePayload>
      GenericReceiveDatagram;

  // Control datagram types
  typedef datagram::basic_Datagram<ControlHeader, ConnectionPayload>
      ConnectionDatagram;
  typedef datagram::basic_Datagram<ControlHeader, EmptyPayload>
      KeepAliveDatagram;
  typedef datagram::basic_Datagram<ControlHeader, AckPayload> AckDatagram;
  typedef datagram::basic_Datagram<ControlHeader, NAckPayload> NAckDatagram;
  typedef datagram::basic_Datagram<ControlHeader, EmptyPayload>
      ShutdownDatagram;
  typedef datagram::basic_Datagram<ControlHeader, EmptyPayload>
      AckOfAckDatagram;
  typedef datagram::basic_Datagram<ControlHeader, MessageDropRequestPayload>
      MessageDropRequestDatagram;

  typedef datagram::basic_Datagram<ControlHeader, GenericReceivePayload>
      GenericControlDatagram;

  // Data datagram
  typedef datagram::basic_Datagram<DataHeader, GenericReceivePayload>
      DataDatagram;
  typedef datagram::basic_Datagram<DataHeader, GenericReceivePayload>
      SendDatagram;
  typedef DataDatagram ReceiveDatagram;

 public:
  static MultiplexerManager<Protocol> multiplexers_manager_;
  static cache::ConnectionsInfoManager<Protocol> connections_info_manager_;
};

template <class NextLayer, class Logger,
          template <class> class CongestionControlAlg>
MultiplexerManager<Protocol<NextLayer, Logger, CongestionControlAlg>>
    Protocol<NextLayer, Logger, CongestionControlAlg>::multiplexers_manager_;

template <class NextLayer, class Logger,
          template <class> class CongestionControlAlg>
cache::ConnectionsInfoManager<Protocol<NextLayer, Logger, CongestionControlAlg>>
    Protocol<NextLayer, Logger,
             CongestionControlAlg>::connections_info_manager_;

}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_PROTOCOL_H_
