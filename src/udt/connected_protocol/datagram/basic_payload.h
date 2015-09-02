#ifndef UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_PAYLOAD_H_
#define UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_PAYLOAD_H_

#include <cstdint>

#include <array>
#include <vector>

#include <boost/asio/buffer.hpp>

#include "udt/common/error/error.h"

#include "udt/connected_protocol/io/buffers.h"
#include "udt/connected_protocol/io/write_op.h"

namespace connected_protocol {
namespace datagram {

template <uint32_t MaxSize>
class BufferPayload {
 public:
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef io::fixed_mutable_buffer_sequence MutableBuffers;
  enum { size = MaxSize };

 public:
  BufferPayload() : data_(), size_(MaxSize), offset_(0) {}
  ~BufferPayload() {}

  BufferPayload& operator=(BufferPayload&& other) {
    data_ = std::move(other.data_);
    offset_ = std::move(other.offset_);
    size_ = std::move(other.size_);

    return *this;
  }

  ConstBuffers GetConstBuffers() const {
    boost::asio::const_buffer buf(boost::asio::buffer(data_, size_) + offset_);
    return ConstBuffers(boost::asio::const_buffers_1(buf));
  }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    p_buffers->push_back(boost::asio::buffer(data_, size_) + offset_);
  }

  MutableBuffers GetMutableBuffers() {
    boost::asio::mutable_buffer buf(boost::asio::buffer(data_, size_) +
                                    offset_);
    return MutableBuffers(boost::asio::mutable_buffers_1(buf));
  }

  void GetMutableBuffers(MutableBuffers* p_buffers) {
    p_buffers->push_back(boost::asio::buffer(data_, size_) + offset_);
  }

  uint32_t GetSize() const { return size_ - offset_; }

  void SetSize(uint32_t new_size) { size_ = new_size; }

  void SetOffset(uint32_t offset) { offset_ = offset; }

 private:
  std::array<uint8_t, MaxSize> data_;
  uint32_t size_;
  uint32_t offset_;
};

template <uint64_t MaximumSize>
class ConstBufferSequencePayload {
 public:
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef connected_protocol::io::basic_pending_write_operation SizedOp;
  enum { size = MaximumSize };

  ConstBufferSequencePayload()
      : buffers_(), current_size_(0), p_sized_op_(nullptr), total_copy_(0) {}

  bool AddConstBuffer(const ConstBuffers::value_type& buffer) {
    if (boost::asio::buffer_size(buffers_) + boost::asio::buffer_size(buffer) >
        MaximumSize) {
      return false;
    }
    buffers_.push_back(buffer);
    current_size_ += boost::asio::buffer_size(buffer);
    return true;
  }

  std::size_t RemainingSize() { return MaximumSize - current_size_; }

  bool IsFull() { return MaximumSize == current_size_; }

  ConstBuffers GetConstBuffers() const { return buffers_; }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    for (auto buffer : buffers_) {
      p_buffers->push_back(buffer);
    }
  }

  void set_p_sized_op(SizedOp* p_sized_op) { p_sized_op_ = p_sized_op; }

  void set_total_copy(std::size_t total_copy) { total_copy_ = total_copy; }

  void complete(boost::asio::io_service& io_service) {
    if (p_sized_op_ != nullptr && total_copy_ != 0) {
      // Execute handler
      auto p_sized_op = p_sized_op_;
      auto total_copy = total_copy_;

      auto do_complete = [p_sized_op, total_copy]() {
        p_sized_op->complete(
            boost::system::error_code(::common::error::success,
                                      ::common::error::get_error_category()),
            total_copy);
      };
      io_service.post(do_complete);
    }
  }

 private:
  ConstBuffers buffers_;
  std::size_t current_size_;
  SizedOp* p_sized_op_;
  std::size_t total_copy_;
};

class basic_ConnectionPayload {
 public:
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef io::fixed_mutable_buffer_sequence MutableBuffers;

 private:
  struct Content {
    Content()
        : version_(0),
          socket_type_(htonl(1)),
          initial_packet_sequence_number_(0),
          maximum_packet_size_(0),
          maximum_window_flow_size_(0),
          connection_type_(0),
          socket_id_(0),
          syn_cookie_(0) {}

    uint32_t version_;
    uint32_t socket_type_;
    uint32_t initial_packet_sequence_number_;
    uint32_t maximum_packet_size_;
    uint32_t maximum_window_flow_size_;
    int32_t connection_type_;
    uint32_t socket_id_;
    /// See RFC 4987
    uint32_t syn_cookie_;
    std::array<uint32_t, 4> peer_address_;
  };

 public:
  enum { size = sizeof(Content) };

  enum version : uint32_t { FORTH = 4 };

  enum socket_defined_type : uint32_t { STREAM = 1, DGRAM = 0 };
  enum client_server_connection_type : int32_t {
    REGULAR = 1,
    RENDEZ_VOUS = 0,
    FIRST_RESPONSE = -1,
    SECOND_RESPONSE = -2,
  };

