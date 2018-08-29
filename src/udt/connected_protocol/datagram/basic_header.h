#ifndef UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_HEADER_H_
#define UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_HEADER_H_

#include <cstdint>

#include <boost/asio/buffer.hpp>

#include "udt/connected_protocol/io/buffers.h"

namespace connected_protocol {
namespace datagram {

class basic_GenericHeader {
 private:
  using ConstBuffers = io::fixed_const_buffer_sequence;
  using MutableBuffers = io::fixed_mutable_buffer_sequence;

  using DataType = std::array<uint32_t, 4>;

 public:
  enum { size = sizeof(DataType) };
  enum { type_mask = 0x80000000 };

  basic_GenericHeader() {}

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers *p_buffers) const {
    p_buffers->push_back(boost::asio::buffer(data_));
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers *p_buffers) {
    p_buffers->push_back(boost::asio::buffer(data_));
  }

  DataType &data() { return data_; }

  bool IsControlPacket() { return ((ntohl(data_[0]) & type_mask) >> 31) == 1; }

  bool IsDataPacket() { return ((ntohl(data_[0]) & type_mask) >> 31) == 0; }

  uint32_t GetTimestamp() const { return ntohl(data_[2]); }

  uint32_t GetSocketId() const { return ntohl(data_[3]); }

 private:
  DataType data_;
};

class basic_DataHeader {
 private:
  using ConstBuffers = io::fixed_const_buffer_sequence;
  using MutableBuffers = io::fixed_mutable_buffer_sequence;

  struct Content {
    Content()
        : packet_sequence_number(0),
          packet_info(0),
          timestamp(0),
          destination_socket(0) {}

    /// 1 bit : 0 packet is a control packet
    /// 2 - 32 bits: packet sequence number
    /// Increased by 1 after each sent data packet
    uint32_t packet_sequence_number;
    /// 1-2 bit : position of the packet in the message (see position enum)
    /// 3 bit : message should be delivered in order
    /// 4 - 32 bit : message identifier. A message contains multiple packets
    uint32_t packet_info;
    /// 32 bits: timestamp when the packet is sent
    /// Relative value starting from the time when the connection is set up
    uint32_t timestamp;
    /// 32 bits: id of the socket for UDT multiplexer
    uint32_t destination_socket;
  };

 public:
  enum position : uint32_t {
    MIDDLE = 0x00000000,
    LAST = 0x40000000,
    FIRST = 0x80000000,
    ONLY_ONE_PACKET = 0xB0000000
  };

  enum order : uint32_t { IN_ORDER = 0x20000000, NOT_IN_ORDER = 0x00000000 };

  enum { size = sizeof(Content) };

  basic_DataHeader() : content_() {}

  basic_DataHeader &operator=(basic_DataHeader &&other) {
    content_ = std::move(other.content_);

    return *this;
  }

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers *p_buffers) const {
    p_buffers->push_back(boost::asio::buffer(&content_, sizeof(content_)));
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers *p_buffers) {
    p_buffers->push_back(boost::asio::buffer(&content_, sizeof(content_)));
  }

  uint32_t packet_sequence_number() const {
    return 0x7FFFFFFF & ntohl(content_.packet_sequence_number);
  }

  void set_packet_sequence_number(uint32_t packet_sequence_number) {
    content_.packet_sequence_number =
        htonl((ntohl(content_.packet_sequence_number) & 0x80000000) |
              packet_sequence_number);
  }

  uint32_t message_position() const {
    return (ntohl(content_.packet_info) & 0xC0000000);
  }

  void set_message_position(position pos) {
    content_.packet_info =
        htonl((ntohl(content_.packet_info) & 0x3FFFFFFF) | pos);
  }

  bool in_order() const {
    return (ntohl(content_.packet_info) & 0x20000000) == IN_ORDER;
  }

  void set_in_order(order in_order) {
    content_.packet_info =
        htonl((ntohl(content_.packet_info) & 0xDFFFFFFF) | in_order);
  }

  uint32_t message_number() const {
    return ntohl(content_.packet_info) & 0x1FFFFFFF;
  }

  void set_message_number(uint32_t message_number) {
    content_.packet_info =
        htonl((ntohl(content_.packet_info) & 0xE0000000) | message_number);
  }

  uint32_t timestamp() const { return ntohl(content_.timestamp); }

  void set_timestamp(uint32_t timestamp) {
    content_.timestamp = htonl(timestamp);
  }

  uint32_t destination_socket() const {
    return ntohl(content_.destination_socket);
  }

  void set_destination_socket(uint32_t destination_socket) {
    content_.destination_socket = htonl(destination_socket);
  }

 private:
  Content content_;
};

class basic_ControlHeader {
 private:
  using ConstBuffers = io::fixed_const_buffer_sequence;
  using MutableBuffers = io::fixed_mutable_buffer_sequence;

 private:
  struct Content {
    Content()
        : flags_(htonl(0x80000000)),
          additional_info_(0),
          timestamp_(0),
          destination_socket_(0) {}

    /// 1 bit: 1 packet is a control packet
    /// 2-16 bits: type of control packet (type enum)
    /// 16-35 bits: reserved for custom implementation
    uint32_t flags_;
    /// 3-32 bits: function of the type
    uint32_t additional_info_;
    /// 32 bits timestamp when the packet is sent
    /// Relative value starting from the time when the connection is set up
    uint32_t timestamp_;
    /// 32 bits: id of the socket for UDT multiplexer
    uint32_t destination_socket_;
  };

 public:
  enum type : uint32_t {
    CONNECTION = 0x00000000,
    KEEP_ALIVE = 0x00010000,
    ACK = 0x00020000,
    NACK = 0x00030000,
    // UNUSED 4
    SHUTDOWN = 0x00050000,
    ACK_OF_ACK = 0x00060000,
    MESSAGE_DROP_REQUEST = 0x00070000,
    CUSTOM = 0x7FFF0000
  };

  enum aditionnal_info : uint32_t { NO_ADDITIONAL_INFO = 0 };

  enum { size = sizeof(Content) };

  basic_ControlHeader() : content_() {}

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers *p_buffers) const {
    p_buffers->push_back(boost::asio::buffer(&content_, sizeof(content_)));
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers *p_buffers) {
    p_buffers->push_back(boost::asio::buffer(&content_, sizeof(content_)));
  }

  uint32_t flags() const { return ntohl(content_.flags_) & 0x7FFF0000; }

  void set_flags(type type) {
    content_.flags_ = htonl((ntohl(content_.flags_) & 0x8000FFFF) | type);
  }

  uint32_t reserved() const { return ntohl(content_.flags_) & 0x0000FFFF; }

  void set_reserved(uint32_t reserved) {
    content_.flags_ = htonl((ntohl(content_.flags_) & 0xFFFF0000) | reserved);
  }

  uint32_t additional_info() const {
    return ntohl(content_.additional_info_) & 0x1FFFFFFF;
  }

  void set_additional_info(uint32_t additional_info) {
    content_.additional_info_ = htonl(
        (ntohl(content_.additional_info_) & 0xE0000000) | additional_info);
  }

  uint32_t timestamp() const { return ntohl(content_.timestamp_); }

  void set_timestamp(uint32_t timestamp) {
    content_.timestamp_ = htonl(timestamp);
  }

  uint32_t destination_socket() const {
    return ntohl(content_.destination_socket_);
  }

  void set_destination_socket(uint32_t destination_socket) {
    content_.destination_socket_ = htonl(destination_socket);
  }

  bool IsType(type type) const { return flags() == type; }

 private:
  Content content_;
};

}  // datagram
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_HEADER_H_
