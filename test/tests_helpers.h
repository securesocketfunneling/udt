#ifndef UDT_TESTS_TESTS_HELPERS_H_
#define UDT_TESTS_TESTS_HELPERS_H_

#include <cstdint>
#include <array>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>
#include <iostream>

#include <boost/system/error_code.hpp>

namespace tests {
struct helpers {
  using SendHandler =
      std::function<void(const boost::system::error_code&, std::size_t)>;
  using ReceiveHandler =
      std::function<void(const boost::system::error_code&, std::size_t)>;
  using ConnectHandler = std::function<void(const boost::system::error_code&)>;
  using AcceptHandler = std::function<void(const boost::system::error_code&)>;

  template <class Buffer>
  static void ResetBuffer(Buffer* p_buffer, typename Buffer::value_type value,
                          bool identical = true) {
    for (size_t i = 0; i < p_buffer->size(); ++i) {
      (*p_buffer)[i] = identical ? value : (value + i) % 256;
    }
  }

  template <class Buffer>
  static bool CheckBuffers(const Buffer& buffer1, const Buffer& buffer2) {
    if (buffer1.size() != buffer2.size()) {
      return false;
    }

    for (std::size_t i = 0; i < buffer1.size(); ++i) {
      if (buffer1[i] != buffer2[i]) {
        return false;
      }
    }

    return true;
  }

  template <class Buffer, class HalfBuffer>
  static bool CheckHalfBuffers(const Buffer& buffer1, const HalfBuffer& buffer2,
                               bool first_half = true) {
    if (buffer1.size() / 2 != buffer2.size()) {
      std::cerr << "Check buffer size failed\n";
      return false;
    }
    std::size_t begin(first_half ? 0 : buffer2.size());
    std::size_t end(first_half ? buffer2.size() : buffer1.size());
    for (std::size_t i = begin; i < end; ++i) {
      if (buffer1[i] != buffer2[i - begin]) {
        std::cerr
            << "Check buffer failed at index " << i << " : "
            << static_cast<uint8_t>(buffer1[i])
            << " != " << static_cast<uint8_t>(buffer2[i - begin]) << "\n";
        return false;
      }
    }

    return true;
  }
};

}  // tests

#endif  // UDT_TESTS_TESTS_HELPERS_H_