 public:
  basic_ConnectionPayload() : content_() {}

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    p_buffers->push_back(boost::asio::buffer(&content_, sizeof(content_)));
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers* p_buffers) {
    p_buffers->push_back(boost::asio::buffer(&content_, sizeof(content_)));
  }

  bool IsSynCookie() const { return version() == 0; }

  bool IsServerHandshakeResponse() const { return version() != 0; }

  uint32_t version() const { return ntohl(content_.version_); }

  void set_version(uint32_t version) { content_.version_ = htonl(version); }

  uint32_t socket_type() const { return ntohl(content_.socket_type_); }

  void set_socket_type(socket_defined_type socket_type) {
    content_.socket_type_ = htonl(socket_type);
  }

  uint32_t initial_packet_sequence_number() const {
    return 0x7FFFFFFF & ntohl(content_.initial_packet_sequence_number_);
  }

  void set_initial_packet_sequence_number(
      uint32_t initial_packet_sequence_number) {
    content_.initial_packet_sequence_number_ =
        htonl((ntohl(content_.initial_packet_sequence_number_) & 0x80000000) |
              initial_packet_sequence_number);
  }

  uint32_t maximum_packet_size() const {
    return ntohl(content_.maximum_packet_size_);
  }

  void set_maximum_packet_size(uint32_t maximum_packet_size) {
    content_.maximum_packet_size_ = htonl(maximum_packet_size);
  }

  uint32_t maximum_window_flow_size() const {
    return ntohl(content_.maximum_window_flow_size_);
  }

  void set_maximum_window_flow_size(uint32_t maximum_window_flow_size) {
    content_.maximum_window_flow_size_ = htonl(maximum_window_flow_size);
  }

  uint32_t connection_type() const { return ntohl(content_.connection_type_); }

  void set_connection_type(client_server_connection_type connection_type) {
    content_.connection_type_ = htonl(connection_type);
  }

  uint32_t socket_id() const { return ntohl(content_.socket_id_); }

  void set_socket_id(uint32_t socket_id) {
    content_.socket_id_ = htonl(socket_id);
  }

  uint32_t syn_cookie() const { return ntohl(content_.syn_cookie_); }

  void set_syn_cookie(uint32_t syn_cookie) {
    content_.syn_cookie_ = htonl(syn_cookie);
  }

  std::array<uint32_t, 4> pear_address() const {
    return content_.peer_address_;
  }

  void set_peer_address(const std::array<uint32_t, 4>& peer_address) {
    content_.peer_address_ = peer_address;
  }

 private:
  Content content_;
};

class basic_AckPayload {
 private:
  struct Content {
    Content()
        : max_packet_sequence_number_(0),
          rtt_(0),
          rtt_var_(0),
          available_buffer_size_(0),
          packet_arrival_speed_(0),
          estimated_link_capacity_(0) {}

    uint32_t max_packet_sequence_number_;
    uint32_t rtt_;
    uint32_t rtt_var_;
    uint32_t available_buffer_size_;
    uint32_t packet_arrival_speed_;
    uint32_t estimated_link_capacity_;
  };

 public:
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef io::fixed_mutable_buffer_sequence MutableBuffers;
  enum { size = sizeof(Content) };

 public:
  basic_AckPayload() : content_(), payload_size_(size) {}

  void set_max_packet_sequence_number(uint32_t seq_number) {
    content_.max_packet_sequence_number_ = htonl(seq_number);
  }

  uint32_t max_packet_sequence_number() const {
    return ntohl(content_.max_packet_sequence_number_);
  }

  void set_rtt(uint32_t rtt) { content_.rtt_ = htonl(rtt); }

  uint32_t rtt() const { return ntohl(content_.rtt_); }

  void set_rtt_var(uint32_t rtt_var) { content_.rtt_var_ = htonl(rtt_var); }

  uint32_t rtt_var() const { return ntohl(content_.rtt_var_); }

  void set_available_buffer_size(uint32_t available_buffer_size) {
    content_.available_buffer_size_ = htonl(available_buffer_size);
  }

  uint32_t available_buffer_size() const {
    return ntohl(content_.available_buffer_size_);
  }

  void set_packet_arrival_speed(uint32_t packet_arrival_speed) {
    content_.packet_arrival_speed_ = htonl(packet_arrival_speed);
  }

  uint32_t packet_arrival_speed() const {
    return ntohl(content_.packet_arrival_speed_);
  }

  void set_estimated_link_capacity(uint32_t estimated_link_capacity) {
    content_.estimated_link_capacity_ = htonl(estimated_link_capacity);
  }

  uint32_t estimated_link_capacity() const {
    return ntohl(content_.estimated_link_capacity_);
  }

  void set_payload_size(std::size_t size) { payload_size_ = size; }

  void SetAsLightAck() { payload_size_ = 4; }

  void SetAsFullAck() { payload_size_ = size; }

  bool IsLightAck() const { return payload_size_ == 4; }

