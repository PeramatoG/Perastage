#include "primitive_model_resources.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace mvr {
namespace {

constexpr const char *kSphereToken = "primitive:sphere";
constexpr const char *kCubeToken = "primitive:cube";
constexpr const char *kCylinderToken = "primitive:cylinder";
constexpr const char *kSphereArchiveName = "perastage_primitive_sphere.glb";
constexpr const char *kCubeArchiveName = "perastage_primitive_cube.glb";
constexpr const char *kCylinderArchiveName = "perastage_primitive_cylinder.glb";

std::string NormalizeLower(std::string value) {
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), [](unsigned char c) {
                return !std::isspace(c);
              }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [](unsigned char c) { return !std::isspace(c); })
                  .base(),
              value.end());
  std::replace(value.begin(), value.end(), '\\', '/');
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool IsSphereRef(const std::string &normalized) {
  return normalized.rfind(kSphereToken, 0) == 0 ||
         normalized.find("perastage_primitive_sphere.glb") != std::string::npos ||
         normalized.find("perastage_primitive_sphere.obj") != std::string::npos ||
         normalized.find("primitive_sphere_") != std::string::npos;
}

bool IsCubeRef(const std::string &normalized) {
  return normalized.rfind(kCubeToken, 0) == 0 ||
         normalized.find("perastage_primitive_cube.glb") != std::string::npos ||
         normalized.find("perastage_primitive_cube.obj") != std::string::npos ||
         normalized.find("primitive_cube_") != std::string::npos;
}

bool IsCylinderRef(const std::string &normalized) {
  return normalized.rfind(kCylinderToken, 0) == 0 ||
         normalized.find("perastage_primitive_cylinder.glb") != std::string::npos ||
         normalized.find("primitive_cylinder_") != std::string::npos;
}

std::string SanitizeUuidForFileName(const std::string &objectUuid) {
  std::string out;
  out.reserve(objectUuid.size());
  for (char ch : objectUuid) {
    if (std::isalnum(static_cast<unsigned char>(ch)))
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    else
      out.push_back('_');
  }
  return out;
}

struct PrimitiveMeshData {
  std::vector<float> positions;
  std::vector<uint16_t> indices;
};

PrimitiveMeshData BuildCubeMesh() {
  PrimitiveMeshData mesh;
  mesh.positions = {
      -0.5f, -0.5f, -0.5f,
      0.5f,  -0.5f, -0.5f,
      0.5f,  0.5f,  -0.5f,
      -0.5f, 0.5f,  -0.5f,
      -0.5f, -0.5f, 0.5f,
      0.5f,  -0.5f, 0.5f,
      0.5f,  0.5f,  0.5f,
      -0.5f, 0.5f,  0.5f,
  };
  mesh.indices = {
      0, 1, 2, 0, 2, 3,
      4, 7, 6, 4, 6, 5,
      0, 4, 5, 0, 5, 1,
      1, 5, 6, 1, 6, 2,
      2, 6, 7, 2, 7, 3,
      3, 7, 4, 3, 4, 0,
  };
  return mesh;
}

PrimitiveMeshData BuildSphereMesh() {
  PrimitiveMeshData mesh;
  mesh.positions = {
      0.0f,  0.5f,  0.0f,
      0.353553f, 0.353553f, 0.0f,
      0.0f,  0.353553f, 0.353553f,
      -0.353553f, 0.353553f, 0.0f,
      0.0f,  0.353553f, -0.353553f,
      0.5f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.5f,
      -0.5f, 0.0f, 0.0f,
      0.0f, 0.0f, -0.5f,
      0.353553f, -0.353553f, 0.0f,
      0.0f, -0.353553f, 0.353553f,
      -0.353553f, -0.353553f, 0.0f,
      0.0f, -0.353553f, -0.353553f,
      0.0f, -0.5f, 0.0f,
  };
  mesh.indices = {
      0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
      1, 5, 2, 2, 6, 3, 3, 7, 4, 4, 8, 1,
      5, 9, 10, 5, 10, 6, 6, 10, 11, 6, 11, 7,
      7, 11, 12, 7, 12, 8, 8, 12, 9, 8, 9, 5,
      9, 13, 10, 10, 13, 11, 11, 13, 12, 12, 13, 9,
  };
  return mesh;
}

