#include "resources/resource_sync_system.h"
#include "performance_metrics.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
extern size_t modelLoads, fixtureLoads;
namespace fs = std::filesystem;
namespace AppPaths {
fs::path GetUserDataDir() { return fs::temp_directory_path() / "unused-progressive-cache"; }
}
int main() {
  const auto root = fs::temp_directory_path() / ("progressive-resource-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  std::unordered_map<std::string, SceneObject> scene;
  std::vector<const std::pair<const std::string, SceneObject> *> objects;
  // 10k objects, 4,490 physical paths, many repeated references; no real MVR.
  for (size_t i = 0; i < 4490; ++i)
    std::ofstream(root / (std::to_string(i) + ".3ds")) << "deterministic mock resource";
  for (size_t i = 0; i < 10000; ++i) {
    auto &object = scene[std::to_string(i)];
    object.uuid = std::to_string(i);
    object.modelFile = std::to_string(i % 4490) + ".3ds";
    object.layer = i < 9000 ? "visible" : "hidden";
  }
  std::vector<const std::pair<const std::string, SceneObject> *> visible;
  for (const auto &entry : scene) {
    objects.push_back(&entry);
    if (entry.second.layer == "visible") visible.push_back(&entry);
  }
  ResourceSyncState state;
  ResourceSyncCallbacks callbacks;
  callbacks.meshProcessingOptions = viewer3d::resources::MeshProcessingOptions{
      .enableMeshOptimization = true, .enableDiskCache = false};
  callbacks.requestGeneration = 1;
  callbacks.maxResources = 8;
  size_t uploads = 0;
  callbacks.setupMeshBuffers = [&](Mesh &) { ++uploads; };
  callbacks.releaseMeshBuffers = [](Mesh &) {};
  auto sync = [&]() {
    const size_t before = modelLoads;
    const auto result = ResourceSyncSystem::Sync(root.string(), {}, objects, {}, {}, visible, {}, state, callbacks);
    assert(result.resourcesProcessed <= 8);
    assert(modelLoads - before <= 8);
    return result;
  };
  auto result = sync();
  assert(result.workPending && modelLoads == 8 && uploads == 8);
  size_t passes = 1;
  while (result.workPending) {
    result = sync(); ++passes;
    assert(passes < 1000);
  }
  assert(modelLoads == 4490 && uploads == 4490 && passes == 562);
  assert(performance::samples[static_cast<size_t>(performance::Work::SceneSignature)].count == 1);
  const auto completed = modelLoads;
  assert(!sync().workPending && modelLoads == completed);
  // Hidden-only resources do not enter the batch until visibility changes.
  SceneObject hidden;
  hidden.uuid = "hidden-only"; hidden.modelFile = "hidden.3ds";
  std::ofstream(root / hidden.modelFile) << "hidden resource";
  auto &entry = *scene.emplace(hidden.uuid, hidden).first;
  objects.push_back(&entry);
  callbacks.requestGeneration = 2;
  while (sync().workPending) {}
  assert(modelLoads == completed);
  visible.push_back(&entry);
  callbacks.requestGeneration = 3;
  while (sync().workPending) {}
  assert(modelLoads == completed + 1);
  // Start unfinished work, then replace the scene using the SAME base path.
  callbacks.requestGeneration = 4;
  sync();
  SceneObject replacement;
  replacement.modelFile = "new.3ds";
  std::ofstream(root / replacement.modelFile) << "new resource";
  std::pair<const std::string, SceneObject> newEntry("replacement", replacement);
  objects = {&newEntry}; visible = objects;
  scene.clear(); // No queued work may retain pointers into the old containers.
  callbacks.requestGeneration = 5;
  assert(!sync().workPending);
  assert(state.loadedMeshes.contains((root / "new.3ds").string()));
  const auto revisionLoads = modelLoads;
  fs::last_write_time(root / "new.3ds", fs::last_write_time(root / "new.3ds") + std::chrono::seconds(1));
  callbacks.requestGeneration = 6;
  sync();
  assert(modelLoads == revisionLoads + 1);
  // Explicit invalidation clears resolved render pointers before releasing data.
  state.resolvedObjectParts[&newEntry.second] = {};
  ResourceSyncSystem::InvalidatePhysicalAsset((root / "new.3ds").string(), {}, state, callbacks);
  assert(state.resolvedObjectParts.empty());
  callbacks.requestGeneration = 7;
  const auto invalidatedLoads = modelLoads;
  sync();
  assert(modelLoads == invalidatedLoads + 1);
  // Fixture work is prioritized, mode aware and deduplicated before metadata reads.
  std::ofstream(root / "fixture.gdtf") << "fixture resource";
  std::unordered_map<std::string, Fixture> fixtureScene;
  std::vector<const std::pair<const std::string, Fixture> *> fixtures;
  for (size_t i = 0; i < 1000; ++i) {
    auto &fixture = fixtureScene[std::to_string(i)];
    fixture.uuid = std::to_string(i); fixture.gdtfSpec = "fixture.gdtf";
    fixture.gdtfMode = i % 2 ? "Mode A" : "Mode B";
  }
  for (const auto &item : fixtureScene) fixtures.push_back(&item);
  callbacks.requestGeneration = 8;
  callbacks.maxResources = 2;
  const auto fixtureResult = ResourceSyncSystem::Sync(root.string(), {}, objects,
      fixtures, {}, visible, fixtures, state, callbacks);
  assert(fixtureResult.workPending && fixtureLoads == 2);
  assert(state.loadedGdtf.size() == 2);
  std::cout << "objects=10000 unique_resources=4490 batch_limit=8 passes=" << passes << '\n';
  fs::remove_all(root);
}
