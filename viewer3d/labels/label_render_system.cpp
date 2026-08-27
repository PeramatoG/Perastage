#include "label_render_system.h"

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
#include <cstdlib>
#include <iomanip>
#include <chrono>
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
};

struct SupportLabelText {
  std::string name;
  std::string coordinates;
  std::string capacity;
  std::string load;
};


struct FixtureLayoutLine {
  std::string text;
  float baseSize = 0.0f;
  std::string fontFamily;
  bool useBoldFont = false;
};

struct FixtureLayoutKey {
  std::string uuid;
  std::string instanceName;
  std::string address;
  int fixtureId = 0;
  bool showName = false;
  bool showId = false;
  bool showDmx = false;
  float nameSize = 0.0f;
  float idSize = 0.0f;
  float dmxSize = 0.0f;
  float labelDist = 0.0f;
  float labelAngle = 0.0f;
  Viewer2DView view = Viewer2DView::Top;

  bool operator==(const FixtureLayoutKey &other) const {
    return uuid == other.uuid && instanceName == other.instanceName &&
           address == other.address && fixtureId == other.fixtureId &&
           showName == other.showName && showId == other.showId &&
           showDmx == other.showDmx && nameSize == other.nameSize &&
           idSize == other.idSize && dmxSize == other.dmxSize &&
           labelDist == other.labelDist && labelAngle == other.labelAngle &&
           view == other.view;
  }
};

struct FixtureLayoutCacheEntry {
  FixtureLayoutKey key;
  std::vector<FixtureLayoutLine> lines;
  float offX = 0.0f;
  float offY = 0.0f;
  float offZ = 0.0f;
};

struct FixtureLabelOverridesCache {
  std::optional<std::string> serialized;
  viewer2d::FixtureLabelOverrideMap overrides;
};

struct FixtureLabelLayoutCacheState {
  std::unordered_map<std::string, FixtureLayoutCacheEntry> fixtureLayoutByUuid;
  FixtureLabelOverridesCache overridesCache;
};

wxString WrapEveryTwoWords(const wxString &text);

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
  if (!cacheState.overridesCache.serialized.has_value() ||
      cacheState.overridesCache.serialized != serialized) {
    cacheState.overridesCache.serialized = serialized;
    cacheState.overridesCache.overrides = viewer2d::LoadFixtureLabelOverrides(cfg);
  }
  return cacheState.overridesCache.overrides;
}

FixtureLayoutCacheEntry BuildFixtureLayoutEntry(const FixtureLayoutKey &layoutKey,
                                                int regularFont,
                                                int boldFont) {
  constexpr const char *kRegularFamily = "sans";
  constexpr const char *kBoldFamily = "sans-bold";
  FixtureLayoutCacheEntry entry;
  entry.key = layoutKey;

  constexpr float deg2rad = 3.14159265358979323846f / 180.0f;
  const float angRad = layoutKey.labelAngle * deg2rad;
  switch (layoutKey.view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    entry.offX = layoutKey.labelDist * std::sin(angRad);
    entry.offY = layoutKey.labelDist * std::cos(angRad);
    break;
  case Viewer2DView::Front:
    entry.offX = layoutKey.labelDist * std::sin(angRad);
    entry.offZ = layoutKey.labelDist * std::cos(angRad);
    break;
  case Viewer2DView::Side:
    entry.offY = -layoutKey.labelDist * std::sin(angRad);
    entry.offZ = layoutKey.labelDist * std::cos(angRad);
    break;
  }

  if (layoutKey.showName) {
    // selection must not alter label text layout.
    wxString baseName = layoutKey.instanceName.empty()
                            ? wxString::FromUTF8(layoutKey.uuid)
                            : wxString::FromUTF8(layoutKey.instanceName);
    wxString wrapped = WrapEveryTwoWords(baseName);
    wxStringTokenizer nameLines(wrapped, "\n");
    while (nameLines.HasMoreTokens()) {
      wxString line = nameLines.GetNextToken();
      auto utf8 = line.ToUTF8();
      entry.lines.push_back(
          {std::string(utf8.data(), utf8.length()), layoutKey.nameSize,
           kRegularFamily, false});
    }
  }

  if (layoutKey.showId) {
    wxString idLine = wxString::Format(_("ID: %d"), layoutKey.fixtureId);
    auto utf8 = idLine.ToUTF8();
    entry.lines.push_back(
        {std::string(utf8.data(), utf8.length()), layoutKey.idSize, kRegularFamily,
         false});
  }

  if (layoutKey.showDmx && !layoutKey.address.empty()) {
    wxString addrLine = wxString::FromUTF8(layoutKey.address);
    auto utf8 = addrLine.ToUTF8();
    entry.lines.push_back(
        {std::string(utf8.data(), utf8.length()), layoutKey.dmxSize, kBoldFamily,
         boldFont >= 0 && boldFont != regularFont});
  }

  return entry;
}

