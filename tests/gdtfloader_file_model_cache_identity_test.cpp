#include "../viewer3d/gdtfloader.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {

struct MeshSize {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

// Appends a trivially copyable value to a binary string.
template <typename T> void AppendBinary(std::string &bytes, const T &value) {
  const char *data = reinterpret_cast<const char *>(&value);
  bytes.append(data, sizeof(T));
}

// Builds a deterministic GLB containing a unit cuboid mesh.
std::string BuildSharedGlb() {
  const std::array<float, 24> positions = {
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};
  const std::array<std::uint16_t, 36> indices = {
      0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1,
      1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0};
  std::string binary;
  for (float value : positions)
    AppendBinary(binary, value);
  for (std::uint16_t value : indices)
    AppendBinary(binary, value);

  std::ostringstream json;
  json << "{\"asset\":{\"version\":\"2.0\"},"
          "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
          "\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{"
          "\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
          "\"buffers\":[{\"byteLength\":"
       << binary.size()
       << "}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,"
          "\"byteLength\":96,\"target\":34962},{\"buffer\":0,"
          "\"byteOffset\":96,\"byteLength\":72,\"target\":34963}],"
          "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
          "\"count\":8,\"type\":\"VEC3\",\"min\":[0,0,0],"
          "\"max\":[1,1,1]},{\"bufferView\":1,\"componentType\":5123,"
          "\"count\":36,\"type\":\"SCALAR\"}]}";
  std::string jsonBytes = json.str();
  while (jsonBytes.size() % 4 != 0)
    jsonBytes.push_back(' ');
  while (binary.size() % 4 != 0)
    binary.push_back('\0');

  std::string glb;
  const std::uint32_t totalLength =
      static_cast<std::uint32_t>(12 + 8 + jsonBytes.size() + 8 + binary.size());
  AppendBinary(glb, std::uint32_t{0x46546c67});
  AppendBinary(glb, std::uint32_t{2});
  AppendBinary(glb, totalLength);
  AppendBinary(glb, static_cast<std::uint32_t>(jsonBytes.size()));
  AppendBinary(glb, std::uint32_t{0x4e4f534a});
  glb += jsonBytes;
  AppendBinary(glb, static_cast<std::uint32_t>(binary.size()));
  AppendBinary(glb, std::uint32_t{0x004e4942});
  glb += binary;
  return glb;
}

// Writes the deterministic multi-model GDTF under a unique cache identity.
std::string WriteGdtf(const std::string &prefix) {
  wxFileName temporary(wxFileName::CreateTempFileName(prefix));
  const std::string path = temporary.GetFullPath().ToStdString() + ".gdtf";
  wxRemoveFile(temporary.GetFullPath());
  const std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"CacheIdentity\">"
      "<Models>"
      "<Model Name=\"ModelA\" File=\"shared\" Length=\"1.0\" Width=\"0.5\" Height=\"0.25\"/>"
      "<Model Name=\"ModelB\" File=\"shared\" Length=\"0.2\" Width=\"0.8\" Height=\"0.4\"/>"
      "</Models><Geometries>"
      "<Geometry Name=\"Both\"><Geometry Name=\"PartA\" Model=\"ModelA\"/>"
      "<Geometry Name=\"PartB\" Model=\"ModelB\"/></Geometry>"
      "<Geometry Name=\"RootA\" Model=\"ModelA\"/>"
      "<Geometry Name=\"RootB\" Model=\"ModelB\"/>"
      "</Geometries><DMXModes>"
      "<DMXMode Name=\"ModeA\" Geometry=\"RootA\"/>"
      "<DMXMode Name=\"ModeB\" Geometry=\"RootB\"/>"
      "<DMXMode Name=\"ModeBoth\" Geometry=\"Both\"/>"
      "</DMXModes></FixtureType></GDTF>";
  wxFFileOutputStream output(path);
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  zip.PutNextEntry("description.xml");
  zip.Write(xml.data(), xml.size());
  zip.PutNextEntry("models/gltf/shared.glb");
  const std::string glb = BuildSharedGlb();
  zip.Write(glb.data(), glb.size());
  zip.Close();
  return path;
}

// Computes axis-aligned mesh dimensions from vertex coordinates.
MeshSize GetMeshSize(const Mesh &mesh) {
  assert(mesh.vertices.size() >= 3);
  MeshSize minimum{mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]};
  MeshSize maximum = minimum;
  for (std::size_t index = 3; index + 2 < mesh.vertices.size(); index += 3) {
    minimum.x = std::min(minimum.x, mesh.vertices[index]);
    minimum.y = std::min(minimum.y, mesh.vertices[index + 1]);
    minimum.z = std::min(minimum.z, mesh.vertices[index + 2]);
    maximum.x = std::max(maximum.x, mesh.vertices[index]);
    maximum.y = std::max(maximum.y, mesh.vertices[index + 1]);
    maximum.z = std::max(maximum.z, mesh.vertices[index + 2]);
  }
  return {maximum.x - minimum.x, maximum.y - minimum.y,
          maximum.z - minimum.z};
}

