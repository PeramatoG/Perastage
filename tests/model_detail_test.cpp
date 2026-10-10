#include "model_detail_policy.h"
#include "resources/runtime_mesh_cache.h"
#include <cassert>

using model_detail::Level;
using viewer3d::resources::RuntimeMeshCache;

Mesh Grid(size_t side) {
  Mesh mesh;
  for (size_t y = 0; y <= side; ++y)
    for (size_t x = 0; x <= side; ++x)
      mesh.vertices.insert(mesh.vertices.end(),
                           {float(x), float(y), 0.0f});
  for (size_t y = 0; y < side; ++y) {
    for (size_t x = 0; x < side; ++x) {
      const uint32_t a = uint32_t(y * (side + 1) + x);
      const uint32_t b = a + uint32_t(side + 1);
      mesh.indices.insert(mesh.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
    }
  }
  return mesh;
}

int main() {
  assert(model_detail::ParseLevel(std::nullopt) == Level::Standard);
  for (const auto value : {"", "garbage", "LOW", "medium"})
    assert(model_detail::ParseLevel(std::string(value)) == Level::Standard);
  for (Level level : {Level::Low, Level::Standard, Level::High})
    assert(model_detail::ParseLevel(std::string(model_detail::ConfigValue(level))) == level);
  for (size_t count : {0u, 300u, 500u, 2000u, 2001u, 10000u, 10001u,
                       50000u, 50001u, 200000u, 200001u, 1000000u}) {
    const auto low = model_detail::ResolvePolicy(count, Level::Low, false);
    const auto standard = model_detail::ResolvePolicy(count, Level::Standard, false);
    assert(model_detail::ResolvePolicy(count, Level::High, false).targetTriangles == count);
    assert(low.targetTriangles <= standard.targetTriangles);
    assert(standard.targetTriangles <= count);
    assert(!low.allowCoarseFallback && !standard.allowCoarseFallback);
    assert(model_detail::ResolvePolicy(count, Level::High, true).targetTriangles ==
           model_detail::ResolvePolicy(count, Level::Low, true).targetTriangles);
  }
  assert(model_detail::ResolvePolicy(2000, Level::Standard, false).targetTriangles == 2000);
  assert(model_detail::ResolvePolicy(10000, Level::Standard, false).targetTriangles == 6500);
  assert(model_detail::ResolvePolicy(50000, Level::Standard, false).targetTriangles == 25000);
  assert(model_detail::ResolvePolicy(200000, Level::Standard, false).targetTriangles == 80000);
  assert(model_detail::ResolvePolicy(1000000, Level::Standard, false).targetTriangles == 100000);
  assert(model_detail::ResolvePolicy(1000000, Level::Low, false).targetTriangles == 25000);

  assert(model_detail::ProxyAcceptableIndexCount(1000, 400) == 800);
  assert(model_detail::ProxyNeedsCoarseFallback(950, 1000, 1000));
  assert(!model_detail::ProxyNeedsCoarseFallback(700, 1000, 800));
  Mesh source = Grid(80);
  source.vao = 7; source.textureId = 9; source.buffersReady = true;
  source.flatVertices = {1, 2, 3}; source.flippedIndicesCache = {0, 1, 2};
  const Mesh original = source;
  RuntimeMeshCache cache;
  size_t uploads = 0, releases = 0;
  auto upload = [&](Mesh &mesh) {
    ++uploads;
    assert(mesh.vao == 0 && mesh.textureId == 0 && !mesh.buffersReady);
    assert(mesh.flatVertices.empty() && mesh.flippedIndicesCache.empty());
  };
  auto release = [&](Mesh &) { ++releases; };
  cache.ApplyPreferences({Level::High, false}, release);
  assert(&cache.Resolve(source, false, upload) == &source);
  assert(&cache.Resolve(source, true, upload) == &source);
  assert(uploads == 0 && cache.Size() == 0);

  cache.ApplyPreferences({Level::Standard, false}, release);
  const Mesh &standard = cache.Resolve(source, false, upload);
  const size_t standardIndices = standard.indices.size();
  assert(standardIndices < source.indices.size());
  assert(&cache.Resolve(source, true, upload) == &standard);
  assert(&cache.Resolve(source, false, upload) == &standard);
  assert(uploads == 1);
  cache.ApplyPreferences({Level::Standard, true}, release);
  assert(&cache.Resolve(source, false, upload) == &standard);
  const Mesh &proxy = cache.Resolve(source, true, upload);
  assert(&proxy != &standard && proxy.indices.size() < standardIndices);
  assert(uploads == 2 && cache.Size() == 2);
  cache.ApplyPreferences({Level::Standard, false}, release);
  assert(releases == 1 && cache.Size() == 1);
  assert(&cache.Resolve(source, true, upload) == &standard);

  cache.ApplyPreferences({Level::Low, false}, release);
  assert(cache.Size() == 0 && releases == 2);
  const Mesh &low = cache.Resolve(source, false, upload);
  assert(low.indices.size() < standardIndices);
  assert(&cache.Resolve(source, true, upload) == &low);
  assert(source.vertices == original.vertices && source.indices == original.indices);
  assert(source.flatVertices == original.flatVertices && source.vao == 7);
  assert(source.textureId == 9 && source.buffersReady);

  // Replacement at the same address invalidates every detail/proxy entry.
  cache.ApplyPreferences({Level::Low, true}, release);
  cache.Resolve(source, true, upload);
  assert(cache.Size() == 2);
  cache.Invalidate(source, release);
  assert(cache.Size() == 0 && releases == 4);
  source = Grid(32);
  assert(cache.Resolve(source, false, upload).indices.size() < source.indices.size());
  const size_t beforeClear = releases;
  cache.Clear(release);
  assert(cache.Size() == 0 && releases == beforeClear + 1);
  Mesh small = Grid(10);
  assert(&cache.Resolve(small, true, upload) == &small);
  cache.ApplyPreferences({Level::High, true}, release);
  assert(&cache.Resolve(source, false, upload) == &source);
  assert(&cache.Resolve(source, true, upload) != &source);
  RuntimeMeshCache independent;
  independent.ApplyPreferences({Level::Low, false}, release);
  const Mesh &independentMesh = independent.Resolve(source, false, upload);
  const Mesh &otherViewerMesh = cache.Resolve(source, true, upload);
  assert(&independentMesh != &otherViewerMesh);
  Mesh otherSource = Grid(40);
  const Mesh &otherRepresentation = independent.Resolve(otherSource, false, upload);
  independent.Invalidate(source, release);
  assert(independent.Size() == 1);
  assert(&independent.Resolve(otherSource, false, upload) == &otherRepresentation);
  independent.Clear(release);
  return 0;
}
