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

enum class CylinderAxis {
  X,
  Y,
  Z,
};

PrimitiveMeshData BuildCubeMesh() {
  PrimitiveMeshData mesh;
  mesh.positions = {
      // -Z face
      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,
      -0.5f,
      // +Z face
      -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,
      0.5f,
      // -Y face
      -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f,
      -0.5f,
      // +X face
      0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
      -0.5f,
      // +Y face
      -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,
      0.5f,
      // -X face
      -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f,
      0.5f,
  };
  mesh.indices = {
      0, 2, 1,   0, 3, 2,    // -Z
      4, 5, 6,   4, 6, 7,    // +Z
      8, 9, 10,  8, 10, 11,  // -Y
      12, 14, 13, 12, 15, 14, // +X
      16, 18, 17, 16, 19, 18, // +Y
      20, 22, 21, 20, 23, 22, // -X
  };
  return mesh;
}

PrimitiveMeshData BuildSphereMesh() {
  PrimitiveMeshData mesh;
  constexpr int kRings = 12;
  constexpr int kSegments = 24;
  constexpr float kRadius = 0.5f;
  constexpr float kPi = 3.14159265358979323846f;

  mesh.positions.reserve(static_cast<size_t>((kRings + 1) * (kSegments + 1) * 3));
  mesh.indices.reserve(static_cast<size_t>(kRings * kSegments * 6));

  for (int ring = 0; ring <= kRings; ++ring) {
    const float v = static_cast<float>(ring) / static_cast<float>(kRings);
    const float phi = v * kPi;
    const float z = std::cos(phi) * kRadius;
    const float ringRadius = std::sin(phi) * kRadius;
    for (int segment = 0; segment <= kSegments; ++segment) {
      const float u = static_cast<float>(segment) / static_cast<float>(kSegments);
      const float theta = u * 2.0f * kPi;
      mesh.positions.push_back(std::cos(theta) * ringRadius);
      mesh.positions.push_back(std::sin(theta) * ringRadius);
      mesh.positions.push_back(z);
    }
  }

  for (int ring = 0; ring < kRings; ++ring) {
    for (int segment = 0; segment < kSegments; ++segment) {
      const uint16_t i0 =
          static_cast<uint16_t>(ring * (kSegments + 1) + segment);
      const uint16_t i1 = static_cast<uint16_t>(i0 + kSegments + 1);
      const uint16_t i2 = static_cast<uint16_t>(i0 + 1);
      const uint16_t i3 = static_cast<uint16_t>(i1 + 1);

      mesh.indices.insert(mesh.indices.end(), {i0, i1, i2});
      mesh.indices.insert(mesh.indices.end(), {i2, i1, i3});
    }
  }

  return mesh;
}

