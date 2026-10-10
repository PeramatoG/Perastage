#pragma once
#include "runtime_mesh_simplification.h"
#include <functional>
#include <map>
#include <tuple>

namespace viewer3d::resources {
// Owned by a single viewer/context. Source invalidation must precede asset
// replacement, so reused Mesh addresses cannot retain stale representations.
class RuntimeMeshCache {
public:
  using Callback = std::function<void(Mesh &)>;
  void ApplyPreferences(model_detail::Preferences preferences,
                        const Callback &release);
  const Mesh &Resolve(const Mesh &source, bool navigating,
                      const Callback &upload);
  void Invalidate(const Mesh &source, const Callback &release);
  void Clear(const Callback &release);
  size_t Size() const { return meshes.size(); }
private:
  using Key = std::tuple<const Mesh *, model_detail::Level, bool>;
  std::map<Key, Mesh> meshes;
  model_detail::Preferences preferences;
};
} // namespace viewer3d::resources
