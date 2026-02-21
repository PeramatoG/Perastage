#include "legendsymbolcapture.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include "configmanager.h"
#include "legendutils.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "viewer2doffscreenrenderer.h"

namespace {
class ScopedHiddenLayersClear {
public:
  explicit ScopedHiddenLayersClear(ConfigManager &cfg)
      : cfg_(cfg), previous_(cfg.GetHiddenLayers()) {
    if (!previous_.empty())
      cfg_.SetHiddenLayers({});
  }

  ~ScopedHiddenLayersClear() {
    if (!previous_.empty())
      cfg_.SetHiddenLayers(previous_);
  }

private:
  ConfigManager &cfg_;
  std::unordered_set<std::string> previous_;
};

struct LegendCaptureTarget {
  std::string modelKey;
  std::string fixtureUuid;

  bool operator==(const LegendCaptureTarget &other) const {
    return modelKey == other.modelKey && fixtureUuid == other.fixtureUuid;
  }
};

struct LegendSymbolSnapshotCache {
  size_t fingerprint = 0;
  std::shared_ptr<const SymbolDefinitionSnapshot> snapshot;
};

size_t HashCombine(size_t seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

size_t HashTargets(const std::vector<LegendCaptureTarget> &targets) {
  size_t seed = 0;
  for (const auto &target : targets) {
    seed = HashCombine(seed, std::hash<std::string>{}(target.modelKey));
    seed = HashCombine(seed, std::hash<std::string>{}(target.fixtureUuid));
  }
  return seed;
}

SymbolViewKind ToSymbolViewKind(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Front:
    return SymbolViewKind::Front;
  case symbols::SymbolView::Top:
    return SymbolViewKind::Top;
  case symbols::SymbolView::Bottom:
    return SymbolViewKind::Bottom;
  case symbols::SymbolView::Left:
  default:
    return SymbolViewKind::Left;
  }
}

CanvasColor SolidBlack() {
  return CanvasColor{0.0f, 0.0f, 0.0f, 1.0f};
}

std::vector<float> FlattenPolyline(const symbols::Polyline2D &polyline) {
  std::vector<float> points;
  points.reserve(polyline.size() * 2);
  for (const auto &point : polyline) {
    points.push_back(point.x);
    points.push_back(point.y);
  }
  return points;
}

SymbolDefinition BuildDefinitionFromToolSymbol(const std::string &modelKey,
                                               const symbols::Symbol2D &symbol,
                                               uint32_t symbolId) {
  SymbolDefinition definition;
  definition.symbolId = symbolId;
  definition.key.modelKey = modelKey;
  definition.key.viewKind = ToSymbolViewKind(symbol.view);
  definition.key.styleVersion = 1;
  definition.bounds.min.x = symbol.bounds.min.x;
  definition.bounds.min.y = symbol.bounds.min.y;
  definition.bounds.max.x = symbol.bounds.max.x;
  definition.bounds.max.y = symbol.bounds.max.y;

  const CanvasColor black = SolidBlack();
  const CanvasStroke stroke{black, symbol.strokeWidthPx};
  const CanvasFill fill{black};

  auto &buffer = definition.localCommands;
  buffer.currentSourceKey = "legend_tool_capture";

  for (const auto &polygon : symbol.fill) {
    PolygonCommand cmd;
    cmd.points = FlattenPolyline(polygon.outer);
    if (cmd.points.size() >= 6) {
      cmd.stroke = stroke;
      cmd.fill = fill;
      cmd.hasFill = true;
      buffer.commands.push_back(std::move(cmd));
      buffer.metadata.push_back(CommandMetadata{true, true});
      buffer.sources.push_back(buffer.currentSourceKey);
    }
  }

  for (const auto &polyline : symbol.strokes) {
    PolylineCommand cmd;
    cmd.points = FlattenPolyline(polyline);
    if (cmd.points.size() >= 4) {
      cmd.stroke = stroke;
      buffer.commands.push_back(std::move(cmd));
      buffer.metadata.push_back(CommandMetadata{true, false});
      buffer.sources.push_back(buffer.currentSourceKey);
    }
  }

  return definition;
}

std::vector<LegendCaptureTarget> CollectLegendFixtureTargets(
    const ConfigManager &cfg) {
  std::vector<LegendCaptureTarget> targets;
  const auto &scene = cfg.GetScene();
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const std::string modelKey = BuildFixtureSymbolKey(fixture, scene.basePath);
    if (modelKey.empty())
      continue;
    targets.push_back(LegendCaptureTarget{modelKey, uuid});
  }

  std::sort(targets.begin(), targets.end(),
            [](const LegendCaptureTarget &lhs, const LegendCaptureTarget &rhs) {
              if (lhs.modelKey == rhs.modelKey)
                return lhs.fixtureUuid < rhs.fixtureUuid;
              return lhs.modelKey < rhs.modelKey;
            });

  targets.erase(std::unique(targets.begin(), targets.end(),
                            [](const LegendCaptureTarget &lhs,
                               const LegendCaptureTarget &rhs) {
                              return lhs.modelKey == rhs.modelKey;
                            }),
                targets.end());
  return targets;
}

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshotWithTool(Viewer2DOffscreenRenderer *offscreenRenderer,
                                    ConfigManager &cfg,
                                    const std::vector<LegendCaptureTarget>
                                        &targets) {
  if (!offscreenRenderer || targets.empty())
    return {};

  auto snapshot = std::make_shared<SymbolDefinitionSnapshot>();
  uint32_t nextSymbolId = 1;

  for (const auto &target : targets) {
    tools::SceneModelSymbolCaptureOptions options;
    options.alignToLocalAxes = false;
    const auto capture = tools::CaptureSceneModelOrthographicSymbols(
        *offscreenRenderer, cfg,
        tools::SceneModelSymbolTarget{tools::SceneModelKind::Fixture,
                                      target.fixtureUuid},
        options);
    if (!capture.ok)
      continue;

    for (const auto &toolSymbol : capture.symbols) {
      SymbolDefinition definition = BuildDefinitionFromToolSymbol(
          target.modelKey, toolSymbol, nextSymbolId++);
      snapshot->emplace(definition.symbolId, std::move(definition));
    }
  }

  if (snapshot->empty())
    return {};

  return snapshot;
}

} // namespace

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshot(Viewer2DOffscreenRenderer *offscreenRenderer,
                            ConfigManager &cfg,
                            bool requireTopAndFrontViews) {
  (void)requireTopAndFrontViews;

  static std::mutex cacheMutex;
  static LegendSymbolSnapshotCache cache;

  const auto targets = CollectLegendFixtureTargets(cfg);
  if (targets.empty())
    return {};

  const size_t fingerprint = HashTargets(targets);
  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (cache.snapshot && cache.fingerprint == fingerprint)
      return cache.snapshot;
  }

  ScopedHiddenLayersClear hiddenLayersGuard(cfg);
  std::shared_ptr<const SymbolDefinitionSnapshot> snapshot =
      CaptureLegendSymbolSnapshotWithTool(offscreenRenderer, cfg, targets);

  if (!snapshot)
    return {};

  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache.fingerprint = fingerprint;
    cache.snapshot = snapshot;
  }

  return snapshot;
}