PrimitiveMeshData BuildCylinderMesh(float topRadius, float bottomRadius,
                                    float height, CylinderAxis axis) {
  PrimitiveMeshData mesh;
  constexpr int kSegments = 32;
  constexpr float kPi = 3.14159265358979323846f;
  mesh.positions.reserve(static_cast<size_t>(kSegments * 6 + 2) * 3);
  mesh.indices.reserve(static_cast<size_t>(kSegments) * 12);

  const float halfHeight = 0.5f * height;

  auto appendAxisPosition = [&](float axial, float radialA, float radialB) {
    if (axis == CylinderAxis::X) {
      mesh.positions.insert(mesh.positions.end(), {axial, radialA, radialB});
    } else if (axis == CylinderAxis::Y) {
      mesh.positions.insert(mesh.positions.end(), {radialA, axial, radialB});
    } else {
      mesh.positions.insert(mesh.positions.end(), {radialA, radialB, axial});
    }
  };

  // Side vertices (separate from cap vertices to keep hard normals on edges).
  const uint16_t sideBase = 0;
  for (int i = 0; i < kSegments; ++i) {
    const float a = static_cast<float>(i) * 2.0f * kPi /
                    static_cast<float>(kSegments);
    const float xTop = topRadius * std::cos(a);
    const float yTop = topRadius * std::sin(a);
    const float xBottom = bottomRadius * std::cos(a);
    const float yBottom = bottomRadius * std::sin(a);
    appendAxisPosition(halfHeight, xTop, yTop);
    appendAxisPosition(-halfHeight, xBottom, yBottom);
  }
  for (int i = 0; i < kSegments; ++i) {
    const uint16_t top0 = static_cast<uint16_t>(sideBase + i * 2);
    const uint16_t bot0 = static_cast<uint16_t>(sideBase + i * 2 + 1);
    const uint16_t top1 =
        static_cast<uint16_t>(sideBase + ((i + 1) % kSegments) * 2);
    const uint16_t bot1 =
        static_cast<uint16_t>(sideBase + ((i + 1) % kSegments) * 2 + 1);
    // Keep counter-clockwise winding for outward normals in the right-handed MVR space.
    mesh.indices.insert(mesh.indices.end(), {top0, bot1, bot0, top0, top1, bot1});
  }

  // Top cap vertices.
  const uint16_t topCapBase = static_cast<uint16_t>(mesh.positions.size() / 3);
  for (int i = 0; i < kSegments; ++i) {
    const float a = static_cast<float>(i) * 2.0f * kPi /
                    static_cast<float>(kSegments);
    appendAxisPosition(halfHeight, topRadius * std::cos(a), topRadius * std::sin(a));
  }
  const uint16_t topCenter = static_cast<uint16_t>(mesh.positions.size() / 3);
  appendAxisPosition(halfHeight, 0.0f, 0.0f);
  for (int i = 0; i < kSegments; ++i) {
    const uint16_t top0 = static_cast<uint16_t>(topCapBase + i);
    const uint16_t top1 = static_cast<uint16_t>(topCapBase + ((i + 1) % kSegments));
    mesh.indices.insert(mesh.indices.end(), {topCenter, top1, top0});
  }

  // Bottom cap vertices.
  const uint16_t bottomCapBase = static_cast<uint16_t>(mesh.positions.size() / 3);
  for (int i = 0; i < kSegments; ++i) {
    const float a = static_cast<float>(i) * 2.0f * kPi /
                    static_cast<float>(kSegments);
    appendAxisPosition(-halfHeight, bottomRadius * std::cos(a),
                       bottomRadius * std::sin(a));
  }
  const uint16_t bottomCenter = static_cast<uint16_t>(mesh.positions.size() / 3);
  appendAxisPosition(-halfHeight, 0.0f, 0.0f);
  for (int i = 0; i < kSegments; ++i) {
    const uint16_t bot0 = static_cast<uint16_t>(bottomCapBase + i);
    const uint16_t bot1 =
        static_cast<uint16_t>(bottomCapBase + ((i + 1) % kSegments));
    mesh.indices.insert(mesh.indices.end(), {bottomCenter, bot0, bot1});
  }

  return mesh;
}

PrimitiveMeshData BuildCylinderMesh() {
  return BuildCylinderMesh(0.5f, 0.5f, 1.0f, CylinderAxis::Y);
}

float ParsePositiveNumber(const std::string &text, float fallback) {
  if (text.empty())
    return fallback;
  try {
    const float value = std::stof(text);
    return std::isfinite(value) && value > 0.0f ? value : fallback;
  } catch (...) {
    return fallback;
  }
}