  bool IsFull() const { return payload_size_ > 16; }

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    p_buffers->push_back(
        boost::asio::buffer(&content_.max_packet_sequence_number_,
                            sizeof(content_.max_packet_sequence_number_)));
    if (!IsLightAck()) {
      p_buffers->push_back(
          boost::asio::buffer(&content_.rtt_, sizeof(content_.rtt_)));
      p_buffers->push_back(
          boost::asio::buffer(&content_.rtt_var_, sizeof(content_.rtt_var_)));
      p_buffers->push_back(
          boost::asio::buffer(&content_.available_buffer_size_,
                              sizeof(content_.available_buffer_size_)));
      p_buffers->push_back(
          boost::asio::buffer(&content_.packet_arrival_speed_,
                              sizeof(content_.packet_arrival_speed_)));
      p_buffers->push_back(
          boost::asio::buffer(&content_.estimated_link_capacity_,
                              sizeof(content_.estimated_link_capacity_)));
    }
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers* p_buffers) {
    p_buffers->push_back(
        boost::asio::buffer(&content_.max_packet_sequence_number_,
                            sizeof(content_.max_packet_sequence_number_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.rtt_, sizeof(content_.rtt_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.rtt_var_, sizeof(content_.rtt_var_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.available_buffer_size_,
                            sizeof(content_.available_buffer_size_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.packet_arrival_speed_,
                            sizeof(content_.packet_arrival_speed_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.estimated_link_capacity_,
                            sizeof(content_.estimated_link_capacity_)));
  }

 private:
  Content content_;
  std::size_t payload_size_;
};

template <uint32_t MaxSize>
class basic_NAckPayload {
 public:
  typedef uint32_t packet_sequence_number_type;
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef io::fixed_mutable_buffer_sequence MutableBuffers;
  typedef std::vector<packet_sequence_number_type> LossVector;
  enum { size = 0 };

 public:
  basic_NAckPayload() : loss_packets_() {}

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    p_buffers->push_back(boost::asio::buffer(loss_packets_));
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers* p_buffers) {
    p_buffers->push_back(boost::asio::buffer(loss_packets_));
  }

  uint32_t GetSize() const { return loss_packets_.size() * 4; }

  /// @param Size in bytes
  void SetSize(std::size_t new_size) {
    int32_t vector_size = (uint32_t)ceil((double)new_size / 4);
    if (vector_size <= MaxSize) {
      loss_packets_.resize(vector_size);
    } else {
      loss_packets_.resize(MaxSize);
    }
  }

  void ResetSize() { loss_packets_.resize(MaxSize); }

  void AddLossPacket(packet_sequence_number_type packet_seq_num) {
    loss_packets_.push_back(htonl(packet_seq_num & 0x7FFFFFFF));
  }

  void AddLossRange(packet_sequence_number_type first_seq_num,
                    packet_sequence_number_type last_seq_num) {
    loss_packets_.push_back(htonl(first_seq_num | 0x80000000));
    loss_packets_.push_back(htonl(last_seq_num & 0x7FFFFFFF));
  }

  LossVector GetLossPackets() const {
    LossVector loss_packets(loss_packets_);
    for (auto& loss_packet : loss_packets) {
      loss_packet = ntohl(loss_packet);
    }
    return loss_packets;
  }

 private:
  LossVector loss_packets_;
};

class basic_MessageDropRequestPayload {
 private:
  struct Content {
    Content() : first_sequence_number_(0), last_sequence_number_(0) {}

    uint32_t first_sequence_number_;
    uint32_t last_sequence_number_;
  };

 public:
  typedef io::fixed_const_buffer_sequence ConstBuffers;
  typedef io::fixed_mutable_buffer_sequence MutableBuffers;
  enum { size = sizeof(Content) };

 public:
  basic_MessageDropRequestPayload(uint32_t first_sequence_number,
                                  uint32_t last_sequence_number)
      : content_() {}

  ConstBuffers GetConstBuffers() const {
    ConstBuffers buffers;
    GetConstBuffers(&buffers);
    return buffers;
  }

  void GetConstBuffers(ConstBuffers* p_buffers) const {
    p_buffers->push_back(
        boost::asio::buffer(&content_.first_sequence_number_,
                            sizeof(content_.first_sequence_number_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.last_sequence_number_,
                            sizeof(content_.last_sequence_number_)));
  }

  MutableBuffers GetMutableBuffers() {
    MutableBuffers buffers;
    GetMutableBuffers(&buffers);
    return buffers;
  }

  void GetMutableBuffers(MutableBuffers* p_buffers) {
    p_buffers->push_back(
        boost::asio::buffer(&content_.first_sequence_number_,
                            sizeof(content_.first_sequence_number_)));
    p_buffers->push_back(
        boost::asio::buffer(&content_.last_sequence_number_,
                            sizeof(content_.last_sequence_number_)));
  }

 private:
  Content content_;
};
}  // datagram
}  // connected_protocol

#endif  // UDT_CONNECTED_PROTOCOL_DATAGRAM_BASIC_PAYLOAD_H_
