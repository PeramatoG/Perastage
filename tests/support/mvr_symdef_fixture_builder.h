/* This file is part of Perastage. */
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

namespace tests::mvr {
// A self-contained glTF 2.0 triangle with authored generator metadata.
inline std::string TriangleGlb() {
  std::string json = R"({"asset":{"version":"2.0","generator":"A"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":4}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
  while (json.size() % 4) json += ' ';
  std::string output;
  const auto append = [&](uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) output += char((value >> (8 * byte)) & 255);
  };
  append(0x46546C67); append(2); append(static_cast<uint32_t>(28 + json.size() + 36));
  append(static_cast<uint32_t>(json.size())); append(0x4E4F534A); output += json;
  append(36); append(0x004E4942);
  for (const float value : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f}) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    append(bits);
  }
  return output;
}
} // namespace tests::mvr