PrimitiveMeshData BuildCylinderMesh() {
  PrimitiveMeshData mesh;
  constexpr int kSegments = 16;
  constexpr float kPi = 3.14159265358979323846f;
  mesh.positions.reserve(static_cast<size_t>(kSegments * 2 + 2) * 3);
  mesh.indices.reserve(static_cast<size_t>(kSegments) * 12);

  for (int i = 0; i < kSegments; ++i) {
    const float a = static_cast<float>(i) * 2.0f * kPi /
                    static_cast<float>(kSegments);
    const float x = 0.5f * std::cos(a);
    const float z = 0.5f * std::sin(a);
    mesh.positions.insert(mesh.positions.end(), {x, 0.5f, z});
    mesh.positions.insert(mesh.positions.end(), {x, -0.5f, z});
  }
  const uint16_t topCenter = static_cast<uint16_t>(mesh.positions.size() / 3);
  mesh.positions.insert(mesh.positions.end(), {0.0f, 0.5f, 0.0f});
  const uint16_t bottomCenter = static_cast<uint16_t>(mesh.positions.size() / 3);
  mesh.positions.insert(mesh.positions.end(), {0.0f, -0.5f, 0.0f});

  for (int i = 0; i < kSegments; ++i) {
    const uint16_t top0 = static_cast<uint16_t>(i * 2);
    const uint16_t bot0 = static_cast<uint16_t>(i * 2 + 1);
    const uint16_t top1 = static_cast<uint16_t>(((i + 1) % kSegments) * 2);
    const uint16_t bot1 = static_cast<uint16_t>(((i + 1) % kSegments) * 2 + 1);

    mesh.indices.insert(mesh.indices.end(), {top0, bot0, bot1, top0, bot1, top1});
    mesh.indices.insert(mesh.indices.end(), {topCenter, top1, top0});
    mesh.indices.insert(mesh.indices.end(), {bottomCenter, bot0, bot1});
  }

  return mesh;
}

void AppendU32(std::vector<uint8_t> &buffer, uint32_t value) {
  buffer.push_back(static_cast<uint8_t>(value & 0xFFu));
  buffer.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
  buffer.push_back(static_cast<uint8_t>((value >> 16u) & 0xFFu));
  buffer.push_back(static_cast<uint8_t>((value >> 24u) & 0xFFu));
}

void AppendBytes(std::vector<uint8_t> &buffer, const void *data, size_t size) {
  const auto *ptr = static_cast<const uint8_t *>(data);
  buffer.insert(buffer.end(), ptr, ptr + size);
}

