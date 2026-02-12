#include "label_render_system.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText
#endif

#include <GL/glew.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "../../core/configmanager.h"
#include "../../core/logger.h"
#include "../../core/scenedatamanager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <iomanip>
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

Viewer3DController::ViewFrustumSnapshot
BuildFrustum(const ProjectionContext &ctx) {
  Viewer3DController::ViewFrustumSnapshot frustum{};
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
ResolveAnchor(const Viewer3DController::BoundingBox *bounds,
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

std::string FormatMeters(float mm) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << mm / 1000.0f;
  std::string s = oss.str();
  s.erase(s.find_last_not_of('0') + 1, std::string::npos);
  if (!s.empty() && s.back() == '.')
    s.pop_back();
  return s;
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
                      int x, int y,
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
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
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
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
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
    if (fixtureIt == fixtures.end() || uuid != m_controller.m_highlightUuid)
      continue;

    const auto &f = fixtureIt->second;
    const auto bit = m_controller.m_fixtureBounds.find(uuid);
    const Viewer3DController::BoundingBox *bounds =
        bit != m_controller.m_fixtureBounds.end() ? &bit->second : nullptr;

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
    DrawText2D(m_controller.m_vg, m_controller.m_font,
               std::string(utf8.data(), utf8.length()), x, y);
  }
}

void LabelRenderSystem::DrawAllFixtureLabels(int width, int height,
                                             Viewer2DView view, float zoom) {
  ConfigManager &cfg = ConfigManager::Get();
  ProjectionContext projection;
  FillProjectionContext(width, height, projection);

  const auto hiddenLayers = SnapshotHiddenLayers(cfg);
  const std::array<const char *, 4> nameKeys = {
      "label_show_name_top", "label_show_name_front", "label_show_name_side",
      "label_show_name_top"};
  const std::array<const char *, 4> idKeys = {
      "label_show_id_top", "label_show_id_front", "label_show_id_side",
      "label_show_id_top"};
  const std::array<const char *, 4> dmxKeys = {
      "label_show_dmx_top", "label_show_dmx_front", "label_show_dmx_side",
      "label_show_dmx_top"};
  const std::array<const char *, 4> distKeys = {
      "label_offset_distance_top", "label_offset_distance_front",
      "label_offset_distance_side", "label_offset_distance_top"};
  const std::array<const char *, 4> angleKeys = {
      "label_offset_angle_top", "label_offset_angle_front",
      "label_offset_angle_side", "label_offset_angle_top"};

  int viewIdx = static_cast<int>(view);
  const bool showName = cfg.GetFloat(nameKeys[viewIdx]) != 0.0f;
  const bool showId = cfg.GetFloat(idKeys[viewIdx]) != 0.0f;
  const bool showDmx = cfg.GetFloat(dmxKeys[viewIdx]) != 0.0f;
  const float nameSize = cfg.GetFloat("label_font_size_name") * zoom;
  const float idSize = cfg.GetFloat("label_font_size_id") * zoom;
  const float dmxSize = cfg.GetFloat("label_font_size_dmx") * zoom;
  const float labelDist = cfg.GetFloat(distKeys[viewIdx]);
  const float labelAngle = cfg.GetFloat(angleKeys[viewIdx]);

  constexpr float deg2rad = 3.14159265358979323846f / 180.0f;
  const float angRad = labelAngle * deg2rad;
  float offX = 0.0f;
  float offY = 0.0f;
  float offZ = 0.0f;
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    offX = labelDist * std::sin(angRad);
    offY = labelDist * std::cos(angRad);
    break;
  case Viewer2DView::Front:
    offX = labelDist * std::sin(angRad);
    offZ = labelDist * std::cos(angRad);
    break;
  case Viewer2DView::Side:
    offY = -labelDist * std::sin(angRad);
    offZ = labelDist * std::cos(angRad);
    break;
  }

  const CullingSettings culling = GetCullingSettings(cfg);
  const float minLabelPixels = culling.minPixels2D;
  const bool useLabelOptimizations =
      cfg.GetFloat("label_optimizations_enabled") >= 0.5f;
  const int maxFixtureLabels = GetLabelLimit(cfg, "label_max_fixtures");

  struct FixtureLabelCandidate {
    const std::string *uuid = nullptr;
    const Fixture *fixture = nullptr;
    double area = 0.0;
  };
  std::vector<FixtureLabelCandidate> candidates;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  candidates.reserve(fixtures.size());
  for (const auto &[uuid, f] : fixtures) {
    if (!IsLayerVisibleCached(hiddenLayers, f.layer))
      continue;

    auto bit = m_controller.m_fixtureBounds.find(uuid);
    if (useLabelOptimizations && culling.enabled &&
        bit != m_controller.m_fixtureBounds.end()) {
      ScreenRect rect;
      bool anyDepthVisible = false;
      if (!ProjectBoundingBoxToScreen(bit->second.min, bit->second.max, projection,
                                      rect, anyDepthVisible) ||
          !anyDepthVisible ||
          ShouldCullByScreenRect(rect, projection, minLabelPixels)) {
        continue;
      }
      const double area = std::max(0.0, rect.maxX - rect.minX) *
                          std::max(0.0, rect.maxY - rect.minY);
      candidates.push_back({&uuid, &f, area});
    } else {
      candidates.push_back({&uuid, &f, 0.0});
    }
  }

  if (useLabelOptimizations && maxFixtureLabels > 0 &&
      static_cast<int>(candidates.size()) > maxFixtureLabels) {
    std::partial_sort(candidates.begin(), candidates.begin() + maxFixtureLabels,
                      candidates.end(), [](const auto &a, const auto &b) {
                        return a.area > b.area;
                      });
    candidates.resize(maxFixtureLabels);
  }

  for (const auto &candidate : candidates) {
    const std::string &uuid = *candidate.uuid;
    const Fixture &f = *candidate.fixture;

    auto bit = m_controller.m_fixtureBounds.find(uuid);
    const Viewer3DController::BoundingBox *bounds =
        bit != m_controller.m_fixtureBounds.end() ? &bit->second : nullptr;

    const auto anchor = ResolveAnchor(bounds, f.transform.o, true, view);
    const double wx = anchor[0] + offX;
    const double wy = anchor[1] + offY;
    const double wz = anchor[2] + offZ;

    int x = 0;
    int y = 0;
    if (!ProjectLabelAnchor(projection, wx, wy, wz, x, y))
      continue;

    constexpr const char *kRegularFamily = "sans";
    constexpr const char *kBoldFamily = "sans-bold";
    std::vector<LabelLine2D> lines;
    if (showName) {
      wxString baseName = f.instanceName.empty() ? wxString::FromUTF8(uuid)
                                                 : wxString::FromUTF8(f.instanceName);
      wxString wrapped = WrapEveryTwoWords(baseName);
      wxStringTokenizer nameLines(wrapped, "\n");
      while (nameLines.HasMoreTokens()) {
        wxString line = nameLines.GetNextToken();
        auto utf8 = line.ToUTF8();
        lines.push_back({m_controller.m_font,
                         std::string(utf8.data(), utf8.length()), nameSize,
                         kRegularFamily});
      }
    }
    if (showId) {
      wxString idLine = "ID: " + wxString::Format("%d", f.fixtureId);
      auto utf8 = idLine.ToUTF8();
      lines.push_back({m_controller.m_font, std::string(utf8.data(), utf8.length()),
                       idSize, kRegularFamily});
    }
    if (showDmx && !f.address.empty()) {
      int dmxFont = m_controller.m_fontBold >= 0 ? m_controller.m_fontBold
                                                 : m_controller.m_font;
      wxString addrLine = wxString::FromUTF8(f.address);
      auto utf8 = addrLine.ToUTF8();
      lines.push_back(
          {dmxFont, std::string(utf8.data(), utf8.length()), dmxSize, kBoldFamily});
    }
    if (lines.empty())
      continue;

    if (m_controller.m_captureCanvas) {
      std::string labelSourceKey = "label:" + uuid;
      m_controller.m_captureCanvas->SetSourceKey(labelSourceKey);

      const float pxToWorld = 1.0f / (PIXELS_PER_METER * zoom);
      const float lineSpacingWorld = 2.0f * pxToWorld;

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
        nvgFontSize(m_controller.m_vg, ln.size);
        nvgFontFaceId(m_controller.m_vg, ln.font);
        nvgTextAlign(m_controller.m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        float bounds2D[4];
        nvgTextBounds(m_controller.m_vg, 0.f, 0.f, ln.text.c_str(), nullptr,
                      bounds2D);
        lineHeightsWorld.push_back((bounds2D[3] - bounds2D[1]) * pxToWorld);
        float ascender = 0.0f;
        float descender = 0.0f;
        float lineh = 0.0f;
        nvgTextMetrics(m_controller.m_vg, &ascender, &descender, &lineh);
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
          currentY -= lineHeightsWorld[i] + lineSpacingWorld;
      }
    }

    NVGcolor textColor =
        m_controller.m_darkMode ? nvgRGBAf(1.f, 1.f, 1.f, 1.f)
                                : nvgRGBAf(0.f, 0.f, 0.f, 1.f);
    NVGcolor outlineColor =
        m_controller.m_darkMode ? nvgRGBAf(0.f, 0.f, 0.f, 1.f)
                                : nvgRGBAf(1.f, 1.f, 1.f, 1.f);
    DrawLabelLines2D(m_controller.m_vg, lines, x, y, textColor, outlineColor,
                     true);
  }
}

