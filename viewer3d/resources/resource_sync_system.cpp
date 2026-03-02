#include "resource_sync_system.h"

#include "loader3ds.h"
#include "loaderglb.h"
#include "matrixutils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

std::string FindFileRecursive(const std::string &baseDir,
                              const std::string &fileName) {
  if (baseDir.empty())
    return {};
  for (auto &p : fs::recursive_directory_iterator(baseDir)) {
    if (!p.is_regular_file())
      continue;
    if (p.path().filename() == fileName)
      return p.path().string();
  }
  return {};
}

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool EqualsIgnoreAsciiCase(const std::string &lhs, const std::string &rhs) {
  return ToLowerAscii(lhs) == ToLowerAscii(rhs);
}

std::string ResolvePathWithCaseFallback(const fs::path &path) {
  if (path.empty())
    return {};

  std::error_code ec;
  if (fs::exists(path, ec))
    return path.string();
  ec.clear();

  fs::path current;
  for (const auto &part : path) {
    if (part.empty())
      continue;

    const std::string segment = part.string();
    if (segment == "/" || segment == "\\") {
      current /= part;
      continue;
    }

    if (current.empty()) {
      current /= part;
      continue;
    }

    fs::path exactPath = current / part;
    if (fs::exists(exactPath, ec)) {
      current = exactPath;
      ec.clear();
      continue;
    }
    ec.clear();

    bool matched = false;
    for (const auto &entry : fs::directory_iterator(current, ec)) {
      if (ec)
        break;
      if (!EqualsIgnoreAsciiCase(entry.path().filename().string(), segment))
        continue;
      current = entry.path();
      matched = true;
      break;
    }
    if (ec || !matched)
      return {};
    ec.clear();
  }

  if (fs::exists(current, ec))
    return current.string();
  return {};
}


std::string TrimPathRef(std::string value) {
  auto isTrim = [](unsigned char c) {
    return std::isspace(c) != 0 || c == '\r' || c == '\n' || c == '\t';
  };

  while (!value.empty() && isTrim(static_cast<unsigned char>(value.front())))
    value.erase(value.begin());
  while (!value.empty() && isTrim(static_cast<unsigned char>(value.back())))
    value.pop_back();

  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);

  return value;
}

std::string NormalizePath(const std::string &p) {
  std::string out = p;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  std::replace(out.begin(), out.end(), '/', sep);
  return out;
}

std::string NormalizeModelKey(const std::string &p) {
  if (p.empty())
    return {};
  fs::path path(p);
  path = path.lexically_normal();
  return NormalizePath(path.string());
}

std::string ResolveCacheKey(const std::string &pathRef) {
  return NormalizeModelKey(TrimPathRef(pathRef));
}

std::string ResolveGdtfPath(const std::string &base, const std::string &spec,
                            bool allowRecursiveFallback = false) {
  const std::string cleanSpec = TrimPathRef(spec);
  if (cleanSpec.empty())
    return {};

  const std::array<std::string, 2> variants = {cleanSpec,
                                               NormalizePath(cleanSpec)};

  std::error_code ec;
  for (const std::string &variant : variants) {
    fs::path absoluteCandidate = fs::path(variant);
    if (absoluteCandidate.is_absolute()) {
      const std::string absoluteResolved =
          ResolvePathWithCaseFallback(absoluteCandidate);
      if (!absoluteResolved.empty())
        return absoluteResolved;
    }
    ec.clear();

    fs::path relativeCandidate = fs::path(base) / absoluteCandidate;
    const std::string relativeResolved =
        ResolvePathWithCaseFallback(relativeCandidate);
    if (!relativeResolved.empty())
      return relativeResolved;
    ec.clear();

    fs::path utf8AbsoluteCandidate = fs::u8path(variant);
    if (utf8AbsoluteCandidate.is_absolute()) {
      const std::string utf8AbsoluteResolved =
          ResolvePathWithCaseFallback(utf8AbsoluteCandidate);
      if (!utf8AbsoluteResolved.empty())
        return utf8AbsoluteResolved;
    }
    ec.clear();

    fs::path utf8RelativeCandidate = fs::path(base) / utf8AbsoluteCandidate;
    const std::string utf8RelativeResolved =
        ResolvePathWithCaseFallback(utf8RelativeCandidate);
    if (!utf8RelativeResolved.empty())
      return utf8RelativeResolved;
    ec.clear();
  }

  if (allowRecursiveFallback)
    return FindFileRecursive(base, fs::path(cleanSpec).filename().string());
  return {};
}

std::string ResolveModelPath(const std::string &base,
                             const std::string &modelRef,
                             bool allowRecursiveFallback = false) {
  return ResolveGdtfPath(base, modelRef, allowRecursiveFallback);
}

