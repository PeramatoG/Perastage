#include "layer_identity.h"
#include "label_render_system.h"
#include "fixture_label_presentation.h"
#include "composed_fixture_labels.h"
#include "fixture_label_composition.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#include <wx/translation.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "configmanager.h"
#include "fixture_label_overrides.h"
#include "logger.h"
#include "scenedatamanager.h"
#include "support.h"
#include "units/unit_label_utils.h"
#include "units/units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <iomanip>
#include <optional>
#include <unordered_map>
#include <nanovg.h>
#include <sstream>
#include <wx/tokenzr.h>

namespace {

static constexpr float LABEL_FONT_SIZE_3D = 18.0f;
static constexpr float LABEL_MAX_WIDTH = 300.0f;
static constexpr float PIXELS_PER_METER = 25.0f;

struct LabelLine2D {
  int font;
  std::string text;
  float size;
  std::string fontFamily;
  bool suppressed = false;
};

struct SupportLabelText {
  std::string name;
  std::string coordinates;
  std::string capacity;
  std::string load;
};


struct FixtureLabelOverridesCache {
  bool loaded = false;
  std::optional<std::string> serialized;
  viewer2d::FixtureLabelOverrideMap overrides;
};

struct FixtureLabelLayoutCacheState {
  viewer2d::FixtureLabelCompositionCache composed;
  size_t composedRevision = 0;
  FixtureLabelOverridesCache overridesCache;
};

std::unordered_map<LabelRenderSystem *, FixtureLabelLayoutCacheState>
    &FixtureLayoutCacheByOwner() {
  static std::unordered_map<LabelRenderSystem *, FixtureLabelLayoutCacheState>
      cacheByOwner;
  return cacheByOwner;
}

FixtureLabelLayoutCacheState &GetFixtureLayoutCacheState(LabelRenderSystem *owner) {
  return FixtureLayoutCacheByOwner()[owner];
}

void EraseFixtureLayoutCacheState(LabelRenderSystem *owner) {
  FixtureLayoutCacheByOwner().erase(owner);
}

const viewer2d::FixtureLabelOverrideMap &GetCachedFixtureLabelOverrides(
    FixtureLabelLayoutCacheState &cacheState, const ConfigManager &cfg) {
  const std::optional<std::string> serialized =
      cfg.GetValue("label_fixture_overrides");
  if (!cacheState.overridesCache.loaded ||
      cacheState.overridesCache.serialized != serialized) {
    cacheState.overridesCache.loaded = true;
    cacheState.overridesCache.serialized = serialized;
    cacheState.overridesCache.overrides = viewer2d::LoadFixtureLabelOverrides(cfg);
  }
  return cacheState.overridesCache.overrides;
}

struct CullingSettings {
  bool enabled = true;
  float minPixels3D = 2.0f;
  float minPixels2D = 1.0f;
};

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

struct ProjectionContext {
  double model[16];
  double proj[16];
  int viewport[4];
  int width = 0;
  int height = 0;
};

void FillProjectionContext(int width, int height, ProjectionContext &ctx) {
  ctx.width = width;
  ctx.height = height;
  glGetDoublev(GL_MODELVIEW_MATRIX, ctx.model);
  glGetDoublev(GL_PROJECTION_MATRIX, ctx.proj);
  glGetIntegerv(GL_VIEWPORT, ctx.viewport);
}

ISelectionContext::ViewFrustumSnapshot
BuildFrustum(const ProjectionContext &ctx) {
  ISelectionContext::ViewFrustumSnapshot frustum{};
  std::copy(std::begin(ctx.viewport), std::end(ctx.viewport),
            std::begin(frustum.viewport));
  std::copy(std::begin(ctx.model), std::end(ctx.model), std::begin(frustum.model));
  std::copy(std::begin(ctx.proj), std::end(ctx.proj),
            std::begin(frustum.projection));
  return frustum;
}

std::unordered_set<std::string> SnapshotHiddenLayers(const ConfigManager &cfg) {
  return cfg.GetHiddenLayers();
}

bool IsLayerVisibleCached(const std::unordered_set<std::string> &hidden,
                          const std::string &layer) {
  if (layer.empty())
    return hidden.find(layerdomain::DefaultLayerUuid()) == hidden.end();
  return hidden.find(layer) == hidden.end();
}

CullingSettings GetCullingSettings(const ConfigManager &cfg) {
  CullingSettings s{};
  s.enabled = cfg.GetFloat("render_culling_enabled") >= 0.5f;
  s.minPixels3D = std::max(0.0f, cfg.GetFloat("render_culling_min_pixels_3d"));
  s.minPixels2D = std::max(0.0f, cfg.GetFloat("render_culling_min_pixels_2d"));
  return s;
}

int GetLabelLimit(const ConfigManager &cfg, const char *key) {
  return std::max(0, static_cast<int>(std::lround(cfg.GetFloat(key))));
}

bool ProjectBoundingBoxToScreen(const std::array<float, 3> &bbMin,
                                const std::array<float, 3> &bbMax,
                                const ProjectionContext &ctx,
                                ScreenRect &outRect,
                                bool &outAnyDepthVisible) {
  outRect = ScreenRect{};
  outAnyDepthVisible = false;
  bool projected = false;

  std::array<std::array<float, 3>, 8> corners = {
      std::array<float, 3>{bbMin[0], bbMin[1], bbMin[2]},
      {bbMax[0], bbMin[1], bbMin[2]},
      {bbMin[0], bbMax[1], bbMin[2]},
      {bbMax[0], bbMax[1], bbMin[2]},
      {bbMin[0], bbMin[1], bbMax[2]},
      {bbMax[0], bbMin[1], bbMax[2]},
      {bbMin[0], bbMax[1], bbMax[2]},
      {bbMax[0], bbMax[1], bbMax[2]}};

  for (const auto &c : corners) {
    double sx, sy, sz;
    if (gluProject(c[0], c[1], c[2], ctx.model, ctx.proj, ctx.viewport, &sx, &sy,
                   &sz) == GL_TRUE) {
      projected = true;
      outRect.minX = std::min(outRect.minX, sx);
      outRect.maxX = std::max(outRect.maxX, sx);
      const double sy2 = static_cast<double>(ctx.height) - sy;
      outRect.minY = std::min(outRect.minY, sy2);
      outRect.maxY = std::max(outRect.maxY, sy2);
      if (sz >= 0.0 && sz <= 1.0)
        outAnyDepthVisible = true;
    }
  }

  return projected;
}

bool ShouldCullByScreenRect(const ScreenRect &rect, const ProjectionContext &ctx,
                            float minPixels) {
  if (rect.maxX < 0.0 || rect.minX > static_cast<double>(ctx.width) ||
      rect.maxY < 0.0 || rect.minY > static_cast<double>(ctx.height)) {
    return true;
  }

  const double screenWidth = rect.maxX - rect.minX;
  const double screenHeight = rect.maxY - rect.minY;
  return screenWidth < static_cast<double>(minPixels) &&
         screenHeight < static_cast<double>(minPixels);
}


// Returns whether a screen rectangle contains measured finite extents.
bool IsScreenRectMeasured(const ScreenRect &rect) {
  return rect.minX <= rect.maxX && rect.minY <= rect.maxY;
}

// Returns whether a label rectangle is completely outside the viewport.
bool ShouldCullLabelByScreenRect(const ScreenRect &rect,
                                 const ProjectionContext &ctx) {
  return rect.maxX < 0.0 || rect.minX > static_cast<double>(ctx.width) ||
         rect.maxY < 0.0 || rect.minY > static_cast<double>(ctx.height);
}



// Computes the pixel area covered by a measured screen rectangle.
double ComputeScreenRectArea(const ScreenRect &rect) {
  return std::max(0.0, rect.maxX - rect.minX) *
         std::max(0.0, rect.maxY - rect.minY);
}

bool ProjectLabelAnchor(const ProjectionContext &ctx, double wx, double wy,
                        double wz, int &outX, int &outY) {
  double sx, sy, sz;
  if (gluProject(wx, wy, wz, ctx.model, ctx.proj, ctx.viewport, &sx, &sy, &sz) !=
      GL_TRUE) {
    return false;
  }
  outX = static_cast<int>(sx);
  outY = ctx.height - static_cast<int>(sy);
  return true;
}

std::array<double, 3>
ResolveAnchor(const ISelectionContext::BoundingBox *bounds,
              const std::array<float, 3> &fallbackOrigin,
              bool anchorTop = false,
              Viewer2DView view = Viewer2DView::Top) {
  if (!bounds) {
    return {fallbackOrigin[0] * RENDER_SCALE, fallbackOrigin[1] * RENDER_SCALE,
            fallbackOrigin[2] * RENDER_SCALE};
  }

  double x = (bounds->min[0] + bounds->max[0]) * 0.5;
  double y = (bounds->min[1] + bounds->max[1]) * 0.5;
  double z = (bounds->min[2] + bounds->max[2]) * 0.5;

  if (anchorTop) {
    switch (view) {
    case Viewer2DView::Top:
    case Viewer2DView::Bottom:
      y = bounds->max[1];
      break;
    case Viewer2DView::Front:
    case Viewer2DView::Side:
      z = bounds->max[2];
      break;
    }
  }

  return {x, y, z};
}

// Formats a distance for rendered labels using the selected project units.
std::string FormatDistanceLabel(float mm, Units::DistanceUnitSystem distanceUnit) {
  return Units::DistanceValueWithUnit(mm, distanceUnit,
                                      Units::ValueFormatContext::Label);
}

SupportLabelText BuildSupportLabelText(const Support &support,
                                       const std::string &fallbackUuid,
                                       Units::DistanceUnitSystem distanceUnit,
                                       Units::WeightUnitSystem weightUnit) {
  SupportLabelText text;
  const std::string distanceSuffix = Units::DistanceUnitSuffix(distanceUnit);
  const std::string weightSuffix = Units::WeightUnitSuffix(weightUnit);
  const auto effectiveData = ResolveEffectiveSupportData(support);
  text.name = support.name.empty() ? fallbackUuid : support.name;
  text.coordinates = "X: " +
                     Units::FormatDistanceFromMillimeters(
                         support.transform.o[0], distanceUnit,
                         Units::ValueFormatContext::Label) +
                     " " + distanceSuffix + ", Y: " +
                     Units::FormatDistanceFromMillimeters(
                         support.transform.o[1], distanceUnit,
                         Units::ValueFormatContext::Label) +
                     " " + distanceSuffix;
  text.capacity = "Capacity: " +
                  Units::FormatWeightFromKilograms(
                      effectiveData.capacityKg, weightUnit,
                      Units::ValueFormatContext::Label) +
                  " " + weightSuffix;
  text.load = "Load: " +
              Units::FormatWeightFromKilograms(
                  support.loadKg, weightUnit, Units::ValueFormatContext::Label) +
              " " + weightSuffix;
  return text;
}



void DrawText2D(NVGcontext *vg, int font, const std::string &text, int x, int y,
                float fontSize = LABEL_FONT_SIZE_3D,
                float maxWidth = LABEL_MAX_WIDTH,
                bool drawBackground = true, bool drawBorder = true,
                NVGcolor textColor = nvgRGBAf(1.f, 1.f, 1.f, 1.f)) {
  if (!vg || font < 0 || text.empty())
    return;

  std::string normalizedText = text;
  size_t pos = 0;
  while ((pos = normalizedText.find("\r\n", pos)) != std::string::npos) {
    normalizedText.replace(pos, 2, "\n");
  }
  for (char &ch : normalizedText) {
    if (ch == '\r')
      ch = '\n';
  }

  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);

