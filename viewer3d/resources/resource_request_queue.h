#pragma once

#include "resource_reference_cache_key.h"
#include <string>
#include <unordered_set>
#include <vector>

namespace viewer3d::resources {
struct ResourceRequest {
  std::string reference, mode, key;
  bool gdtf = false;
};
// Own references, never scene pointers. Build once per invalidation, then resume
// at the cursor instead of normalizing every SceneObject on every fill-in frame.
class ResourceRequestQueue {
public:
  void Clear() { requests.clear(); keys.clear(); cursor = 0; }
  void Add(const std::string &reference, const std::string &mode = {}, bool gdtf = false) {
    const auto normalized = BuildResourceReferenceCacheKey(reference);
    if (normalized.empty()) return;
    const auto key = (gdtf ? "gdtf:" : "model:") +
        std::to_string(normalized.size()) + ":" + normalized + mode;
    if (keys.insert(key).second) requests.push_back({reference, mode, key, gdtf});
  }
  bool Empty() const { return cursor == requests.size(); }
  const ResourceRequest &Front() const { return requests[cursor]; }
  void Advance() { ++cursor; }
private:
  std::vector<ResourceRequest> requests;
  std::unordered_set<std::string> keys;
  size_t cursor = 0;
};
} // namespace viewer3d::resources