size_t HashCombine(size_t seed, size_t value) {
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

size_t HashString(const std::string &value) { return std::hash<std::string>{}(value); }

size_t HashFloat(float value) {
  return std::hash<int>{}(std::lround(value * 1000.0f));
}

size_t HashMatrix(const Matrix &m) {
  size_t hash = 0;
  for (float value : m.u)
    hash = HashCombine(hash, HashFloat(value));
  for (float value : m.v)
    hash = HashCombine(hash, HashFloat(value));
  for (float value : m.w)
    hash = HashCombine(hash, HashFloat(value));
  for (float value : m.o)
    hash = HashCombine(hash, HashFloat(value));
  return hash;
}



FixtureSceneNode BuildInstancedFixtureNode(const GdtfNode3D &templateNode,
                                          const Matrix &fixtureTransform,
                                          int parentIndexOffset) {
  FixtureSceneNode node;
  node.stableName = templateNode.stableName;
  node.parentIndex = templateNode.parentIndex < 0
                         ? parentIndexOffset
                         : templateNode.parentIndex + parentIndexOffset + 1;
  node.localTransform = templateNode.localTransform;
  node.worldTransform = MatrixUtils::Multiply(fixtureTransform, templateNode.worldTransform);
  node.isAxis = templateNode.type == GdtfNodeType::Axis;
  node.isEmitter = templateNode.type == GdtfNodeType::Emitter;
  return node;
}

void EnsureModelLoaded(const std::string &path, ResourceSyncState &state,
                       const ResourceSyncCallbacks &callbacks,
                       bool &assetsChanged) {
  if (path.empty() || state.loadedMeshes.find(path) != state.loadedMeshes.end())
    return;

  Mesh mesh;
  bool loaded = false;
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ext == ".3ds")
    loaded = Load3DS(path, mesh);
  else if (ext == ".glb")
    loaded = LoadGLB(path, mesh);

  if (loaded) {
    if (callbacks.setupMeshBuffers)
      callbacks.setupMeshBuffers(mesh);
    state.loadedMeshes[path] = std::move(mesh);
    assetsChanged = true;
  } else if (callbacks.appendConsoleMessage) {
    callbacks.appendConsoleMessage("Failed to load model: " + path);
  }
}

} // namespace