  nvgBeginFrame(vg, vp[2], vp[3], 1.0f);
  nvgSave(vg);
  nvgFontSize(vg, fontSize);
  nvgFontFaceId(vg, font);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  float textWidth = 0.0f;
  size_t start = 0;
  while (start <= normalizedText.size()) {
    size_t end = normalizedText.find('\n', start);
    std::string line = normalizedText.substr(start, end - start);
    float lb[4];
    nvgTextBounds(vg, 0.f, 0.f, line.c_str(), nullptr, lb);
    textWidth = std::max(textWidth, lb[2] - lb[0]);
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (maxWidth > 0.0f)
    textWidth = std::min(textWidth, maxWidth);
  const int padding = 4;

  float bounds[4];
  nvgTextBoxBounds(vg, static_cast<float>(x), static_cast<float>(y), textWidth,
                   normalizedText.c_str(), nullptr, bounds);

  if (drawBackground) {
    nvgBeginPath(vg);
    nvgRect(vg, bounds[0] - padding, bounds[1] - padding,
            (bounds[2] - bounds[0]) + padding * 2,
            (bounds[3] - bounds[1]) + padding * 2);
    nvgFillColor(vg, nvgRGBAf(0.f, 0.f, 0.f, 0.6f));
    nvgFill(vg);
  }

  if (drawBorder) {
    nvgBeginPath(vg);
    nvgRect(vg, bounds[0] - padding, bounds[1] - padding,
            (bounds[2] - bounds[0]) + padding * 2,
            (bounds[3] - bounds[1]) + padding * 2);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.8f));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
  }

  nvgFillColor(vg, textColor);
  nvgTextBox(vg, static_cast<float>(x), static_cast<float>(y), textWidth,
             normalizedText.c_str(), nullptr);
  nvgRestore(vg);
  nvgEndFrame(vg);
}

