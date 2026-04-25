#include "selection_overlay_pass.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "configmanager.h"
#include "opaque_fixture_pass.h"
#include "opaque_object_pass.h"
#include "opaque_truss_pass.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"

namespace {
std::array<float, 3> MakeDeterministicColor(std::string_view key) {
  const uint64_t hash = std::hash<std::string_view>{}(key);
  const uint8_t r = static_cast<uint8_t>(64u + (hash & 0x7Fu));
  const uint8_t g = static_cast<uint8_t>(64u + ((hash >> 8) & 0x7Fu));
  const uint8_t b = static_cast<uint8_t>(64u + ((hash >> 16) & 0x7Fu));
  return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
          static_cast<float>(b) / 255.0f};
}

bool HexToRGB(const std::string &hex, float &r, float &g, float &b) {
  auto hexNibble = [](char ch) -> int {
    if (ch >= '0' && ch <= '9')
      return ch - '0';
    if (ch >= 'a' && ch <= 'f')
      return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F')
      return 10 + (ch - 'A');
    return -1;
  };
  auto hexPairToInt = [&](char hi, char lo) -> int {
    const int hiNibble = hexNibble(hi);
    const int loNibble = hexNibble(lo);
    if (hiNibble < 0 || loNibble < 0)
      return -1;
    return (hiNibble << 4) | loNibble;
  };
  if (hex.size() != 7 || hex[0] != '#')
    return false;
  const int ri = hexPairToInt(hex[1], hex[2]);
  const int gi = hexPairToInt(hex[3], hex[4]);
  const int bi = hexPairToInt(hex[5], hex[6]);
  if (ri < 0 || gi < 0 || bi < 0)
    return false;
  r = static_cast<float>(ri) / 255.0f;
  g = static_cast<float>(gi) / 255.0f;
  b = static_cast<float>(bi) / 255.0f;
  return true;
}

bool ContainsUuid(const std::vector<std::string> &uuids, const std::string &uuid) {
  return std::find(uuids.begin(), uuids.end(), uuid) != uuids.end();
}

void AppendUuidIfRenderable(const std::string &uuid,
                            const std::unordered_map<std::string, Fixture> &fixtures,
                            const std::unordered_map<std::string, Truss> &trusses,
                            const std::unordered_map<std::string, SceneObject> &objects,
                            Viewer3DVisibleSet &overlaySet) {
  if (uuid.empty())
    return;

  if (fixtures.find(uuid) != fixtures.end() && !ContainsUuid(overlaySet.fixtureUuids, uuid)) {
    overlaySet.fixtureUuids.push_back(uuid);
    return;
  }
  if (trusses.find(uuid) != trusses.end() && !ContainsUuid(overlaySet.trussUuids, uuid)) {
    overlaySet.trussUuids.push_back(uuid);
    return;
  }
  if (objects.find(uuid) != objects.end() && !ContainsUuid(overlaySet.objectUuids, uuid)) {
    overlaySet.objectUuids.push_back(uuid);
    return;
  }
}
} // namespace

void SelectionOverlayPass::Render(Viewer3DController &controller,
                                  const RenderFrameContext &context,
                                  const Viewer3DVisibleSet &visibleSet) {
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  const auto &objects = SceneDataManager::Instance().GetSceneObjects();

  Viewer3DVisibleSet overlaySet;
  const auto appendFromVisibility = [&](const std::vector<std::string> &sourceUuids,
                                        std::vector<std::string> &targetUuids) {
    for (const auto &uuid : sourceUuids) {
      if (uuid == controller.m_highlightUuid ||
          controller.m_selectedUuids.find(uuid) != controller.m_selectedUuids.end()) {
        targetUuids.push_back(uuid);
      }
    }
  };

  appendFromVisibility(visibleSet.fixtureUuids, overlaySet.fixtureUuids);
  appendFromVisibility(visibleSet.trussUuids, overlaySet.trussUuids);
  appendFromVisibility(visibleSet.objectUuids, overlaySet.objectUuids);

  AppendUuidIfRenderable(controller.m_highlightUuid, fixtures, trusses, objects,
                         overlaySet);

  for (const auto &uuid : controller.m_selectedUuids) {
    AppendUuidIfRenderable(uuid, fixtures, trusses, objects, overlaySet);
  }

  if (overlaySet.Empty())
    return;

  auto getTypeColor = [&](const std::string &key, const std::string &hex) {
    std::array<float, 3> c;
    if (!hex.empty() && HexToRGB(hex, c[0], c[1], c[2]))
      return c;
    return MakeDeterministicColor("type:" + key);
  };
  auto getLayerColor = [&](const std::string &key) {
    std::array<float, 3> c;
    auto opt = ConfigManager::Get().GetLayerColor(key);
    if (opt && HexToRGB(*opt, c[0], c[1], c[2]))
      return c;
    return MakeDeterministicColor("layer:" + key);
  };
  auto resolveSymbolView = [](Viewer2DView viewKind) {
    switch (viewKind) {
    case Viewer2DView::Top:
      return SymbolViewKind::Top;
    case Viewer2DView::Front:
      return SymbolViewKind::Front;
    case Viewer2DView::Side:
      return SymbolViewKind::Left;
    case Viewer2DView::Bottom:
    default:
      return SymbolViewKind::Bottom;
    }
  };
  auto getPickColor = [](const std::string &) {
    return std::array<float, 3>{1.0f, 1.0f, 1.0f};
  };

  RenderFrameContext overlayContext = context;
  overlayContext.skipCapture = true;
  overlayContext.selectionOverlayPass = true;
  OpaqueObjectPass::Render(controller, overlayContext, overlaySet, getLayerColor,
                           resolveSymbolView, getPickColor);
  OpaqueTrussPass::Render(controller, overlayContext, overlaySet, getLayerColor,
                          resolveSymbolView, getPickColor);
  OpaqueFixturePass::Render(controller, overlayContext, overlaySet, getTypeColor,
                            getLayerColor, resolveSymbolView, getPickColor);
}
