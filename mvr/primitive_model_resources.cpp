#include "primitive_model_resources.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace mvr {
namespace {

constexpr const char *kSphereToken = "primitive:sphere";
constexpr const char *kCubeToken = "primitive:cube";
constexpr const char *kSphereArchiveName = "primitives/perastage_primitive_sphere.obj";
constexpr const char *kCubeArchiveName = "primitives/perastage_primitive_cube.obj";

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
         normalized.find("perastage_primitive_sphere.obj") != std::string::npos;
}

bool IsCubeRef(const std::string &normalized) {
  return normalized.rfind(kCubeToken, 0) == 0 ||
         normalized.find("perastage_primitive_cube.obj") != std::string::npos;
}

const char *ObjDataForToken(const std::string &primitiveToken) {
  const std::string normalized = NormalizeLower(primitiveToken);
  if (normalized.rfind(kCubeToken, 0) == 0) {
    return R"OBJ(o PerastagePrimitiveCube
v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
f 1 2 3
f 1 3 4
f 5 8 7
f 5 7 6
f 1 5 6
f 1 6 2
f 2 6 7
f 2 7 3
f 3 7 8
f 3 8 4
f 4 8 5
f 4 5 1
)OBJ";
  }

  if (normalized.rfind(kSphereToken, 0) == 0) {
    return R"OBJ(o PerastagePrimitiveSphere
v 0 0.5 0
v 0.353553 0.353553 0
v 0 0.353553 0.353553
v -0.353553 0.353553 0
v 0 0.353553 -0.353553
v 0.5 0 0
v 0 0 0.5
v -0.5 0 0
v 0 0 -0.5
v 0.353553 -0.353553 0
v 0 -0.353553 0.353553
v -0.353553 -0.353553 0
v 0 -0.353553 -0.353553
v 0 -0.5 0
f 1 2 3
f 1 3 4
f 1 4 5
f 1 5 2
f 2 6 3
f 3 7 4
f 4 8 5
f 5 9 2
f 6 10 11
f 6 11 7
f 7 11 12
f 7 12 8
f 8 12 13
f 8 13 9
f 9 13 10
f 9 10 6
f 10 14 11
f 11 14 12
f 12 14 13
f 13 14 10
)OBJ";
  }

  return nullptr;
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
  return false;
}

std::string PrimitiveArchivePathForToken(const std::string &primitiveToken) {
  const std::string normalized = NormalizeLower(primitiveToken);
  if (normalized.rfind(kSphereToken, 0) == 0)
    return kSphereArchiveName;
  if (normalized.rfind(kCubeToken, 0) == 0)
    return kCubeArchiveName;
  return {};
}

bool WritePrimitiveObjForToken(const std::string &primitiveToken,
                               const std::string &outputPath) {
  const char *objData = ObjDataForToken(primitiveToken);
  if (objData == nullptr)
    return false;

  std::error_code ec;
  const fs::path output = fs::u8path(outputPath);
  if (output.has_parent_path())
    fs::create_directories(output.parent_path(), ec);

  std::ofstream out(output, std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;

  out << objData;
  return out.good();
}

} // namespace mvr
