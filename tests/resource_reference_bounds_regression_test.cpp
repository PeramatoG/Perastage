#include "bounds_cache_system.h"
#include "configmanager.h"
#include "resource_reference_cache_key.h"

#include <cassert>
#include <cmath>
#include <mutex>

namespace {

// Returns one axis span from a computed viewer bounding box.
float Span(const Viewer3DBoundingBox &bounds, size_t axis) {
  return bounds.max[axis] - bounds.min[axis];
}

} // namespace

// Keeps all fixture types visible for the focused bounds-cache regression.
bool ConfigManager::IsFixtureTypeVisible(const std::string &) const {
  return true;
}

// Verifies canonical reference equivalence and producer-to-bounds lookup parity.
int main() {
  using viewer3d::resources::BuildResourceReferenceCacheKey;
  const std::string expected = BuildResourceReferenceCacheKey("fixtures/Foo.gdtf");
  assert(expected == BuildResourceReferenceCacheKey("fixtures\\Foo.gdtf"));
  assert(expected == BuildResourceReferenceCacheKey("./fixtures/Foo.gdtf"));
  assert(expected == BuildResourceReferenceCacheKey("  fixtures/Foo.gdtf  "));
  assert(expected == BuildResourceReferenceCacheKey("\"fixtures/Foo.gdtf\""));
  assert(BuildResourceReferenceCacheKey("C:/root/fixtures/Foo.gdtf") ==
         BuildResourceReferenceCacheKey("C:\\root\\fixtures\\Foo.gdtf"));

  ResourceSyncState resources;
  ResourceSyncState::PathResolutionEntry resolved;
  resolved.attempted = true;
  resolved.resolvedPath = "/physical/Foo.gdtf";
  resources.resolvedGdtfSpecs[expected] = resolved;

  Mesh mesh;
  mesh.vertices = {-500.0f, -300.0f, -200.0f, 500.0f, 300.0f, 200.0f};
  mesh.indices = {0, 1};
  resources.loadedGdtf[BuildGdtfResourceKey(resolved.resolvedPath, "Basic")] =
      {GdtfObject{mesh, Matrix{}, false}};

  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.gdtfSpec = "./fixtures/Foo.gdtf";
  fixture.gdtfMode = "Basic";
  std::unordered_map<std::string, Fixture> fixtures = {{fixture.uuid, fixture}};
  std::unordered_map<std::string, Viewer3DBoundingBox> modelBounds;
  std::unordered_map<std::string, Viewer3DBoundingBox> fixtureBounds;
  std::unordered_map<std::string, Viewer3DBoundingBox> trussBounds;
  std::unordered_map<std::string, Viewer3DBoundingBox> objectBounds;
  std::unordered_set<std::string> boundsHiddenLayers;
  size_t cachedVersion = 0;
  bool sceneChanged = true;
  bool assetsChanged = true;
  bool visibilityChanged = true;
  std::mutex sortedListsMutex;
  bool sortedListsDirty = false;
  BoundsCacheSystem::Context context{
      resources,       modelBounds,       fixtureBounds,      trussBounds,
      objectBounds,    boundsHiddenLayers, 1,                  cachedVersion,
      sceneChanged,    assetsChanged,     visibilityChanged,  sortedListsMutex,
      sortedListsDirty};
  BoundsCacheSystem::RebuildIfDirty(context, {}, {}, {}, fixtures);
  const auto bounds = fixtureBounds.at(fixture.uuid);
  assert(std::fabs(Span(bounds, 0) - 1.0f) < 0.0001f);
  assert(std::fabs(Span(bounds, 1) - 0.6f) < 0.0001f);
  assert(std::fabs(Span(bounds, 2) - 0.4f) < 0.0001f);
  return 0;
}
