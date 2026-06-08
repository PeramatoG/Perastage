#include "loader_obj.h"

#include <algorithm>
#include <charconv>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

struct ObjIndex {
  int v = -1;
  int vt = -1;
  int vn = -1;

  // Compares two OBJ vertex index tuples to support hash-map deduplication.
  bool operator==(const ObjIndex &other) const {
    return v == other.v && vt == other.vt && vn == other.vn;
  }
};

struct ObjIndexHash {
  // Produces a stable hash for combined OBJ position/UV/normal indices.
  size_t operator()(const ObjIndex &idx) const {
    return (static_cast<size_t>(idx.v + 1) * 73856093u) ^
           (static_cast<size_t>(idx.vt + 1) * 19349663u) ^
           (static_cast<size_t>(idx.vn + 1) * 83492791u);
  }
};

// Parses a signed OBJ index token and resolves it to a zero-based absolute index.
bool ParseObjIndexToken(const std::string &token, int count, int &outIndex) {
  if (token.empty())
    return false;

  int raw = 0;
  const char *begin = token.data();
  const char *end = begin + token.size();
  const std::from_chars_result result = std::from_chars(begin, end, raw);
  if (result.ec != std::errc{} || result.ptr != end)
    return false;

  if (raw > 0)
    outIndex = raw - 1;
  else if (raw < 0)
    outIndex = count + raw;
  else
    return false;
  return outIndex >= 0 && outIndex < count;
}

// Parses one OBJ face vertex entry in the form v, v/vt, v//vn, or v/vt/vn.
bool ParseFaceVertexToken(const std::string &token, int vCount, int vtCount,
                          int vnCount, ObjIndex &outIndex) {
  std::array<std::string, 3> parts;
  size_t partIndex = 0;
  size_t start = 0;
  for (size_t i = 0; i <= token.size() && partIndex < parts.size(); ++i) {
    if (i == token.size() || token[i] == '/') {
      parts[partIndex++] = token.substr(start, i - start);
      start = i + 1;
    }
  }
  if (parts[0].empty() || !ParseObjIndexToken(parts[0], vCount, outIndex.v))
    return false;
  if (!parts[1].empty() && !ParseObjIndexToken(parts[1], vtCount, outIndex.vt))
    return false;
  if (!parts[2].empty() && !ParseObjIndexToken(parts[2], vnCount, outIndex.vn))
    return false;
  return true;
}

// Writes little-endian uint32 values into a binary output buffer.
void AppendU32(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

// Appends arbitrary raw bytes into a binary output buffer.
void AppendBytes(std::vector<uint8_t> &out, const void *data, size_t size) {
  const uint8_t *begin = static_cast<const uint8_t *>(data);
  out.insert(out.end(), begin, begin + size);
}

// Pads a binary chunk to the required 4-byte alignment.
void PadTo4(std::vector<uint8_t> &out, uint8_t fillByte) {
  while ((out.size() % 4u) != 0u)
    out.push_back(fillByte);
}

} // namespace

// Loads an OBJ mesh and guarantees usable normals/UVs/material defaults for runtime rendering.
bool LoadOBJ(const std::string &path, Mesh &outMesh, std::string *errorMessage) {
  outMesh.vertices.clear();
  outMesh.indices.clear();
  outMesh.normals.clear();
  outMesh.texcoords.clear();
  outMesh.textureRgba.clear();
  outMesh.textureWidth = 0;
  outMesh.textureHeight = 0;

  std::ifstream file(path);
  if (!file.is_open()) {
    if (errorMessage)
      *errorMessage = "could not open OBJ file";
    return false;
  }

  std::vector<std::array<float, 3>> positions;
  std::vector<std::array<float, 3>> normals;
  std::vector<std::array<float, 2>> texcoords;

  std::unordered_map<ObjIndex, uint32_t, ObjIndexHash> uniqueMap;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd == "v") {
      std::array<float, 3> p{};
      if (!(iss >> p[0] >> p[1] >> p[2]))
        continue;
      positions.push_back(p);
    } else if (cmd == "vn") {
      std::array<float, 3> n{};
      if (!(iss >> n[0] >> n[1] >> n[2]))
        continue;
      normals.push_back(n);
    } else if (cmd == "vt") {
      std::array<float, 2> t{};
      if (!(iss >> t[0] >> t[1]))
        continue;
      texcoords.push_back(t);
    } else if (cmd == "f") {
      std::vector<ObjIndex> face;
      std::string token;
      while (iss >> token) {
        ObjIndex idx;
        if (!ParseFaceVertexToken(token, static_cast<int>(positions.size()),
                                  static_cast<int>(texcoords.size()),
                                  static_cast<int>(normals.size()), idx)) {
          if (errorMessage)
            *errorMessage = "invalid face index in OBJ";
          return false;
        }
        face.push_back(idx);
      }
      if (face.size() < 3)
        continue;
      for (size_t i = 1; i + 1 < face.size(); ++i) {
        const ObjIndex tri[3] = {face[0], face[i], face[i + 1]};
        for (const ObjIndex &corner : tri) {
          auto it = uniqueMap.find(corner);
          if (it == uniqueMap.end()) {
            const uint32_t newIndex = static_cast<uint32_t>(outMesh.vertices.size() / 3);
            uniqueMap.emplace(corner, newIndex);
            const auto &p = positions[static_cast<size_t>(corner.v)];
            outMesh.vertices.insert(outMesh.vertices.end(), {p[0], p[1], p[2]});
            if (corner.vn >= 0) {
              const auto &n = normals[static_cast<size_t>(corner.vn)];
              outMesh.normals.insert(outMesh.normals.end(), {n[0], n[1], n[2]});
            }
            if (corner.vt >= 0) {
              const auto &t = texcoords[static_cast<size_t>(corner.vt)];
              outMesh.texcoords.insert(outMesh.texcoords.end(), {t[0], 1.0f - t[1]});
            }
            outMesh.indices.push_back(newIndex);
          } else {
            outMesh.indices.push_back(it->second);
          }
        }
      }
    }
  }

  if (outMesh.vertices.empty() || outMesh.indices.empty()) {
    if (errorMessage)
      *errorMessage = "OBJ does not contain triangulatable geometry";
    return false;
  }

  const size_t vertexCount = outMesh.vertices.size() / 3;
  if (outMesh.normals.size() != outMesh.vertices.size())
    ComputeNormals(outMesh);
  if (outMesh.texcoords.size() != vertexCount * 2)
    outMesh.texcoords.assign(vertexCount * 2, 0.0f);

  outMesh.materialBaseColor = {0.85f, 0.85f, 0.85f};
  outMesh.hasMaterialBaseColor = true;
  return true;
}