void DrawLabelLines2D(NVGcontext *vg, const std::vector<LabelLine2D> &lines,
                      int x, int y, int horizontalAlign = NVG_ALIGN_CENTER,
                      NVGcolor textColor = nvgRGBAf(1.f, 1.f, 1.f, 1.f),
                      NVGcolor outlineColor = nvgRGBAf(0.f, 0.f, 0.f, 1.f),
                      bool outline = false) {
  if (!vg || lines.empty())
    return;

  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);
  nvgBeginFrame(vg, vp[2], vp[3], 1.0f);
  nvgSave(vg);

  const float lineSpacing = 2.0f;
  std::vector<float> heights(lines.size());
  for (size_t i = 0; i < lines.size(); ++i) {
    nvgFontSize(vg, lines[i].size);
    nvgFontFaceId(vg, lines[i].font);
    nvgTextAlign(vg, horizontalAlign | NVG_ALIGN_TOP);
    float bounds[4];
    nvgTextBounds(vg, 0.f, 0.f, lines[i].text.c_str(), nullptr, bounds);
    heights[i] = bounds[3] - bounds[1];
  }

  float totalHeight = 0.0f;
  for (size_t i = 0; i < heights.size(); ++i) {
    totalHeight += heights[i];
    if (i + 1 < heights.size())
      totalHeight += lineSpacing;
  }

  float currentY = y - totalHeight * 0.5f;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].suppressed) {
      currentY += heights[i] + lineSpacing;
      continue;
    }
    nvgFontSize(vg, lines[i].size);
    nvgFontFaceId(vg, lines[i].font);
    nvgTextAlign(vg, horizontalAlign | NVG_ALIGN_TOP);
    if (outline) {
      nvgFillColor(vg, outlineColor);
      const std::array<std::array<float, 2>, 8> offsets = {
          std::array<float, 2>{-1.f, 0.f}, std::array<float, 2>{1.f, 0.f},
          std::array<float, 2>{0.f, -1.f}, std::array<float, 2>{0.f, 1.f},
          std::array<float, 2>{-1.f, -1.f}, std::array<float, 2>{1.f, -1.f},
          std::array<float, 2>{-1.f, 1.f}, std::array<float, 2>{1.f, 1.f}};
      for (const auto &offset : offsets) {
        nvgText(vg, static_cast<float>(x) + offset[0], currentY + offset[1],
                lines[i].text.c_str(), nullptr);
      }
    }
    nvgFillColor(vg, textColor);
    nvgText(vg, static_cast<float>(x), currentY, lines[i].text.c_str(), nullptr);
    currentY += heights[i] + lineSpacing;
  }

  nvgRestore(vg);
  nvgEndFrame(vg);
}



} // namespace

