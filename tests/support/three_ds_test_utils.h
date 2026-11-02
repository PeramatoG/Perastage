#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace three_ds_test {

// Appends a little-endian integer to a byte buffer.
template <typename T>
static void AppendValue(std::vector<uint8_t> &out, T value) {
  for (size_t i = 0; i < sizeof(T); ++i)
    out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xffu));
}

// Appends a little-endian float to a byte buffer.
static void AppendFloat(std::vector<uint8_t> &out, float value) {
  static_assert(sizeof(float) == sizeof(uint32_t));
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(float));
  AppendValue<uint32_t>(out, bits);
}

// Wraps raw payload bytes in a 3DS chunk header.
static std::vector<uint8_t> Chunk(uint16_t id,
                                  const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> out;
  AppendValue<uint16_t>(out, id);
  AppendValue<uint32_t>(out, static_cast<uint32_t>(payload.size() + 6));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

// Appends a nested child chunk into a parent payload buffer.
static void AppendChunk(std::vector<uint8_t> &out, uint16_t id,
                        const std::vector<uint8_t> &payload) {
  const std::vector<uint8_t> chunk = Chunk(id, payload);
  out.insert(out.end(), chunk.begin(), chunk.end());
}

} // namespace three_ds_test
