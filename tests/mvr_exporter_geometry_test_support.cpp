#include "mvr_exporter_geometry_test_support.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace tests::mvr_geometry {

// Appends one little-endian integer to a binary test fixture.
template <typename T>
static void AppendBinaryValue(std::vector<std::uint8_t> &out, T value) {
  for (std::size_t index = 0; index < sizeof(T); ++index)
    out.push_back(static_cast<std::uint8_t>((value >> (index * 8)) & 0xffu));
}

// Appends one little-endian float to a binary test fixture.
static void AppendBinaryFloat(std::vector<std::uint8_t> &out, float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(value));
  AppendBinaryValue(out, bits);
}

// Appends one nested 3DS chunk to a binary test fixture.
static void Append3dsChunk(std::vector<std::uint8_t> &out, std::uint16_t id,
                           const std::vector<std::uint8_t> &payload) {
  AppendBinaryValue<std::uint16_t>(out, id);
  AppendBinaryValue<std::uint32_t>(
      out, static_cast<std::uint32_t>(payload.size() + 6));
  out.insert(out.end(), payload.begin(), payload.end());
}

// Builds a minimal valid 3DS resource for truss export compliance tests.
std::string BuildTrussGeometry3ds() {
  std::vector<std::uint8_t> vertices;
  AppendBinaryValue<std::uint16_t>(vertices, 4);
  const float points[] = {0.0f, 0.0f,   0.0f, 2000.0f, 0.0f, 0.0f,
                          0.0f, 400.0f, 0.0f, 0.0f,    0.0f, 400.0f};
  for (float value : points)
    AppendBinaryFloat(vertices, value);
  std::vector<std::uint8_t> faces;
  AppendBinaryValue<std::uint16_t>(faces, 2);
  for (std::uint16_t value : {0, 1, 2, 0, 0, 2, 3, 0})
    AppendBinaryValue<std::uint16_t>(faces, value);
  std::vector<std::uint8_t> mesh;
  Append3dsChunk(mesh, 0x4110, vertices);
  Append3dsChunk(mesh, 0x4120, faces);
  std::vector<std::uint8_t> object{'t', 'r', 'u', 's', 's', 0};
  Append3dsChunk(object, 0x4100, mesh);
  std::vector<std::uint8_t> editor;
  Append3dsChunk(editor, 0x4000, object);
  std::vector<std::uint8_t> root;
  Append3dsChunk(root, 0x3D3D, editor);
  std::vector<std::uint8_t> archive;
  Append3dsChunk(archive, 0x4D4D, root);
  return {reinterpret_cast<const char *>(archive.data()), archive.size()};
}

// Builds a minimal 3DS document containing a standard diffuse bitmap map.
std::string BuildTexturedGeometry3ds(const std::string &textureName) {
  std::vector<std::uint8_t> textureNamePayload(textureName.begin(),
                                               textureName.end());
  textureNamePayload.push_back(0);
  std::vector<std::uint8_t> textureMap;
  Append3dsChunk(textureMap, 0xA300, textureNamePayload);
  std::vector<std::uint8_t> material;
  Append3dsChunk(material, 0xA200, textureMap);
  std::vector<std::uint8_t> editor;
  Append3dsChunk(editor, 0xAFFF, material);
  std::vector<std::uint8_t> root;
  Append3dsChunk(root, 0x3D3D, editor);
  std::vector<std::uint8_t> archive;
  Append3dsChunk(archive, 0x4D4D, root);
  return {reinterpret_cast<const char *>(archive.data()), archive.size()};
}

} // namespace tests::mvr_geometry