LabelRenderSystem::~LabelRenderSystem() { EraseFixtureLayoutCacheState(this); }

void LabelRenderSystem::DrawFixtureLabels(
    int width, int height,
    const std::unordered_set<std::string> *provisionalFixtures) {
  const auto &highlight = m_controller.GetHighlightUuid();
  if (highlight.empty() ||
      (provisionalFixtures && provisionalFixtures->contains(highlight)))
    return;

  ConfigManager &cfg = ConfigManager::Get();
  ProjectionContext projection;
  FillProjectionContext(width, height, projection);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const CullingSettings culling = GetCullingSettings(cfg);
  const float minLabelPixels = culling.minPixels3D;
  const bool useLabelOptimizations =
      cfg.GetFloat("label_optimizations_enabled") >= 0.5f;
  const bool showName = cfg.GetFloat("label_show_name") != 0.0f;
  const bool showId = cfg.GetFloat("label_show_id") != 0.0f;
  const bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  const auto &visibleSet = m_controller.GetVisibleSet(
      BuildFrustum(projection), hiddenLayers, culling.enabled, minLabelPixels);

  for (const auto &uuid : visibleSet.fixtureUuids) {
    auto fixtureIt = fixtures.find(uuid);
    if (fixtureIt == fixtures.end() || uuid != highlight)
      continue;

    const auto &f = fixtureIt->second;
    const auto bit = m_controller.GetFixtureBoundsMap().find(uuid);
    const ISelectionContext::BoundingBox *bounds =
        bit != m_controller.GetFixtureBoundsMap().end() ? &bit->second : nullptr;

    if (useLabelOptimizations && culling.enabled && bounds) {
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bounds->min, bounds->max, projection, rect,
                                      anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, projection, minLabelPixels)) {
        continue;
      }
    }

    const auto anchor = ResolveAnchor(bounds, f.transform.o);
    int x = 0;
    int y = 0;
    if (!ProjectLabelAnchor(projection, anchor[0], anchor[1], anchor[2], x, y))
      continue;

    wxString label;
    if (showName)
      label = f.instanceName.empty() ? wxString::FromUTF8(uuid)
                                     : wxString::FromUTF8(f.instanceName);
    if (showId) {
      if (!label.empty())
        label += "\n";
      label += "ID: " + wxString::Format("%d", f.fixtureId);
    }
    if (showDmx && !f.address.empty()) {
      if (!label.empty())
        label += "\n";
      label += wxString::FromUTF8(f.address);
    }
    if (label.empty())
      continue;

    auto utf8 = label.ToUTF8();
    DrawText2D(m_controller.GetNanoVGContext(), m_controller.GetLabelFont(),
               std::string(utf8.data(), utf8.length()), x, y);
  }
}