ResourceSyncResult ResourceSyncSystem::Sync(
    const std::string &basePath,
    const std::vector<const std::pair<const std::string, Truss> *> &visibleTrusses,
    const std::vector<const std::pair<const std::string, SceneObject> *> &visibleObjects,
    const std::vector<const std::pair<const std::string, Fixture> *> &visibleFixtures,
    ResourceSyncState &state, const ResourceSyncCallbacks &callbacks) {
  ResourceSyncResult result;

  if (state.lastSceneBasePath != basePath) {
    for (auto &[path, mesh] : state.loadedMeshes) {
      (void)path;
      if (callbacks.releaseMeshBuffers)
        callbacks.releaseMeshBuffers(mesh);
    }
    state.loadedMeshes.clear();
    state.loadedGdtf.clear();
    state.loadedGdtfGeometryTrees.clear();
    state.fixtureNodeRegistry.clear();
    state.fixtureAnchorRegistry.clear();
    state.failedGdtfReasons.clear();
    state.reportedGdtfFailureCounts.clear();
    state.reportedGdtfFailureReasons.clear();
    state.resolvedGdtfSpecs.clear();
    state.resolvedModelRefs.clear();
    state.lastSceneBasePath = basePath;
    result.assetsChanged = true;
  }

  size_t sceneSignature = HashString(basePath);
  for (const auto *entry : visibleTrusses) {
    const auto &[uuid, t] = *entry;
    sceneSignature = HashCombine(sceneSignature, HashString(uuid));
    sceneSignature = HashCombine(sceneSignature, HashString(t.symbolFile));
    sceneSignature = HashCombine(sceneSignature, HashMatrix(t.transform));
    sceneSignature = HashCombine(sceneSignature, HashFloat(t.lengthMm));
    sceneSignature = HashCombine(sceneSignature, HashFloat(t.widthMm));
    sceneSignature = HashCombine(sceneSignature, HashFloat(t.heightMm));
  }
  for (const auto *entry : visibleObjects) {
    const auto &[uuid, o] = *entry;
    sceneSignature = HashCombine(sceneSignature, HashString(uuid));
    sceneSignature = HashCombine(sceneSignature, HashString(o.modelFile));
    sceneSignature = HashCombine(sceneSignature, HashMatrix(o.transform));
    for (const auto &g : o.geometries) {
      sceneSignature = HashCombine(sceneSignature, HashString(g.modelFile));
      sceneSignature = HashCombine(sceneSignature, HashMatrix(g.localTransform));
    }
  }
  for (const auto *entry : visibleFixtures) {
    const auto &[uuid, f] = *entry;
    sceneSignature = HashCombine(sceneSignature, HashString(uuid));
    sceneSignature = HashCombine(sceneSignature, HashString(f.gdtfSpec));
    sceneSignature = HashCombine(sceneSignature, HashMatrix(f.transform));
  }

  if (!state.hasSceneSignature || sceneSignature != state.lastSceneSignature) {
    state.lastSceneSignature = sceneSignature;
    state.hasSceneSignature = true;
    result.sceneChanged = true;

    // Retry previously failed GDTF loads whenever the visible scene changes.
    // This prevents stale failure caches when a fixture GDTF path is updated
    // (or a previously invalid file gets replaced) without changing basePath.
    state.failedGdtfReasons.clear();
    state.reportedGdtfFailureCounts.clear();
    state.reportedGdtfFailureReasons.clear();
  }
  result.sceneSignature = state.lastSceneSignature;
  result.hasSceneSignature = state.hasSceneSignature;

  auto ensureGdtfResolvedPath = [&](const std::string &spec) {
    const std::string cleanSpec = TrimPathRef(spec);
    if (cleanSpec.empty())
      return;
    const std::string key = ResolveCacheKey(cleanSpec);
    auto [it, inserted] =
        state.resolvedGdtfSpecs.try_emplace(key, ResourceSyncState::PathResolutionEntry{});
    if (!inserted && it->second.attempted && !it->second.resolvedPath.empty() &&
        fs::exists(fs::u8path(it->second.resolvedPath)))
      return;
    it->second.resolvedPath = ResolveGdtfPath(basePath, cleanSpec, true);
    it->second.attempted = true;
  };

  auto ensureModelResolvedPath = [&](const std::string &modelRef) {
    const std::string cleanModelRef = TrimPathRef(modelRef);
    if (cleanModelRef.empty())
      return;
    const std::string key = ResolveCacheKey(cleanModelRef);
    auto [it, inserted] =
        state.resolvedModelRefs.try_emplace(key, ResourceSyncState::PathResolutionEntry{});
    if (!inserted && it->second.attempted && !it->second.resolvedPath.empty() &&
        fs::exists(fs::u8path(it->second.resolvedPath)))
      return;
    it->second.resolvedPath = ResolveModelPath(basePath, cleanModelRef, true);
    it->second.attempted = true;
  };

  for (const auto *entry : visibleTrusses)
    ensureModelResolvedPath(entry->second.symbolFile);

  for (const auto *entry : visibleObjects) {
    const auto &obj = entry->second;
    if (!obj.geometries.empty()) {
      for (const auto &geo : obj.geometries)
        ensureModelResolvedPath(geo.modelFile);
    } else {
      ensureModelResolvedPath(obj.modelFile);
    }
  }

  for (const auto *entry : visibleFixtures)
    ensureGdtfResolvedPath(entry->second.gdtfSpec);

  for (const auto *entry : visibleTrusses) {
    const auto &t = entry->second;
    auto pathIt = state.resolvedModelRefs.find(ResolveCacheKey(t.symbolFile));
    const std::string path =
        (pathIt != state.resolvedModelRefs.end() && pathIt->second.attempted)
            ? pathIt->second.resolvedPath
            : std::string();
    EnsureModelLoaded(path, state, callbacks, result.assetsChanged);
  }

  for (const auto *entry : visibleObjects) {
    const auto &obj = entry->second;
    if (!obj.geometries.empty()) {
      for (const auto &geo : obj.geometries) {
        auto pathIt = state.resolvedModelRefs.find(ResolveCacheKey(geo.modelFile));
        const std::string path =
            (pathIt != state.resolvedModelRefs.end() && pathIt->second.attempted)
                ? pathIt->second.resolvedPath
                : std::string();
        EnsureModelLoaded(path, state, callbacks, result.assetsChanged);
      }
      continue;
    }

    auto pathIt = state.resolvedModelRefs.find(ResolveCacheKey(obj.modelFile));
    const std::string path =
        (pathIt != state.resolvedModelRefs.end() && pathIt->second.attempted)
            ? pathIt->second.resolvedPath
            : std::string();
    EnsureModelLoaded(path, state, callbacks, result.assetsChanged);
  }

  std::unordered_map<std::string, size_t> gdtfErrorCounts;
  std::unordered_map<std::string, std::string> gdtfErrorReasons;
  std::unordered_set<std::string> processedGdtfPaths;
  std::unordered_set<std::string> missingGdtfSpecs;

  for (const auto *entry : visibleFixtures) {
    const auto &f = entry->second;
    if (f.gdtfSpec.empty())
      continue;

    auto gdtfPathIt = state.resolvedGdtfSpecs.find(ResolveCacheKey(f.gdtfSpec));
    const std::string gdtfPath =
        (gdtfPathIt != state.resolvedGdtfSpecs.end() && gdtfPathIt->second.attempted)
            ? gdtfPathIt->second.resolvedPath
            : std::string();

    if (gdtfPath.empty()) {
      ++gdtfErrorCounts[f.gdtfSpec];
      gdtfErrorReasons[f.gdtfSpec] = "GDTF file not found";
      if (!missingGdtfSpecs.insert(f.gdtfSpec).second)
        continue;
      continue;
    }

    auto failedIt = state.failedGdtfReasons.find(gdtfPath);
    if (failedIt != state.failedGdtfReasons.end()) {
      ++gdtfErrorCounts[gdtfPath];
      gdtfErrorReasons[gdtfPath] = failedIt->second;
      continue;
    }

    if (!processedGdtfPaths.insert(gdtfPath).second)
      continue;

    if (state.loadedGdtf.find(gdtfPath) == state.loadedGdtf.end()) {
      std::vector<GdtfObject> objs;
      std::string gdtfError;
      if (LoadGdtf(gdtfPath, objs, &gdtfError)) {
        state.loadedGdtf[gdtfPath] = std::move(objs);

        GdtfGeometryTree geometryTree;
        std::string geometryTreeError;
        if (LoadGdtfGeometryTree(gdtfPath, geometryTree, &geometryTreeError)) {
          state.loadedGdtfGeometryTrees[gdtfPath] = std::move(geometryTree);
        }

        result.assetsChanged = true;
      } else {
        std::string reason = gdtfError.empty() ? "Failed to load GDTF" : gdtfError;
        state.failedGdtfReasons[gdtfPath] = reason;
        ++gdtfErrorCounts[gdtfPath];
        gdtfErrorReasons[gdtfPath] = reason;
        result.assetsChanged = true;
      }
    }
  }

  state.fixtureNodeRegistry.clear();
  state.fixtureAnchorRegistry.clear();
  for (const auto *entry : visibleFixtures) {
    const auto &[uuid, fixture] = *entry;
    if (fixture.gdtfSpec.empty())
      continue;

    auto gdtfPathIt = state.resolvedGdtfSpecs.find(ResolveCacheKey(fixture.gdtfSpec));
    if (gdtfPathIt == state.resolvedGdtfSpecs.end() || !gdtfPathIt->second.attempted ||
        gdtfPathIt->second.resolvedPath.empty())
      continue;

    auto treeIt = state.loadedGdtfGeometryTrees.find(gdtfPathIt->second.resolvedPath);
    if (treeIt == state.loadedGdtfGeometryTrees.end())
      continue;

    std::vector<FixtureSceneNode> instancedNodes;
    instancedNodes.reserve(treeIt->second.nodes.size() + 1);

    FixtureSceneNode rootNode;
    rootNode.stableName = "Fixture_" + uuid;
    rootNode.parentIndex = -1;
    rootNode.localTransform = fixture.transform;
    rootNode.worldTransform = fixture.transform;
    instancedNodes.push_back(rootNode);

    FixtureAnchorRegistryEntry anchors;
    anchors.fixtureRootName = rootNode.stableName;

    for (const GdtfNode3D &templateNode : treeIt->second.nodes) {
      FixtureSceneNode node = BuildInstancedFixtureNode(templateNode, fixture.transform, 0);
      if (node.isAxis)
        anchors.axisNodes.push_back(node);
      if (node.isEmitter)
        anchors.emitterNodes.push_back(node);
      instancedNodes.push_back(std::move(node));
    }

    state.fixtureNodeRegistry[uuid] = std::move(instancedNodes);
    state.fixtureAnchorRegistry[uuid] = std::move(anchors);
  }

  if (callbacks.appendConsoleMessage) {
    for (const auto &[path, count] : gdtfErrorCounts) {
      const std::string &reason = gdtfErrorReasons[path];
      auto prevCountIt = state.reportedGdtfFailureCounts.find(path);
      auto prevReasonIt = state.reportedGdtfFailureReasons.find(path);
      const bool sameCount = prevCountIt != state.reportedGdtfFailureCounts.end() &&
                             prevCountIt->second == count;
      const bool sameReason = prevReasonIt != state.reportedGdtfFailureReasons.end() &&
                              prevReasonIt->second == reason;
      if (sameCount && sameReason)
        continue;

      if (count > 1) {
        callbacks.appendConsoleMessage("Failed to load GDTF " + path + " (" +
                                       std::to_string(count) +
                                       " fixtures): " + reason);
      } else {
        callbacks.appendConsoleMessage("Failed to load GDTF " + path + ": " + reason);
      }
      state.reportedGdtfFailureCounts[path] = count;
      state.reportedGdtfFailureReasons[path] = reason;
    }
  }

  return result;
}