bool WriteGlb(const PrimitiveMeshData &mesh, const std::string &outputPath) {
  if (mesh.positions.empty() || mesh.indices.empty())
    return false;

  float minX = mesh.positions[0], minY = mesh.positions[1], minZ = mesh.positions[2];
  float maxX = minX, maxY = minY, maxZ = minZ;
  for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
    minX = std::min(minX, mesh.positions[i]);
    minY = std::min(minY, mesh.positions[i + 1]);
    minZ = std::min(minZ, mesh.positions[i + 2]);
    maxX = std::max(maxX, mesh.positions[i]);
    maxY = std::max(maxY, mesh.positions[i + 1]);
    maxZ = std::max(maxZ, mesh.positions[i + 2]);
  }

  const uint32_t positionBytes = static_cast<uint32_t>(mesh.positions.size() * sizeof(float));
  const uint32_t indexBytes = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint16_t));

  std::vector<uint8_t> bin;
  bin.reserve(positionBytes + indexBytes + 4);
  AppendBytes(bin, mesh.positions.data(), positionBytes);
  while ((bin.size() % 4u) != 0u)
    bin.push_back(0u);
  const uint32_t indexOffset = static_cast<uint32_t>(bin.size());
  AppendBytes(bin, mesh.indices.data(), indexBytes);
  while ((bin.size() % 4u) != 0u)
    bin.push_back(0u);
  const uint32_t totalBinBytes = static_cast<uint32_t>(bin.size());

  std::ostringstream json;
  json << "{"
       << "\"asset\":{\"version\":\"2.0\"},"
       << "\"buffers\":[{\"byteLength\":" << totalBinBytes << "}],"
       << "\"bufferViews\":["
       << "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << positionBytes
       << ",\"target\":34962},"
       << "{\"buffer\":0,\"byteOffset\":" << indexOffset
       << ",\"byteLength\":" << indexBytes << ",\"target\":34963}"
       << "],"
       << "\"accessors\":["
       << "{\"bufferView\":0,\"componentType\":5126,\"count\":"
       << (mesh.positions.size() / 3u)
       << ",\"type\":\"VEC3\",\"min\":[" << minX << "," << minY << "," << minZ
       << "],\"max\":[" << maxX << "," << maxY << "," << maxZ << "]},"
       << "{\"bufferView\":1,\"componentType\":5123,\"count\":"
       << mesh.indices.size() << ",\"type\":\"SCALAR\"}"
       << "],"
       << "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
       << "\"nodes\":[{\"mesh\":0}],"
       << "\"scenes\":[{\"nodes\":[0]}],"
       << "\"scene\":0"
       << "}";

  std::string jsonText = json.str();
  while ((jsonText.size() % 4u) != 0u)
    jsonText.push_back(' ');

  const uint32_t jsonLen = static_cast<uint32_t>(jsonText.size());
  const uint32_t totalLen = 12u + 8u + jsonLen + 8u + totalBinBytes;

  std::vector<uint8_t> glb;
  glb.reserve(totalLen);
  AppendU32(glb, 0x46546C67u);
  AppendU32(glb, 2u);
  AppendU32(glb, totalLen);

  AppendU32(glb, jsonLen);
  AppendU32(glb, 0x4E4F534Au);
  AppendBytes(glb, jsonText.data(), jsonText.size());

  AppendU32(glb, totalBinBytes);
  AppendU32(glb, 0x004E4942u);
  AppendBytes(glb, bin.data(), bin.size());

  std::error_code ec;
  const fs::path output = fs::u8path(outputPath);
  if (output.has_parent_path())
    fs::create_directories(output.parent_path(), ec);

  std::ofstream out(output, std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;
  out.write(reinterpret_cast<const char *>(glb.data()),
            static_cast<std::streamsize>(glb.size()));
  return out.good();
}

} // namespace

bool ResolvePrimitiveTokenFromModelRef(const std::string &modelRef,
                                       std::string &outPrimitiveToken) {
  const std::string normalized = NormalizeLower(modelRef);
  if (IsSphereRef(normalized)) {
    outPrimitiveToken = kSphereToken;
    return true;
  }
  if (IsCubeRef(normalized)) {
    outPrimitiveToken = kCubeToken;
    return true;
  }
  if (IsCylinderRef(normalized)) {
    outPrimitiveToken = kCylinderToken;
    return true;
  }
  return false;
}

std::string PrimitiveArchivePathForToken(const std::string &primitiveToken) {
  const std::string normalized = NormalizeLower(primitiveToken);
  if (normalized.rfind(kSphereToken, 0) == 0)
    return kSphereArchiveName;
  if (normalized.rfind(kCubeToken, 0) == 0)
    return kCubeArchiveName;
  if (normalized.rfind(kCylinderToken, 0) == 0)
    return kCylinderArchiveName;
  return {};
}

std::string PrimitiveArchivePathForToken(const std::string &primitiveToken,
                                         const std::string &objectUuid) {
  const std::string normalized = NormalizeLower(primitiveToken);
  const std::string suffix = SanitizeUuidForFileName(objectUuid);
  if (suffix.empty())
    return PrimitiveArchivePathForToken(primitiveToken);
  if (normalized.rfind(kSphereToken, 0) == 0)
    return "primitive_sphere_" + suffix + ".glb";
  if (normalized.rfind(kCubeToken, 0) == 0)
    return "primitive_cube_" + suffix + ".glb";
  if (normalized.rfind(kCylinderToken, 0) == 0)
    return "primitive_cylinder_" + suffix + ".glb";
  return {};
}

bool WritePrimitiveModelForToken(const std::string &primitiveToken,
                                 const std::string &outputPath) {
  const std::string normalized = NormalizeLower(primitiveToken);
  if (normalized.rfind(kCubeToken, 0) == 0)
    return WriteGlb(BuildCubeMesh(), outputPath);
  if (normalized.rfind(kSphereToken, 0) == 0)
    return WriteGlb(BuildSphereMesh(), outputPath);
  if (normalized.rfind(kCylinderToken, 0) == 0)
    return WriteGlb(BuildCylinderMesh(), outputPath);
  return false;
}

} // namespace mvr