// Uses canonical fixture composition; support labels retain their own adapter.
void LabelRenderSystem::DrawAllFixtureLabels(int width, int height,
                                             Viewer2DView view, float zoom,
                                             bool interactiveLabelMode,
                                             const std::unordered_set<std::string> *provisionalFixtures) {
  ConfigManager &cfg = ConfigManager::Get();
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  const auto weightUnitSystem =
      Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
  ProjectionContext projection;
  FillProjectionContext(width, height, projection);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  auto &layoutCacheState = GetFixtureLayoutCacheState(this);
  const auto &fixtureOverrides =
      m_controller.GetFixtureLabelPresentation() ? m_controller.GetFixtureLabelPresentation()->styles :
      GetCachedFixtureLabelOverrides(layoutCacheState, cfg);

  if (layoutCacheState.composedRevision != cfg.GetRenderRevision()) {
    layoutCacheState.composed.PruneRemovedFixtures(cfg.GetScene());
    layoutCacheState.composedRevision = cfg.GetRenderRevision();
  }
  const bool cullOffscreen = !m_controller.GetCaptureCanvas() &&
      (interactiveLabelMode ||
       (cfg.GetFloat("label_optimizations_enabled") >= 0.5f &&
        cfg.GetFloat("render_culling_enabled") >= 0.5f));
  DrawComposedFixtureLabels(m_controller, width, height, view, zoom,
                            fixtureOverrides, layoutCacheState.composed,
                            BuildFrustum(projection), hiddenLayers,
                            cfg.GetHiddenFixtureTypes(), cullOffscreen,
                            provisionalFixtures);
  const auto &supports = cfg.GetScene().supports;
  constexpr const char *kRegularFamily = "sans";
  // Keep default support label sizing aligned with fixture labels at Size = 3.
  constexpr float kSupportNameSizePx = 3.0f;
  constexpr float kSupportBodySizePx = 3.0f;
  constexpr float kSupportTopGapPx = 8.0f;
  constexpr float kSupportBottomGapPx = 8.0f;
  constexpr float kSupportRightGapPx = 8.0f;
  constexpr float kSupportInfoTopGapPx = 3.0f;
  constexpr float kSupportLineSpacingPx = 2.0f;

  for (const auto &[uuid, support] : supports) {
    if (!IsLayerVisibleCached(hiddenLayers, support.layerUuid))
      continue;

    const double wx = support.transform.o[0] * RENDER_SCALE;
    const double wy = support.transform.o[1] * RENDER_SCALE;
    const double wz = support.transform.o[2] * RENDER_SCALE;

    int x = 0;
    int y = 0;
    if (!ProjectLabelAnchor(projection, wx, wy, wz, x, y))
      continue;

    const SupportLabelText text = BuildSupportLabelText(
        support, uuid, distanceUnitSystem, weightUnitSystem);
    const float pxToWorld = 1.0f / (PIXELS_PER_METER * zoom);

    std::vector<LabelLine2D> titleLines = {
        {m_controller.GetLabelBoldFont() >= 0 ? m_controller.GetLabelBoldFont()
                                              : m_controller.GetLabelFont(),
         text.name, kSupportNameSizePx * zoom, kRegularFamily}};
    std::vector<LabelLine2D> coordLines = {
        {m_controller.GetLabelFont(), text.coordinates, kSupportBodySizePx * zoom,
         kRegularFamily}};
    std::vector<LabelLine2D> infoLines = {
        {m_controller.GetLabelFont(), text.capacity, kSupportBodySizePx * zoom,
         kRegularFamily},
        {m_controller.GetLabelFont(), text.load, kSupportBodySizePx * zoom,
         kRegularFamily}};

    if (m_controller.GetCaptureCanvas()) {
      std::string labelSourceKey = "support-label:" + uuid;
      m_controller.GetCaptureCanvas()->SetSourceKey(labelSourceKey);

      auto toPlan2D = [](double px, double py, double pz, Viewer2DView labelView) {
        switch (labelView) {
        case Viewer2DView::Top:
        case Viewer2DView::Bottom:
          return std::array<float, 2>{static_cast<float>(px), static_cast<float>(py)};
        case Viewer2DView::Front:
          return std::array<float, 2>{static_cast<float>(px), static_cast<float>(pz)};
        case Viewer2DView::Side:
          return std::array<float, 2>{static_cast<float>(-py), static_cast<float>(pz)};
        }
        return std::array<float, 2>{static_cast<float>(px), static_cast<float>(py)};
      };

      auto recordSingleLine = [&](const std::string &value, float worldX, float worldY,
                                  float sizePx,
                                  CanvasTextStyle::HorizontalAlign hAlign) {
        CanvasTextStyle style;
        style.fontFamily = kRegularFamily;
        style.fontSize = sizePx * pxToWorld;
        nvgFontSize(m_controller.GetNanoVGContext(), sizePx * zoom);
        nvgFontFaceId(m_controller.GetNanoVGContext(), m_controller.GetLabelFont());
        float ascender = 0.0f;
        float descender = 0.0f;
        float lineh = 0.0f;
        nvgTextMetrics(m_controller.GetNanoVGContext(), &ascender, &descender, &lineh);
        style.ascent = ascender * pxToWorld;
        style.descent = -descender * pxToWorld;
        style.lineHeight = lineh * pxToWorld;
        style.extraLineSpacing = kSupportLineSpacingPx * pxToWorld;
        style.color = {0.0f, 0.0f, 0.0f, 1.0f};
        style.outlineColor = {1.0f, 1.0f, 1.0f, 1.0f};
        style.outlineWidth = pxToWorld * 0.5f;
        style.hAlign = hAlign;
        style.vAlign = CanvasTextStyle::VerticalAlign::Baseline;
        m_controller.RecordText(worldX, worldY, value, style);
      };

      const auto canvasAnchor = toPlan2D(wx, wy, wz, view);
      const float titleBaseline = canvasAnchor[1] + kSupportTopGapPx * pxToWorld;
      const float coordBaseline = canvasAnchor[1] - kSupportBottomGapPx * pxToWorld;
      const float infoX = canvasAnchor[0] + kSupportRightGapPx * pxToWorld;
      const float infoTop = canvasAnchor[1] + kSupportInfoTopGapPx * pxToWorld;

      recordSingleLine(text.name, canvasAnchor[0], titleBaseline, kSupportNameSizePx,
                       CanvasTextStyle::HorizontalAlign::Center);
      recordSingleLine(text.coordinates, canvasAnchor[0], coordBaseline, kSupportBodySizePx,
                       CanvasTextStyle::HorizontalAlign::Center);
      recordSingleLine(text.capacity, infoX, infoTop, kSupportBodySizePx,
                       CanvasTextStyle::HorizontalAlign::Left);
      recordSingleLine(text.load, infoX,
                       infoTop - (kSupportBodySizePx + kSupportLineSpacingPx) * pxToWorld,
                       kSupportBodySizePx, CanvasTextStyle::HorizontalAlign::Left);
    }

    NVGcolor textColor =
        m_controller.IsDarkMode() ? nvgRGBAf(1.f, 1.f, 1.f, 1.f)
                                  : nvgRGBAf(0.f, 0.f, 0.f, 1.f);
    NVGcolor outlineColor =
        m_controller.IsDarkMode() ? nvgRGBAf(0.f, 0.f, 0.f, 1.f)
                                  : nvgRGBAf(1.f, 1.f, 1.f, 1.f);
    DrawLabelLines2D(m_controller.GetNanoVGContext(), titleLines, x,
                     static_cast<int>(std::lround(y - kSupportTopGapPx * zoom)),
                     NVG_ALIGN_CENTER, textColor, outlineColor, true);
    DrawLabelLines2D(m_controller.GetNanoVGContext(), coordLines, x,
                     static_cast<int>(std::lround(y + kSupportBottomGapPx * zoom)),
                     NVG_ALIGN_CENTER, textColor, outlineColor, true);
    DrawLabelLines2D(
        m_controller.GetNanoVGContext(), infoLines,
        static_cast<int>(std::lround(x + kSupportRightGapPx * zoom)),
        static_cast<int>(std::lround(y - kSupportInfoTopGapPx * zoom)), NVG_ALIGN_LEFT,
        textColor, outlineColor, true);
  }
}

