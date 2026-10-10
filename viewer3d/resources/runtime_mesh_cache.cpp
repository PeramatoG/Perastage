#include "runtime_mesh_cache.h"

namespace viewer3d::resources {
void RuntimeMeshCache::ApplyPreferences(model_detail::Preferences next,
                                       const Callback &release) {
  if (next == preferences) return;
  if (next.level != preferences.level) {
    Clear(release);
  } else {
    for (auto it = meshes.begin(); it != meshes.end();) {
      if (std::get<2>(it->first)) {
        if (release) release(it->second);
        it = meshes.erase(it);
      } else ++it;
    }
  }
  preferences = next;
}
const Mesh &RuntimeMeshCache::Resolve(const Mesh &source, bool navigating,
                                     const Callback &upload) {
  const bool proxy = navigating && preferences.movingProxy;
  const auto policy = model_detail::ResolvePolicy(source.indices.size() / 3,
                                                 preferences.level, proxy);
  if (policy.targetTriangles >= source.indices.size() / 3) return source;
  const Key key{&source, preferences.level, proxy};
  auto it = meshes.find(key);
  if (it == meshes.end()) {
    Mesh mesh = BuildRuntimeRepresentation(source, policy);
    if (upload) upload(mesh);
    it = meshes.emplace(key, std::move(mesh)).first;
  }
  return it->second;
}
void RuntimeMeshCache::Invalidate(const Mesh &source, const Callback &release) {
  for (auto it = meshes.begin(); it != meshes.end();) {
    if (std::get<0>(it->first) == &source) {
      if (release) release(it->second);
      it = meshes.erase(it);
    } else ++it;
  }
}
void RuntimeMeshCache::Clear(const Callback &release) {
  if (release) for (auto &[key, mesh] : meshes) release(mesh);
  meshes.clear();
}
} // namespace viewer3d::resources
