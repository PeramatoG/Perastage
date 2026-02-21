#include "legendsymbolcapture.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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

std::vector<std::pair<std::string, std::string>>
CollectLegendFixtureTargets(const ConfigManager &cfg) {
  std::vector<std::pair<std::string, std::string>> targets;
  const auto &scene = cfg.GetScene();
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const std::string modelKey = BuildFixtureSymbolKey(fixture, scene.basePath);
    if (modelKey.empty())
      continue;
    targets.emplace_back(modelKey, uuid);
  }

  std::sort(targets.begin(), targets.end(),
            [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });
  targets.erase(std::unique(targets.begin(), targets.end(),
                            [](const auto &lhs, const auto &rhs) {
                              return lhs.first == rhs.first;
                            }),
                targets.end());
  return targets;
}

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureLegendSymbolSnapshotWithTool(Viewer2DOffscreenRenderer *offscreenRenderer,
                                    ConfigManager &cfg) {
  if (!offscreenRenderer)
    return {};

  const auto targets = CollectLegendFixtureTargets(cfg);
  if (targets.empty())
    return {};

  auto snapshot = std::make_shared<SymbolDefinitionSnapshot>();
  uint32_t nextSymbolId = 1;

  for (const auto &[modelKey, fixtureUuid] : targets) {
    tools::SceneModelSymbolCaptureOptions options;
    options.alignToLocalAxes = false;
    const auto capture = tools::CaptureSceneModelOrthographicSymbols(
        *offscreenRenderer, cfg,
        tools::SceneModelSymbolTarget{tools::SceneModelKind::Fixture,
                                      fixtureUuid},
        options);
    if (!capture.ok)
      continue;

    for (const auto &toolSymbol : capture.symbols) {
      SymbolDefinition definition =
          BuildDefinitionFromToolSymbol(modelKey, toolSymbol, nextSymbolId++);
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
  ScopedHiddenLayersClear hiddenLayersGuard(cfg);
  return CaptureLegendSymbolSnapshotWithTool(offscreenRenderer, cfg);
}