std::vector<LabelLine2D> BuildDrawableLines(const FixtureLayoutCacheEntry &entry,
                                            float zoom, int regularFont,
                                            int boldFont) {
  std::vector<LabelLine2D> lines;
  lines.reserve(entry.lines.size());
  for (const auto &line : entry.lines) {
    const int font = line.useBoldFont ? boldFont : regularFont;
    lines.push_back(
        {font, line.text, line.baseSize * zoom, line.fontFamily});
  }
  return lines;
}

struct FixtureLabelCandidate {
  const std::string *uuid = nullptr;
  const Fixture *fixture = nullptr;
  double area = 0.0;
};

struct PreparedFixtureLabelLayout {
  const std::string *uuid = nullptr;
  const Fixture *fixture = nullptr;
  const FixtureLayoutCacheEntry *cachedLayout = nullptr;
  std::optional<FixtureLayoutCacheEntry> transientLayout;
};

struct DrawableFixtureLabelLayout {
  const PreparedFixtureLabelLayout *prepared = nullptr;
  std::vector<LabelLine2D> lines;
  int x = 0;
  int y = 0;
  double wx = 0.0;
  double wy = 0.0;
  double wz = 0.0;
  double area = 0.0;
};

FixtureLayoutKey BuildFixtureLayoutKey(
    const std::string &uuid, const Fixture &fixture,
    const viewer2d::FixtureLabelOverride *overrideSettings,
    const ConfigManager &cfg, Viewer2DView view, int viewIdx) {
  FixtureLayoutKey key;
  key.uuid = uuid;
  key.instanceName = fixture.instanceName;
  key.address = fixture.address;
  key.fixtureId = fixture.fixtureId;
  key.showName = viewer2d::ResolveShowLabelName(cfg, overrideSettings, viewIdx);
  key.showId = viewer2d::ResolveShowLabelId(cfg, overrideSettings, viewIdx);
  key.showDmx = viewer2d::ResolveShowLabelDmx(cfg, overrideSettings, viewIdx);
  key.nameSize = viewer2d::ResolveLabelFontSizeName(cfg, overrideSettings);
  key.idSize = viewer2d::ResolveLabelFontSizeId(cfg, overrideSettings);
  key.dmxSize = viewer2d::ResolveLabelFontSizeDmx(cfg, overrideSettings);
  key.labelDist =
      viewer2d::ResolveLabelOffsetDistance(cfg, overrideSettings, viewIdx);
  key.labelAngle =
      viewer2d::ResolveLabelOffsetAngle(cfg, overrideSettings, viewIdx);
  key.view = view;
  return key;
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
    return hidden.find(DEFAULT_LAYER_NAME) == hidden.end();
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

// Measures the on-screen rectangle occupied by the prepared 2D label lines.
ScreenRect MeasureLabelLinesScreenRect(NVGcontext *vg,
                                       const std::vector<LabelLine2D> &lines,
                                       int x, int y,
                                       int horizontalAlign = NVG_ALIGN_CENTER) {
  ScreenRect rect;
  if (!vg || lines.empty())
    return rect;

  constexpr float lineSpacing = 2.0f;
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

  float currentY = static_cast<float>(y) - totalHeight * 0.5f;
  for (size_t i = 0; i < lines.size(); ++i) {
    nvgFontSize(vg, lines[i].size);
    nvgFontFaceId(vg, lines[i].font);
    nvgTextAlign(vg, horizontalAlign | NVG_ALIGN_TOP);
    float bounds[4];
    nvgTextBounds(vg, static_cast<float>(x), currentY, lines[i].text.c_str(),
                  nullptr, bounds);
    rect.minX = std::min(rect.minX, static_cast<double>(bounds[0] - 1.0f));
    rect.minY = std::min(rect.minY, static_cast<double>(bounds[1] - 1.0f));
    rect.maxX = std::max(rect.maxX, static_cast<double>(bounds[2] + 1.0f));
    rect.maxY = std::max(rect.maxY, static_cast<double>(bounds[3] + 1.0f));
    currentY += heights[i] + lineSpacing;
  }

  return rect;
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

wxString WrapEveryTwoWords(const wxString &text) {
  wxStringTokenizer tk(text, " ");
  wxString result;
  int count = 0;
  while (tk.HasMoreTokens()) {
    if (count > 0) {
      if (count % 2 == 0)
        result += "\n";
      else
        result += " ";
    }
    result += tk.GetNextToken();
    ++count;
  }
  return result;
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

bool ShouldTraceLabelOrder() {
  static const bool enabled = std::getenv("PERASTAGE_TRACE_LABELS") != nullptr;
  return enabled;
}

} // namespace

LabelRenderSystem::~LabelRenderSystem() { EraseFixtureLayoutCacheState(this); }

void LabelRenderSystem::DrawFixtureLabels(int width, int height) {
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
    if (fixtureIt == fixtures.end() || uuid != m_controller.GetHighlightUuid())
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

// Draws all fixture labels for the 2D viewer while culling by label bounds.
void LabelRenderSystem::DrawAllFixtureLabels(int width, int height,
                                             Viewer2DView view, float zoom,
                                             bool interactiveLabelMode) {
  ConfigManager &cfg = ConfigManager::Get();
  const auto distanceUnitSystem =
      Units::ParseDistanceUnitSystem(cfg.GetValue("ui_distance_unit_system"));
  const auto weightUnitSystem =
      Units::ParseWeightUnitSystem(cfg.GetValue("ui_weight_unit_system"));
  ProjectionContext projection;
  FillProjectionContext(width, height, projection);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const int viewIdx = static_cast<int>(view);
  auto &layoutCacheState = GetFixtureLayoutCacheState(this);
  const auto &fixtureOverrides =
      GetCachedFixtureLabelOverrides(layoutCacheState, cfg);

  const CullingSettings culling = GetCullingSettings(cfg);
  const bool useLabelOptimizations =
      !interactiveLabelMode &&
      (cfg.GetFloat("label_optimizations_enabled") >= 0.5f);
  const int maxFixtureLabels = interactiveLabelMode
                                   ? 0
                                   : GetLabelLimit(cfg, "label_max_fixtures");

  constexpr float kDefaultBuildBudgetMs = 2.0f;
  const float configuredBudgetMs = cfg.GetFloat("label_layout_build_budget_ms");
  const float buildBudgetMs =
      configuredBudgetMs > 0.0f ? configuredBudgetMs : kDefaultBuildBudgetMs;
  const auto buildBudget =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
          std::chrono::duration<float, std::milli>(buildBudgetMs));
  const auto buildStart = std::chrono::steady_clock::now();
  bool buildBudgetExceeded = false;

  std::vector<FixtureLabelCandidate> candidates;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  candidates.reserve(fixtures.size());
  for (const auto &[uuid, f] : fixtures) {
    if (!IsLayerVisibleCached(hiddenLayers, f.layer) ||
        !cfg.IsFixtureTypeVisible(f.typeName))
      continue;

    candidates.push_back({&uuid, &f, 0.0});
  }

  for (auto it = layoutCacheState.fixtureLayoutByUuid.begin();
       it != layoutCacheState.fixtureLayoutByUuid.end();) {
    if (fixtures.find(it->first) == fixtures.end()) {
      it = layoutCacheState.fixtureLayoutByUuid.erase(it);
      continue;
    }
    ++it;
  }

  std::vector<PreparedFixtureLabelLayout> preparedLayouts;
  preparedLayouts.reserve(candidates.size());

  // Phase 1: build/cache fixture label layout on CPU.
  for (const auto &candidate : candidates) {
    const std::string &uuid = *candidate.uuid;
    const Fixture &f = *candidate.fixture;
    const viewer2d::FixtureLabelOverride *overrideSettings = nullptr;
    if (auto oit = fixtureOverrides.find(uuid); oit != fixtureOverrides.end())
      overrideSettings = &oit->second;

    const FixtureLayoutKey desiredKey =
        BuildFixtureLayoutKey(uuid, f, overrideSettings, cfg, view, viewIdx);
    if (!desiredKey.showName && !desiredKey.showId && !desiredKey.showDmx)
      continue;

    auto cacheIt = layoutCacheState.fixtureLayoutByUuid.find(uuid);
    const bool needsRebuild =
        cacheIt == layoutCacheState.fixtureLayoutByUuid.end() ||
        !(cacheIt->second.key == desiredKey);

    PreparedFixtureLabelLayout prepared;
    prepared.uuid = &uuid;
    prepared.fixture = &f;

    if (needsRebuild) {
      if (buildBudgetExceeded) {
        // Keep labels visible for this frame, even if budget is exhausted.
        prepared.transientLayout = BuildFixtureLayoutEntry(
            desiredKey, m_controller.GetLabelFont(), m_controller.GetLabelBoldFont());
      } else {
        cacheIt = layoutCacheState.fixtureLayoutByUuid
                      .insert_or_assign(uuid, BuildFixtureLayoutEntry(
                                                  desiredKey,
                                                  m_controller.GetLabelFont(),
                                                  m_controller.GetLabelBoldFont()))
                      .first;
        prepared.cachedLayout = &cacheIt->second;
        if (std::chrono::steady_clock::now() - buildStart > buildBudget)
          buildBudgetExceeded = true;
      }
    } else {
      prepared.cachedLayout = &cacheIt->second;
    }

    const FixtureLayoutCacheEntry &layout =
        prepared.transientLayout.has_value() ? *prepared.transientLayout
                                             : *prepared.cachedLayout;
    if (layout.lines.empty())
      continue;
    preparedLayouts.push_back(std::move(prepared));
  }

  std::vector<DrawableFixtureLabelLayout> drawableLayouts;
  drawableLayouts.reserve(preparedLayouts.size());

  // Phase 2: project labels and cull by label bounds instead of fixture bounds.
  for (const auto &prepared : preparedLayouts) {
    const std::string &uuid = *prepared.uuid;
    const Fixture &f = *prepared.fixture;
    const FixtureLayoutCacheEntry &layout =
        prepared.transientLayout.has_value() ? *prepared.transientLayout
                                             : *prepared.cachedLayout;

    auto bit = m_controller.GetFixtureBoundsMap().find(uuid);
    const ISelectionContext::BoundingBox *bounds =
        bit != m_controller.GetFixtureBoundsMap().end() ? &bit->second : nullptr;

    const auto anchor = ResolveAnchor(bounds, f.transform.o, true, view);
    const double wx = anchor[0] + layout.offX;
    const double wy = anchor[1] + layout.offY;
    const double wz = anchor[2] + layout.offZ;

    int x = 0;
    int y = 0;
    if (!ProjectLabelAnchor(projection, wx, wy, wz, x, y))
      continue;

    std::vector<LabelLine2D> lines =
        BuildDrawableLines(layout, zoom, m_controller.GetLabelFont(),
                           m_controller.GetLabelBoldFont() >= 0
                               ? m_controller.GetLabelBoldFont()
                               : m_controller.GetLabelFont());
    if (lines.empty())
      continue;

    ScreenRect labelRect = MeasureLabelLinesScreenRect(
        m_controller.GetNanoVGContext(), lines, x, y, NVG_ALIGN_CENTER);
    if (useLabelOptimizations && culling.enabled &&
        IsScreenRectMeasured(labelRect) &&
        ShouldCullLabelByScreenRect(labelRect, projection)) {
      continue;
    }

    DrawableFixtureLabelLayout drawable;
    drawable.prepared = &prepared;
    drawable.lines = std::move(lines);
    drawable.x = x;
    drawable.y = y;
    drawable.wx = wx;
    drawable.wy = wy;
    drawable.wz = wz;
    drawable.area = ComputeScreenRectArea(labelRect);
    drawableLayouts.push_back(std::move(drawable));
  }

  if (useLabelOptimizations && maxFixtureLabels > 0 &&
      static_cast<int>(drawableLayouts.size()) > maxFixtureLabels) {
    std::partial_sort(drawableLayouts.begin(),
                      drawableLayouts.begin() + maxFixtureLabels,
                      drawableLayouts.end(), [](const auto &a, const auto &b) {
                        return a.area > b.area;
                      });
    drawableLayouts.resize(maxFixtureLabels);
  }

  // Phase 3: draw and capture labels that remain visible in the current frame.
  for (const auto &drawable : drawableLayouts) {
    const std::string &uuid = *drawable.prepared->uuid;
    const auto &lines = drawable.lines;
    const int x = drawable.x;
    const int y = drawable.y;
    const double wx = drawable.wx;
    const double wy = drawable.wy;
    const double wz = drawable.wz;

    if (!interactiveLabelMode && m_controller.GetCaptureCanvas()) {
      std::string labelSourceKey = "label:" + uuid;
      m_controller.GetCaptureCanvas()->SetSourceKey(labelSourceKey);

      const float pxToWorld = 1.0f / (PIXELS_PER_METER * zoom);
      const float lineSpacingWorld = 2.0f * pxToWorld;
      constexpr float kPdfFixtureLabelLineAdvanceScale = 0.82f;

      std::vector<float> worldFontSizes;
      std::vector<float> lineHeightsWorld;
      std::vector<float> ascentsWorld;
      std::vector<float> descentsWorld;
      worldFontSizes.reserve(lines.size());
      lineHeightsWorld.reserve(lines.size());
      ascentsWorld.reserve(lines.size());
      descentsWorld.reserve(lines.size());

      for (const auto &ln : lines) {
        worldFontSizes.push_back(ln.size * pxToWorld);
        nvgFontSize(m_controller.GetNanoVGContext(), ln.size);
        nvgFontFaceId(m_controller.GetNanoVGContext(), ln.font);
        nvgTextAlign(m_controller.GetNanoVGContext(), NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        float bounds2D[4];
        nvgTextBounds(m_controller.GetNanoVGContext(), 0.f, 0.f, ln.text.c_str(), nullptr,
                      bounds2D);
        lineHeightsWorld.push_back((bounds2D[3] - bounds2D[1]) * pxToWorld);
        float ascender = 0.0f;
        float descender = 0.0f;
        float lineh = 0.0f;
        nvgTextMetrics(m_controller.GetNanoVGContext(), &ascender, &descender, &lineh);
        ascentsWorld.push_back(ascender * pxToWorld);
        descentsWorld.push_back(-descender * pxToWorld);
      }

      float totalHeight = 0.0f;
      for (size_t i = 0; i < lineHeightsWorld.size(); ++i) {
        totalHeight += lineHeightsWorld[i];
        if (i + 1 < lineHeightsWorld.size())
          totalHeight += lineSpacingWorld;
      }

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

      auto canvasAnchor = toPlan2D(wx, wy, wz, view);
      float currentY = canvasAnchor[1] + totalHeight * 0.5f;
      for (size_t i = 0; i < lines.size(); ++i) {
        CanvasTextStyle style;
        style.fontFamily = lines[i].fontFamily;
        style.fontSize = worldFontSizes[i];
        style.ascent = ascentsWorld[i];
        style.descent = descentsWorld[i];
        style.lineHeight = lineHeightsWorld[i];
        style.extraLineSpacing = lineSpacingWorld;
        style.color = {0.0f, 0.0f, 0.0f, 1.0f};
        style.outlineColor = {1.0f, 1.0f, 1.0f, 1.0f};
        style.outlineWidth = pxToWorld * 0.5f;
        style.hAlign = CanvasTextStyle::HorizontalAlign::Center;
        style.vAlign = CanvasTextStyle::VerticalAlign::Baseline;
        float baseline = currentY - style.ascent;
        if (ShouldTraceLabelOrder()) {
          std::ostringstream trace;
          trace << "[label-capture] fixture=" << uuid
                << " source=" << labelSourceKey << " text=\"" << lines[i].text
                << "\" x=" << canvasAnchor[0] << " baseline=" << baseline
                << " size=" << style.fontSize << " vAlign=Baseline";
          Logger::Instance().Log(trace.str());
        }
        m_controller.RecordText(canvasAnchor[0], baseline, lines[i].text, style);
        if (i + 1 < lines.size())
          currentY -=
              (lineHeightsWorld[i] + lineSpacingWorld) *
              kPdfFixtureLabelLineAdvanceScale;
      }
    }

    NVGcolor textColor =
        m_controller.IsDarkMode() ? nvgRGBAf(1.f, 1.f, 1.f, 1.f)
                                  : nvgRGBAf(0.f, 0.f, 0.f, 1.f);
    NVGcolor outlineColor =
        m_controller.IsDarkMode() ? nvgRGBAf(0.f, 0.f, 0.f, 1.f)
                                  : nvgRGBAf(1.f, 1.f, 1.f, 1.f);
    DrawLabelLines2D(m_controller.GetNanoVGContext(), lines, x, y, NVG_ALIGN_CENTER,
                     textColor, outlineColor, true);
  }

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
    if (!IsLayerVisibleCached(hiddenLayers, support.layer))
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