void ParseCylinderTokenDimensions(const std::string &token, float &topRadius,
                                  float &bottomRadius, float &height,
                                  CylinderAxis &axis) {
  topRadius = 0.5f;
  bottomRadius = 0.5f;
  height = 1.0f;
  axis = CylinderAxis::Y;

  const std::string normalized = NormalizeLower(token);
  if (normalized.rfind(kCylinderToken, 0) != 0)
    return;

  const size_t separator = normalized.find(';');
  if (separator == std::string::npos || separator + 1 >= normalized.size())
    return;

  std::stringstream ss(normalized.substr(separator + 1));
  std::string field;
  while (std::getline(ss, field, ';')) {
    const size_t equalPos = field.find('=');
    if (equalPos == std::string::npos)
      continue;
    const std::string key = field.substr(0, equalPos);
    const std::string value = field.substr(equalPos + 1);
    if (key == "top") {
      topRadius = ParsePositiveNumber(value, topRadius);
    } else if (key == "bottom") {
      bottomRadius = ParsePositiveNumber(value, bottomRadius);
    } else if (key == "height") {
      height = ParsePositiveNumber(value, height);
    } else if (key == "axis") {
      if (value == "x")
        axis = CylinderAxis::X;
      else if (value == "y")
        axis = CylinderAxis::Y;
      else if (value == "z")
        axis = CylinderAxis::Z;
    }
  }

  // Perastage stores explicit primitive:cylinder dimensions in millimeters.
  // Some legacy/internal paths may still provide meters. Normalize to meters:
  // treat clearly large values as millimeters.
  const float largestDimension = std::max({topRadius, bottomRadius, height});
  if (largestDimension > 20.0f) {
    constexpr float kMillimetersPerMeter = 1000.0f;
    topRadius /= kMillimetersPerMeter;
    bottomRadius /= kMillimetersPerMeter;
    height /= kMillimetersPerMeter;
  }
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
  std::vector<float> normals(mesh.positions.size(), 0.0f);
  for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const uint16_t ia = mesh.indices[i];
    const uint16_t ib = mesh.indices[i + 1];
    const uint16_t ic = mesh.indices[i + 2];
    const size_t a = static_cast<size_t>(ia) * 3u;
    const size_t b = static_cast<size_t>(ib) * 3u;
    const size_t c = static_cast<size_t>(ic) * 3u;
    if (c + 2 >= mesh.positions.size())
      continue;

    const float abx = mesh.positions[b] - mesh.positions[a];
    const float aby = mesh.positions[b + 1] - mesh.positions[a + 1];
    const float abz = mesh.positions[b + 2] - mesh.positions[a + 2];
    const float acx = mesh.positions[c] - mesh.positions[a];
    const float acy = mesh.positions[c + 1] - mesh.positions[a + 1];
    const float acz = mesh.positions[c + 2] - mesh.positions[a + 2];

    const float nx = aby * acz - abz * acy;
    const float ny = abz * acx - abx * acz;
    const float nz = abx * acy - aby * acx;
    normals[a] += nx;
    normals[a + 1] += ny;
    normals[a + 2] += nz;
    normals[b] += nx;
    normals[b + 1] += ny;
    normals[b + 2] += nz;
    normals[c] += nx;
    normals[c + 1] += ny;
    normals[c + 2] += nz;
  }
  for (size_t i = 0; i + 2 < normals.size(); i += 3) {
    const float len =
        std::sqrt(normals[i] * normals[i] + normals[i + 1] * normals[i + 1] +
                  normals[i + 2] * normals[i + 2]);
    if (len > 1e-8f) {
      normals[i] /= len;
      normals[i + 1] /= len;
      normals[i + 2] /= len;
    } else {
      normals[i] = 0.0f;
      normals[i + 1] = 0.0f;
      normals[i + 2] = 1.0f;
    }
  }
  const uint32_t normalBytes = static_cast<uint32_t>(normals.size() * sizeof(float));
  const uint32_t indexBytes = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint16_t));

  std::vector<uint8_t> bin;
  bin.reserve(positionBytes + normalBytes + indexBytes + 8);
  AppendBytes(bin, mesh.positions.data(), positionBytes);
  while ((bin.size() % 4u) != 0u)
    bin.push_back(0u);
  const uint32_t normalOffset = static_cast<uint32_t>(bin.size());
  AppendBytes(bin, normals.data(), normalBytes);
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
       << "{\"buffer\":0,\"byteOffset\":" << normalOffset
       << ",\"byteLength\":" << normalBytes << ",\"target\":34962},"
       << "{\"buffer\":0,\"byteOffset\":" << indexOffset
       << ",\"byteLength\":" << indexBytes << ",\"target\":34963}"
       << "],"
       << "\"accessors\":["
       << "{\"bufferView\":0,\"componentType\":5126,\"count\":"
       << (mesh.positions.size() / 3u)
       << ",\"type\":\"VEC3\",\"min\":[" << minX << "," << minY << "," << minZ
       << "],\"max\":[" << maxX << "," << maxY << "," << maxZ << "]},"
       << "{\"bufferView\":1,\"componentType\":5126,\"count\":"
       << (normals.size() / 3u) << ",\"type\":\"VEC3\"},"
       << "{\"bufferView\":2,\"componentType\":5123,\"count\":"
       << mesh.indices.size() << ",\"type\":\"SCALAR\"}"
       << "],"
       << "\"materials\":[{\"doubleSided\":true}],"
       << "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},\"indices\":2,\"material\":0}]}],"
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
  if (IsCubeRef(normalized))
    return WriteGlb(BuildCubeMesh(), outputPath);
  if (IsSphereRef(normalized))
    return WriteGlb(BuildSphereMesh(), outputPath);
  if (IsCylinderRef(normalized)) {
    float topRadius = 0.5f;
    float bottomRadius = 0.5f;
    float height = 1.0f;
    CylinderAxis axis = CylinderAxis::Z;
    if (normalized.rfind(kCylinderToken, 0) == 0)
      ParseCylinderTokenDimensions(primitiveToken, topRadius, bottomRadius, height, axis);
    return WriteGlb(BuildCylinderMesh(topRadius, bottomRadius, height, axis), outputPath);
  }
  return false;
}

} // namespace mvr