// Returns true when dimensions are equal within loader precision.
bool NearlyEqual(float left, float right) {
  return std::fabs(left - right) < 0.01f;
}

// Builds a deterministic structural signature for loaded GDTF objects.
std::string BuildSignature(const std::vector<GdtfObject> &objects) {
  std::ostringstream signature;
  signature.precision(9);
  signature << objects.size();
  for (const GdtfObject &object : objects) {
    const MeshSize size = GetMeshSize(object.mesh);
    signature << '|' << object.mesh.vertices.size() << ','
              << object.mesh.indices.size() << ',' << size.x << ',' << size.y
              << ',' << size.z;
    for (float value : object.transform.o)
      signature << ',' << value;
    std::uint64_t hash = 1469598103934665603ULL;
    for (float value : object.mesh.vertices) {
      std::uint32_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      hash = (hash ^ bits) * 1099511628211ULL;
    }
    signature << ',' << hash;
  }
  return signature.str();
}

// Loads one exact mode and requires production loader success.
std::vector<GdtfObject> LoadMode(const std::string &path,
                                 const std::string &mode) {
  std::vector<GdtfObject> objects;
  std::string error;
  assert(LoadGdtf(path, objects, mode, &error));
  return objects;
}

} // namespace

// Proves file-backed model dimensions and mode load order are cache-independent.
int main() {
  const std::string sameModePath = WriteGdtf("gdtf_model_cache_same_mode_");
  const std::string orderAbPath = WriteGdtf("gdtf_model_cache_order_ab_");
  const std::string orderBaPath = WriteGdtf("gdtf_model_cache_order_ba_");
  const std::string coldAPath = WriteGdtf("gdtf_model_cache_cold_a_");
  const std::string coldBPath = WriteGdtf("gdtf_model_cache_cold_b_");
  const std::string defaultPath = WriteGdtf("gdtf_model_cache_default_");

  const auto both = LoadMode(sameModePath, "ModeBoth");
  assert(both.size() == 2);
  const MeshSize modelA = GetMeshSize(both[0].mesh);
  const MeshSize modelB = GetMeshSize(both[1].mesh);
  assert(NearlyEqual(modelA.x, 1000.0f) && NearlyEqual(modelA.y, 500.0f) &&
         NearlyEqual(modelA.z, 250.0f));
  assert(NearlyEqual(modelB.x, 200.0f) && NearlyEqual(modelB.y, 800.0f) &&
         NearlyEqual(modelB.z, 400.0f));

  LoadMode(orderAbPath, "ModeA");
  const auto warmB = LoadMode(orderAbPath, "ModeB");
  const auto coldB = LoadMode(coldBPath, "ModeB");
  assert(BuildSignature(warmB) == BuildSignature(coldB));
  LoadMode(orderBaPath, "ModeB");
  const auto warmA = LoadMode(orderBaPath, "ModeA");
  const auto coldA = LoadMode(coldAPath, "ModeA");
  assert(BuildSignature(warmA) == BuildSignature(coldA));

  std::vector<GdtfObject> defaultObjects;
  std::string error;
  assert(LoadGdtf(defaultPath, defaultObjects, &error));
  const auto defaultThenB = LoadMode(defaultPath, "ModeB");
  assert(BuildSignature(defaultThenB) == BuildSignature(coldB));
  assert(BuildSignature(LoadMode(defaultPath, "ModeB")) ==
         BuildSignature(coldB));

  for (const std::string &path :
       {sameModePath, orderAbPath, orderBaPath, coldAPath, coldBPath,
        defaultPath}) {
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
  }
  return 0;
}
