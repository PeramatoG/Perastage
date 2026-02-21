#include "generated_layout_symbols.h"

#include <algorithm>
#include <unordered_map>

#include "configmanager.h"
#include "legendutils.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "viewer2doffscreenrenderer.h"

namespace {

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

CommandBuffer BuildCommandBufferFromGeneratedSymbol(const symbols::Symbol2D &symbol) {
  CommandBuffer buffer;
  CanvasStroke stroke{};
  stroke.color.r = 0.0f;
  stroke.color.g = 0.0f;
  stroke.color.b = 0.0f;
  stroke.color.a = 1.0f;
  stroke.width = std::max(1.0f, symbol.strokeWidthPx) * 0.1f;

  CanvasFill fill{};
  fill.color.r = 0.88f;
  fill.color.g = 0.88f;
  fill.color.b = 0.88f;
  fill.color.a = 1.0f;

  for (const auto &polygon : symbol.fill) {
    if (polygon.outer.size() < 3)
      continue;
    PolygonCommand cmd;
    cmd.stroke = stroke;
    cmd.hasFill = true;
    cmd.fill = fill;
    cmd.points.reserve(polygon.outer.size() * 2);
    for (const auto &point : polygon.outer) {
      cmd.points.push_back(point.x);
      cmd.points.push_back(point.y);
    }
    buffer.commands.emplace_back(std::move(cmd));
    buffer.metadata.emplace_back();
    buffer.sources.emplace_back("generated_symbol");
  }

  for (const auto &polyline : symbol.strokes) {
    if (polyline.size() < 2)
      continue;
    PolylineCommand cmd;
    cmd.stroke = stroke;
    cmd.points.reserve(polyline.size() * 2);
    for (const auto &point : polyline) {
      cmd.points.push_back(point.x);
      cmd.points.push_back(point.y);
    }
    buffer.commands.emplace_back(std::move(cmd));
    buffer.metadata.emplace_back();
    buffer.sources.emplace_back("generated_symbol");
  }

  return buffer;
}

} // namespace

std::shared_ptr<const SymbolDefinitionSnapshot>
CaptureGeneratedLayoutSymbolSnapshot(Viewer2DOffscreenRenderer &renderer,
                                     ConfigManager &cfg,
                                     const std::vector<std::string> &modelKeys) {
  auto snapshot = std::make_shared<SymbolDefinitionSnapshot>();
  if (modelKeys.empty())
    return snapshot;

  std::unordered_map<std::string, std::string> keyToFixtureUuid;
  const auto &scene = cfg.GetScene();
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const std::string key = BuildFixtureSymbolKey(fixture, scene.basePath);
    if (key.empty())
      continue;
    if (keyToFixtureUuid.find(key) == keyToFixtureUuid.end())
      keyToFixtureUuid.emplace(key, uuid);
  }

  uint32_t symbolId = 1;
  for (const auto &modelKey : modelKeys) {
    auto fixtureIt = keyToFixtureUuid.find(modelKey);
    if (fixtureIt == keyToFixtureUuid.end())
      continue;

    auto capture = tools::CaptureSceneModelOrthographicSymbols(
        renderer, cfg,
        tools::SceneModelSymbolTarget{tools::SceneModelKind::Fixture,
                                      fixtureIt->second});
    if (!capture.ok)
      continue;

    for (const auto &generated : capture.symbols) {
      SymbolDefinition definition;
      definition.symbolId = symbolId++;
      definition.key.modelKey = modelKey;
      definition.key.viewKind = ToSymbolViewKind(generated.view);
      definition.key.styleVersion = 1;
      definition.bounds.min.x = generated.bounds.min.x;
      definition.bounds.min.y = generated.bounds.min.y;
      definition.bounds.max.x = generated.bounds.max.x;
      definition.bounds.max.y = generated.bounds.max.y;
      definition.localCommands = BuildCommandBufferFromGeneratedSymbol(generated);
      snapshot->emplace(definition.symbolId, std::move(definition));
    }
  }

  return snapshot;
}
