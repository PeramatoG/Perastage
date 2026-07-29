/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <wx/init.h>

#include "filesystem_path_utils.h"
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
static bool WriteScaledBasis3ds(const fs::path &path) {
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
  return out.good();
}

// Compares floats with a small tolerance for binary mesh data.
static bool NearlyEqual(float lhs, float rhs) {
  return std::abs(lhs - rhs) <= 1e-4f;
}

// Removes a temporary directory when its owner leaves scope.
class TemporaryDirectoryGuard {
public:
  // Stores the temporary directory that this guard owns.
  explicit TemporaryDirectoryGuard(fs::path path) : path_(std::move(path)) {}

  // Removes the owned directory without throwing during scope cleanup.
  ~TemporaryDirectoryGuard() {
    std::error_code cleanupError;
    fs::remove_all(path_, cleanupError);
  }

  TemporaryDirectoryGuard(const TemporaryDirectoryGuard &) = delete;
  TemporaryDirectoryGuard &operator=(const TemporaryDirectoryGuard &) = delete;

private:
  fs::path path_;
};

// Returns one validated axis size from a loaded mesh bounding box.
static std::optional<float> MeshAxisSize(const Mesh &mesh, size_t axis,
                                         std::string &error) {
  if (axis >= 3) {
    error = "axis is outside the XYZ range";
    return std::nullopt;
  }
  if (mesh.vertices.empty()) {
    error = "vertex vector is empty";
    return std::nullopt;
  }
  if (mesh.vertices.size() % 3 != 0) {
    error = "vertex float count is not divisible by three";
    return std::nullopt;
  }

  float minValue = std::numeric_limits<float>::max();
  float maxValue = -std::numeric_limits<float>::max();
  for (size_t i = axis; i < mesh.vertices.size(); i += 3) {
    const float component = mesh.vertices[i];
    if (!std::isfinite(component)) {
      error = "vertex component at float index " + std::to_string(i) +
              " is not finite";
      return std::nullopt;
    }
    minValue = std::min(minValue, component);
    maxValue = std::max(maxValue, component);
  }
  return maxValue - minValue;
}

// Checks mesh topology and dimensions while emitting actionable diagnostics.
static bool CheckMesh(const char *stage, const Mesh &mesh,
                      const std::array<float, 3> &expected,
                      const std::string &loadError,
                      std::array<float, 3> &actual) {
  bool valid = true;
  if (mesh.vertices.size() != 12 || mesh.vertices.size() / 3 != 4 ||
      mesh.indices.size() != 6) {
    std::cerr << stage << " mesh topology mismatch: vertex float count="
              << mesh.vertices.size() << ", vertex count="
              << mesh.vertices.size() / 3 << ", index count="
              << mesh.indices.size() << ", load error=" << loadError << '\n';
    valid = false;
  }

  constexpr std::array<const char *, 3> axisNames = {"X", "Y", "Z"};
  for (size_t axis = 0; axis < axisNames.size(); ++axis) {
    std::string boundsError;
    const std::optional<float> dimension =
        MeshAxisSize(mesh, axis, boundsError);
    if (!dimension || !std::isfinite(*dimension) ||
        !NearlyEqual(*dimension, expected[axis])) {
      std::cerr << stage << " dimension mismatch: axis=" << axisNames[axis]
                << ", expected=" << expected[axis] << ", actual=";
      if (dimension)
        std::cerr << *dimension;
      else
        std::cerr << "unavailable (" << boundsError << ')';
      std::cerr << ", vertex float count=" << mesh.vertices.size()
                << ", vertex count=" << mesh.vertices.size() / 3
                << ", index count=" << mesh.indices.size()
                << ", load error=" << loadError << '\n';
      valid = false;
    } else {
      actual[axis] = *dimension;
    }
  }
  return valid;
}

// Runs the 3DS native-dimensions loader regression test.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk()) {
    std::cerr << "Failed to initialize wxWidgets.\n";
    return 1;
  }

  const fs::path tempDir =
      fs::temp_directory_path() / "loader3ds_native_dimensions_test";
  fs::remove_all(tempDir);
  int result = 0;
  {
    TemporaryDirectoryGuard cleanup(tempDir);
    std::error_code directoryError;
    if (!fs::create_directories(tempDir, directoryError) || directoryError) {
      std::cerr << "Failed to create fixture directory: "
                << directoryError.message() << '\n';
      result = 1;
    } else {
      const fs::path modelPath = tempDir / "eurotruss_FD34-250.3ds";
      if (!WriteScaledBasis3ds(modelPath) || !fs::is_regular_file(modelPath)) {
        std::cerr << "Failed to create the 3DS source fixture.\n";
        result = 1;
      } else {
        Mesh nativeMesh;
        std::string nativeError;
        const std::string modelPathUtf8 = PathUtils::PathToUtf8(modelPath);
        if (!Load3DS(modelPathUtf8, nativeMesh, false,
                     &nativeError)) {
          std::cerr << "native Load3DS failed: " << nativeError << '\n';
          result = 1;
        }

        Mesh transformedMesh;
        std::string transformedError;
        if (!Load3DS(modelPathUtf8, transformedMesh, true,
                     &transformedError)) {
          std::cerr << "transformed Load3DS failed: " << transformedError
                    << '\n';
          result = 1;
        }

        std::array<float, 3> nativeDimensions{};
        std::array<float, 3> transformedDimensions{};
        const bool nativeValid =
            CheckMesh("native", nativeMesh, {290.0f, 2500.0f, 290.0f},
                      nativeError, nativeDimensions);
        const bool transformedValid =
            CheckMesh("transformed", transformedMesh,
                      {29.0f, 250.0f, 29.0f}, transformedError,
                      transformedDimensions);
        if (!nativeValid || !transformedValid) {
          result = 1;
        }

        for (size_t axis = 0; axis < nativeDimensions.size(); ++axis) {
          if (!NearlyEqual(transformedDimensions[axis],
                           nativeDimensions[axis] * 0.1f)) {
            std::cerr << "transformed scale mismatch at axis " << axis
                      << ": native=" << nativeDimensions[axis]
                      << ", transformed=" << transformedDimensions[axis]
                      << '\n';
            result = 1;
          }
        }
      }
    }
  }

  if (fs::exists(tempDir)) {
    std::cerr << "Temporary fixture directory was not removed.\n";
    result = 1;
  }
  return result;
}