void LabelRenderSystem::DrawTrussLabels(int width, int height) {
  ConfigManager &cfg = ConfigManager::Get();
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
    if (trussIt == trusses.end() || uuid != m_controller.m_highlightUuid)
      continue;
    if (useLabelOptimizations && maxLabels > 0 && labelsDrawn >= maxLabels)
      break;

    const auto &t = trussIt->second;
    const auto bit = m_controller.m_trussBounds.find(uuid);
    const Viewer3DController::BoundingBox *bounds =
        bit != m_controller.m_trussBounds.end() ? &bit->second : nullptr;

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
    const std::string heightText = FormatMeters(baseHeight);
    label += wxString::Format("\nh = %s m", heightText.c_str());

    auto utf8 = label.ToUTF8();
    DrawText2D(m_controller.m_vg, m_controller.m_font,
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
    if (objectIt == objects.end() || uuid != m_controller.m_highlightUuid)
      continue;
    if (useLabelOptimizations && maxLabels > 0 && labelsDrawn >= maxLabels)
      break;

    const auto &obj = objectIt->second;
    const auto bit = m_controller.m_objectBounds.find(uuid);
    const Viewer3DController::BoundingBox *bounds =
        bit != m_controller.m_objectBounds.end() ? &bit->second : nullptr;

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
    DrawText2D(m_controller.m_vg, m_controller.m_font,
               std::string(utf8.data(), utf8.length()), x, y);
    ++labelsDrawn;
  }
}
