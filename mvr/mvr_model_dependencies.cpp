/* This file is part of Perastage. */
#include "mvr_model_dependencies.h"
#include "json.hpp"
#include <algorithm>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <functional>
#include <set>

namespace mvr {
namespace {
uint32_t Read32(const unsigned char *bytes) {
  return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
         (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

ModelDependencies Inspect3ds(std::ifstream &file, std::streamoff size) {
  ModelDependencies result;
  std::set<std::string> references;
  std::function<bool(std::streamoff, std::streamoff, int)> visit;
  visit = [&](std::streamoff start, std::streamoff end, int depth) {
    if (depth > 32) return false;
    for (auto offset = start; offset < end;) {
      unsigned char header[6];
      file.seekg(offset);
      if (end - offset < 6 || !file.read(reinterpret_cast<char *>(header), 6)) return false;
      const uint16_t type = uint16_t(header[0]) | (uint16_t(header[1]) << 8);
      const auto length = Read32(header + 2);
      if (length < 6 || length > end - offset) return false;
      if (offset == 0 && type != 0x4D4D) return false;
      if (type == 0xA300) {
        std::string reference;
        bool terminated = false;
        for (auto i = offset + 6; i < offset + length; ++i) {
          char ch;
          if (!file.get(ch)) return false;
          if (ch == '\0') { terminated = true; break; }
          reference.push_back(ch);
        }
        if (!terminated) return false;
        if (!reference.empty()) references.insert(reference);
      } else if (type == 0x4D4D || type == 0x3D3D || type == 0xAFFF ||
                 (type >= 0xA200 && type <= 0xA3FF)) {
        // Include all material map kinds, not just the diffuse texture map.
        if (!visit(offset + 6, offset + length, depth + 1)) return false;
      }
      offset += length;
    }
    return true;
  };
  if (size < 6 || !visit(0, size, 0)) result.error = "Cannot inspect the 3DS dependency structure.";
  result.references.assign(references.begin(), references.end());
  return result;
}

ModelDependencies InspectGltf(std::ifstream &file, std::streamoff size, bool binary) {
  ModelDependencies result;
  std::string text;
  constexpr uint32_t maxJsonBytes = 64 * 1024 * 1024;
  if (binary) {
    unsigned char header[20];
    if (size < 20 || !file.read(reinterpret_cast<char *>(header), 20) ||
        Read32(header) != 0x46546C67 || Read32(header + 4) != 2 ||
        Read32(header + 8) != size || Read32(header + 16) != 0x4E4F534A ||
        Read32(header + 12) > size - 20 || Read32(header + 12) > maxJsonBytes) {
      result.error = "Cannot inspect the GLB JSON chunk.";
      return result;
    }
    text.resize(Read32(header + 12));
  } else {
    if (size < 0 || size > maxJsonBytes) {
      result.error = "Cannot inspect the glTF JSON size.";
      return result;
    }
    text.resize(static_cast<size_t>(size));
  }
  if (!file.read(text.data(), static_cast<std::streamsize>(text.size()))) {
    result.error = "Cannot read model JSON.";
    return result;
  }
  const auto json = nlohmann::json::parse(text, nullptr, false);
  if (json.is_discarded()) { result.error = "Cannot parse model JSON."; return result; }
  if (!json.is_object() || !json.contains("asset") || !json["asset"].is_object() ||
      !json["asset"].contains("version") || json["asset"]["version"] != "2.0" ||
      (json.contains("extensionsRequired") &&
       (!json["extensionsRequired"].is_array() || !json["extensionsRequired"].empty()))) {
    // Keep discovered dependencies available to legacy callers with separate policies.
    result.error = "MVR 1.6 requires glTF 2.0 without required extensions.";
  }
  std::set<std::string> references;
  const auto visit = [&](auto &&self, const nlohmann::json &node) -> void {
    if (node.is_object()) {
      for (const auto &[key, value] : node.items()) {
        if (key == "uri" && value.is_string()) {
          const auto uri = value.get<std::string>();
          if (!uri.empty() && uri.rfind("data:", 0) != 0) references.insert(uri);
        } else self(self, value);
      }
    } else if (node.is_array()) for (const auto &value : node) self(self, value);
  };
  visit(visit, json);
  result.references.assign(references.begin(), references.end());
  return result;
}
} // namespace

ModelDependencies InspectModelDependencies(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return {{}, "Model resource is unavailable."};
  const auto size = file.tellg();
  file.seekg(0);
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (extension == ".3ds" || extension.empty()) return Inspect3ds(file, size);
  if (extension == ".glb" || extension == ".gltf") return InspectGltf(file, size, extension == ".glb");
  return {{}, "Model format is outside the inspected dependency subset."};
}
} // namespace mvr
