#include "runtime_mesh_cache.h"

namespace viewer3d::resources {
void RuntimeMeshCache::ApplyPreferences(model_detail::Preferences next,
                                       const Callback &release) {
  if (next == preferences) return;
  pending.clear();
  queued.clear();
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
  (void)upload; // Drawing only resolves; optional work is prepared after display.
  const bool movingProxy = preferences.movingProxy || preferences.level == model_detail::Level::Low;
  const bool proxy = navigating && movingProxy;
  if (movingProxy) Queue(source, true);
  const auto policy = model_detail::ResolvePolicy(source.indices.size() / 3,
                                                 preferences.level, proxy);
  if (policy.targetTriangles >= source.indices.size() / 3) return source;
  const Key key{&source, preferences.level, proxy};
  Queue(source, proxy);
  auto it = meshes.find(key);
  return it == meshes.end() ? source : it->second;
}
void RuntimeMeshCache::Queue(const Mesh &source, bool proxy) {
  const Key key{&source, preferences.level, proxy};
  if (model_detail::ResolvePolicy(source.indices.size() / 3, preferences.level,
                                  proxy).targetTriangles >= source.indices.size() / 3 ||
      meshes.contains(key) || !queued.insert(key).second) return;
  // Prepare interaction representations before optional idle quality work.
  if (proxy) pending.push_front(key);
  else pending.push_back(key);
}
size_t RuntimeMeshCache::PreparePending(const Callback &upload, size_t limit) {
  size_t prepared = 0;
  while (!pending.empty() && prepared < limit) {
    const Key key = pending.front();
    pending.pop_front();
    queued.erase(key);
    const Mesh &source = *std::get<0>(key);
    Mesh mesh = BuildRuntimeRepresentation(source, model_detail::ResolvePolicy(
        source.indices.size() / 3, std::get<1>(key), std::get<2>(key)));
    if (upload) upload(mesh);
    meshes.emplace(key, std::move(mesh));
    ++prepared;
  }
  return prepared;
}
void RuntimeMeshCache::Invalidate(const Mesh &source, const Callback &release) {
  std::erase_if(pending, [&](const Key &key) {
    if (std::get<0>(key) != &source) return false;
    queued.erase(key);
    return true;
  });
  for (auto it = meshes.begin(); it != meshes.end();) {
    if (std::get<0>(it->first) == &source) {
      if (release) release(it->second);
      it = meshes.erase(it);
    } else ++it;
  }
}
void RuntimeMeshCache::Clear(const Callback &release) {
  pending.clear();
  queued.clear();
  if (release) for (auto &[key, mesh] : meshes) release(mesh);
  meshes.clear();
}
} // namespace viewer3d::resources