// Converts an OBJ mesh into a compact GLB file that can be embedded into MVR exports.
bool ConvertObjFileToGlb(const std::string &objPath, const std::string &glbPath,
                         std::string *errorMessage) {
  Mesh mesh;
  if (!LoadOBJ(objPath, mesh, errorMessage))
    return false;

  std::vector<uint8_t> bin;
  const auto appendAligned = [&](const void *ptr, size_t bytes) -> uint32_t {
    const uint32_t offset = static_cast<uint32_t>(bin.size());
    AppendBytes(bin, ptr, bytes);
    PadTo4(bin, 0);
    return offset;
  };

  const uint32_t posOffset = appendAligned(mesh.vertices.data(), mesh.vertices.size() * sizeof(float));
  const uint32_t normOffset = appendAligned(mesh.normals.data(), mesh.normals.size() * sizeof(float));
  const uint32_t uvOffset = appendAligned(mesh.texcoords.data(), mesh.texcoords.size() * sizeof(float));
  const uint32_t idxOffset = appendAligned(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

  const size_t vertexCount = mesh.vertices.size() / 3;
  std::ostringstream json;
  json << "{\"asset\":{\"version\":\"2.0\"},"
       << "\"buffers\":[{\"byteLength\":" << bin.size() << "}],"
       << "\"bufferViews\":["
       << "{\"buffer\":0,\"byteOffset\":" << posOffset << ",\"byteLength\":" << mesh.vertices.size() * sizeof(float) << "},"
       << "{\"buffer\":0,\"byteOffset\":" << normOffset << ",\"byteLength\":" << mesh.normals.size() * sizeof(float) << "},"
       << "{\"buffer\":0,\"byteOffset\":" << uvOffset << ",\"byteLength\":" << mesh.texcoords.size() * sizeof(float) << "},"
       << "{\"buffer\":0,\"byteOffset\":" << idxOffset << ",\"byteLength\":" << mesh.indices.size() * sizeof(uint32_t) << "}],"
       << "\"accessors\":["
       << "{\"bufferView\":0,\"componentType\":5126,\"count\":" << vertexCount << ",\"type\":\"VEC3\"},"
       << "{\"bufferView\":1,\"componentType\":5126,\"count\":" << vertexCount << ",\"type\":\"VEC3\"},"
       << "{\"bufferView\":2,\"componentType\":5126,\"count\":" << vertexCount << ",\"type\":\"VEC2\"},"
       << "{\"bufferView\":3,\"componentType\":5125,\"count\":" << mesh.indices.size() << ",\"type\":\"SCALAR\"}],"
       << "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.85,0.85,0.85,1.0],\"metallicFactor\":0.0,\"roughnessFactor\":1.0}}],"
       << "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}],"
       << "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";

  std::string jsonText = json.str();
  while ((jsonText.size() % 4u) != 0u)
    jsonText.push_back(' ');

  std::vector<uint8_t> glb;
  const uint32_t totalLen = static_cast<uint32_t>(12u + 8u + jsonText.size() + 8u + bin.size());
  AppendU32(glb, 0x46546C67u);
  AppendU32(glb, 2u);
  AppendU32(glb, totalLen);
  AppendU32(glb, static_cast<uint32_t>(jsonText.size()));
  AppendU32(glb, 0x4E4F534Au);
  AppendBytes(glb, jsonText.data(), jsonText.size());
  AppendU32(glb, static_cast<uint32_t>(bin.size()));
  AppendU32(glb, 0x004E4942u);
  AppendBytes(glb, bin.data(), bin.size());

  std::filesystem::path outPath(glbPath);
  std::error_code ec;
  std::filesystem::create_directories(outPath.parent_path(), ec);

  std::ofstream out(glbPath, std::ios::binary);
  if (!out.is_open()) {
    if (errorMessage)
      *errorMessage = "could not create output GLB file";
    return false;
  }
  out.write(reinterpret_cast<const char *>(glb.data()), static_cast<std::streamsize>(glb.size()));
  if (!out.good()) {
    if (errorMessage)
      *errorMessage = "failed while writing GLB file";
    return false;
  }
  return true;
}
