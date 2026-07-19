#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace wirestead_bench {

constexpr size_t kFrameHeaderSize = sizeof(uint32_t);

inline std::string make_payload(size_t size) {
  std::string payload(size, '\0');
  for (size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<char>('A' + (i % 26));
  }
  return payload;
}

inline std::string make_frame(std::string_view payload) {
  const auto payload_size = static_cast<uint32_t>(payload.size());
  std::string frame(kFrameHeaderSize + payload.size(), '\0');
  std::memcpy(frame.data(), &payload_size, kFrameHeaderSize);
  std::memcpy(frame.data() + kFrameHeaderSize, payload.data(), payload.size());
  return frame;
}

class FrameDecoder {
 public:
  std::vector<std::string> push(std::string_view bytes) {
    buffer_.append(bytes.data(), bytes.size());

    std::vector<std::string> frames;
    while (buffer_.size() >= kFrameHeaderSize) {
      uint32_t payload_size = 0;
      std::memcpy(&payload_size, buffer_.data(), kFrameHeaderSize);

      const size_t frame_size = kFrameHeaderSize + static_cast<size_t>(payload_size);
      if (buffer_.size() < frame_size) {
        break;
      }

      frames.emplace_back(buffer_.data() + kFrameHeaderSize, payload_size);
      buffer_.erase(0, frame_size);
    }

    return frames;
  }

 private:
  std::string buffer_;
};

inline bool payload_matches(std::string_view expected, std::string_view actual) { return expected == actual; }

}  // namespace wirestead_bench
