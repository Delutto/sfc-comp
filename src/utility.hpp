#pragma once

#include <cstddef>

#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <span>

namespace sfc_comp {

template <typename... Args>
std::string format(const char* format, Args... args) {
  size_t len = std::snprintf(nullptr, 0, format, args...);
  std::vector<char> buff(len + 1);
  std::snprintf(buff.data(), len + 1, format, args...);
  return std::string(buff.begin(), buff.begin() + len);
}

void check_size(size_t input_size, size_t min_size, size_t max_size);

namespace utility {

uint16_t crc16(std::span<const uint8_t> input);
uint16_t crc16(std::span<const uint8_t> input, size_t offset, size_t count);

std::array<size_t, 256> freq_u8(std::span<const uint8_t> input);

std::vector<uint16_t> k_most_freq_u16(std::span<const uint8_t> input, size_t k);

template <typename T, size_t K>
std::array<T, K> k_most(std::span<const size_t> counts) {
  const size_t size = size_t(1) << (8 * sizeof(T));
  if (!(size == counts.size())) {
    throw std::runtime_error(format("The input size (= 0x%X) should be 0x%X.", counts.size(), size));
  }
  T order[size];
  std::iota(order, order + size, 0);
  std::partial_sort(order, order + K, order + size,
    [&](const T a, const T b) { return counts[a] > counts[b]; });
  std::array<T, K> ret;
  for (size_t i = 0; i < K; ++i) ret[i] = order[i];
  return ret;
}

} // namespace utility

inline constexpr int popcount32(uint32_t x) {
  return std::popcount(x);
}

inline constexpr size_t ilog2(size_t n) {
  return std::bit_width(n) - 1;
}

inline uint16_t read16(std::span<const uint8_t> input, size_t i) {
  return input[i] | (input[i + 1] << 8);
}

inline uint32_t read32(std::span<const uint8_t> input, size_t i) {
  return read16(input, i) | read16(input, i + 2) << 16;
}

inline void write16(std::span<uint8_t> c, size_t i, uint32_t v) {
  c[i + 0] = v >> 0;
  c[i + 1] = v >> 8;
}

inline void write16b(std::span<uint8_t> c, size_t i, uint32_t v) {
  c[i + 0] = v >> 8;
  c[i + 1] = v >> 0;
}

inline void write24(std::span<uint8_t> c, size_t i, uint32_t v) {
  c[i + 0] = v >> 0;
  c[i + 1] = v >> 8;
  c[i + 2] = v >> 16;
}

inline void write24b(std::span<uint8_t> c, size_t i, uint32_t v) {
  c[i + 0] = v >> 16;
  c[i + 1] = v >> 8;
  c[i + 2] = v >> 0;
}

inline void write32(std::span<uint8_t> c, size_t i, uint32_t v) {
  c[i + 0] = v >> 0;
  c[i + 1] = v >> 8;
  c[i + 2] = v >> 16;
  c[i + 3] = v >> 24;
}

inline void write32b(std::span<uint8_t> c, size_t i, uint32_t v) {
  c[i + 0] = v >> 24;
  c[i + 1] = v >> 16;
  c[i + 2] = v >> 8;
  c[i + 3] = v >> 0;
}

inline constexpr uint16_t swap16(uint16_t x) {
  return (x >> 8) | (x << 8);
}

constexpr std::array<uint16_t, 256> crc_table = {
  0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
  0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
  0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
  0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
  0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
  0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
  0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
  0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
  0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
  0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
  0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
  0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
  0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
  0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
  0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
  0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
  0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
  0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
  0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
  0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
  0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
  0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
  0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
  0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
  0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
  0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
  0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
  0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
  0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
  0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
  0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
  0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};

} // namespace sfc_comp