// Draws visible truss labels in the 3D viewport.
void LabelRenderSystem::DrawTrussLabels(int width, int height) {
  ConfigManager &cfg = ConfigManager::Get();
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  ProjectionContext projection;
  FillProjectionContext(width, height, projection);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const CullingSettings culling = GetCullingSettings(cfg);
  const float minLabelPixels = culling.minPixels3D;
  const bool useLabelOptimizations =
      cfg.GetFloat("label_optimizations_enabled") >= 0.5f;
  int labelsDrawn = 0;
  const int maxLabels = GetLabelLimit(cfg, "label_max_trusses");
  const auto &trusses = SceneDataManager::Instance().GetTrusses();

  const auto &visibleSet = m_controller.GetVisibleSet(
      BuildFrustum(projection), hiddenLayers, culling.enabled, minLabelPixels);
  for (const auto &uuid : visibleSet.trussUuids) {
    auto trussIt = trusses.find(uuid);
    if (trussIt == trusses.end() || uuid != m_controller.GetHighlightUuid())
      continue;
    if (useLabelOptimizations && maxLabels > 0 && labelsDrawn >= maxLabels)
      break;

    const auto &t = trussIt->second;
    const auto bit = m_controller.GetTrussBoundsMap().find(uuid);
    const ISelectionContext::BoundingBox *bounds =
        bit != m_controller.GetTrussBoundsMap().end() ? &bit->second : nullptr;

    if (useLabelOptimizations && culling.enabled && bounds) {
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bounds->min, bounds->max, projection, rect,
                                      anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, projection, minLabelPixels)) {
        continue;
      }
    }

    const auto anchor = ResolveAnchor(bounds, t.transform.o);
    int x = 0;
    int y = 0;
    if (!ProjectLabelAnchor(projection, anchor[0], anchor[1], anchor[2], x, y))
      continue;

    wxString label = t.name.empty() ? wxString::FromUTF8(uuid)
                                    : wxString::FromUTF8(t.name);
    float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
    const std::string heightText =
        FormatDistanceLabel(baseHeight, distanceUnitSystem);
    label += "\nh = " + wxString::FromUTF8(heightText);

    auto utf8 = label.ToUTF8();
    DrawText2D(m_controller.GetNanoVGContext(), m_controller.GetLabelFont(),
               std::string(utf8.data(), utf8.length()), x, y);
    ++labelsDrawn;
  }
}

