/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <wx/init.h>

#include "loader3ds.h"

namespace fs = std::filesystem;

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

// Writes a minimal 3DS mesh with native millimeter vertices and a scaled object
// basis.
static void WriteScaledBasis3ds(const fs::path &path) {
  std::vector<uint8_t> vertices;
  AppendValue<uint16_t>(vertices, 4);
  const float points[] = {0.0f, 0.0f,    0.0f, 290.0f, 0.0f, 0.0f,
                          0.0f, 2500.0f, 0.0f, 0.0f,   0.0f, 290.0f};
  for (float value : points)
    AppendFloat(vertices, value);

  std::vector<uint8_t> faces;
  AppendValue<uint16_t>(faces, 2);
  const uint16_t faceData[] = {0, 1, 2, 0, 0, 2, 3, 0};
  for (uint16_t value : faceData)
    AppendValue<uint16_t>(faces, value);

  std::vector<uint8_t> basis;
  const float basisValues[] = {0.1f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f,
                               0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f};
  for (float value : basisValues)
    AppendFloat(basis, value);

  std::vector<uint8_t> triMesh;
  AppendChunk(triMesh, 0x4110, vertices);
  AppendChunk(triMesh, 0x4120, faces);
  AppendChunk(triMesh, 0x4160, basis);

  std::vector<uint8_t> object;
  object.insert(object.end(), {'t', 'r', 'u', 's', 's', 0});
  AppendChunk(object, 0x4100, triMesh);

  std::vector<uint8_t> editor;
  AppendChunk(editor, 0x4000, object);

  std::vector<uint8_t> rootPayload;
  AppendChunk(rootPayload, 0x3D3D, editor);
  const std::vector<uint8_t> root = Chunk(0x4D4D, rootPayload);

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(root.data()),
            static_cast<std::streamsize>(root.size()));
  assert(out.good());
}

// Compares floats with a small tolerance for binary mesh data.
static bool NearlyEqual(float lhs, float rhs) {
  return std::abs(lhs - rhs) <= 1e-4f;
}

// Returns one axis size from a loaded mesh bounding box.
static float MeshAxisSize(const Mesh &mesh, size_t axis) {
  float minValue = std::numeric_limits<float>::max();
  float maxValue = -std::numeric_limits<float>::max();
  for (size_t i = axis; i + 2 < mesh.vertices.size(); i += 3) {
    minValue = std::min(minValue, mesh.vertices[i]);
    maxValue = std::max(maxValue, mesh.vertices[i]);
  }
  return maxValue - minValue;
}

// Runs the 3DS native-dimensions loader regression test.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempDir =
      fs::temp_directory_path() / "loader3ds_native_dimensions_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir);
  const fs::path modelPath = tempDir / "eurotruss_FD34-250.3ds";
  WriteScaledBasis3ds(modelPath);

  Mesh nativeMesh;
  std::string error;
  assert(Load3DS(modelPath.generic_string(), nativeMesh, false, &error));
  assert(NearlyEqual(MeshAxisSize(nativeMesh, 0), 290.0f));
  assert(NearlyEqual(MeshAxisSize(nativeMesh, 1), 2500.0f));
  assert(NearlyEqual(MeshAxisSize(nativeMesh, 2), 290.0f));

  Mesh transformedMesh;
  assert(Load3DS(modelPath.generic_string(), transformedMesh, true, &error));
  assert(NearlyEqual(MeshAxisSize(transformedMesh, 0), 29.0f));
  assert(NearlyEqual(MeshAxisSize(transformedMesh, 1), 250.0f));
  assert(NearlyEqual(MeshAxisSize(transformedMesh, 2), 29.0f));

  fs::remove_all(tempDir);
  return 0;
}