void LabelRenderSystem::DrawSceneObjectLabels(int width, int height) {
  ConfigManager &cfg = ConfigManager::Get();
  ProjectionContext projection;
  FillProjectionContext(width, height, projection);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const CullingSettings culling = GetCullingSettings(cfg);
  const float minLabelPixels = culling.minPixels3D;
  const bool useLabelOptimizations =
      cfg.GetFloat("label_optimizations_enabled") >= 0.5f;
  int labelsDrawn = 0;
  const int maxLabels = GetLabelLimit(cfg, "label_max_objects");
  const auto &objects = SceneDataManager::Instance().GetSceneObjects();

  const auto &visibleSet = m_controller.GetVisibleSet(
      BuildFrustum(projection), hiddenLayers, culling.enabled, minLabelPixels);
  for (const auto &uuid : visibleSet.objectUuids) {
    auto objectIt = objects.find(uuid);
    if (objectIt == objects.end() || uuid != m_controller.GetHighlightUuid())
      continue;
    if (useLabelOptimizations && maxLabels > 0 && labelsDrawn >= maxLabels)
      break;

    const auto &obj = objectIt->second;
    const auto bit = m_controller.GetObjectBoundsMap().find(uuid);
    const ISelectionContext::BoundingBox *bounds =
        bit != m_controller.GetObjectBoundsMap().end() ? &bit->second : nullptr;

    if (useLabelOptimizations && culling.enabled && bounds) {
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bounds->min, bounds->max, projection, rect,
                                      anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, projection, minLabelPixels)) {
        continue;
      }
    }

    const auto anchor = ResolveAnchor(bounds, obj.transform.o);
    int x = 0;
    int y = 0;
    if (!ProjectLabelAnchor(projection, anchor[0], anchor[1], anchor[2], x, y))
      continue;

    wxString label = obj.name.empty() ? wxString::FromUTF8(uuid)
                                      : wxString::FromUTF8(obj.name);
    auto utf8 = label.ToUTF8();
    DrawText2D(m_controller.GetNanoVGContext(), m_controller.GetLabelFont(),
               std::string(utf8.data(), utf8.length()), x, y);
    ++labelsDrawn;
  }
}
