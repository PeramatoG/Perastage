/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
/*
 * File: viewer3dcontroller.cpp
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Implementation of 3D viewer logic.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef DrawText // Avoid conflict with Windows GDI macro of the same name
#endif

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdlib>
#include <numeric>

#include "configmanager.h"
#include "loader3ds.h"
#include "loaderglb.h"
#include "scenedatamanager.h"
#include "viewer3dcontroller.h"
// Include shared Matrix type used throughout models
#include "../models/types.h"
#include "consolepanel.h"
#include "logger.h"

#include <wx/tokenzr.h>
#include <wx/wx.h>
#define NANOVG_GL2_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <cstdint>
#include <string_view>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

// Font size for on-screen labels drawn with NanoVG in the 3D viewer
static constexpr float LABEL_FONT_SIZE_3D = 18.0f;
// Maximum width for on-screen labels before wrapping
static constexpr float LABEL_MAX_WIDTH = 300.0f;
// Width of fixture labels in meters for the 2D view
// Pixels per meter used by the 2D view
static constexpr float PIXELS_PER_METER = 25.0f;

static bool ShouldTraceLabelOrder() {
  static const bool enabled = std::getenv("PERASTAGE_TRACE_LABELS") != nullptr;
  return enabled;
}
static std::string FindFileRecursive(const std::string &baseDir,
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

// Replace Windows path separators with the platform preferred one
static std::string NormalizePath(const std::string &p) {
  std::string out = p;
  char sep = static_cast<char>(fs::path::preferred_separator);
  std::replace(out.begin(), out.end(), '\\', sep);
  return out;
}

static std::string NormalizeModelKey(const std::string &p) {
  if (p.empty())
    return {};
  fs::path path(p);
  path = path.lexically_normal();
  return NormalizePath(path.string());
}

static uint32_t HashString(std::string_view value) {
  uint32_t hash = 2166136261u;
  for (unsigned char c : value) {
    hash ^= c;
    hash *= 16777619u;
  }
  return hash;
}

static std::array<float, 3> HsvToRgb(float h, float s, float v) {
  const float c = v * s;
  const float hh = h * 6.0f;
  const float x = c * (1.0f - std::fabs(std::fmod(hh, 2.0f) - 1.0f));
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;

  if (hh >= 0.0f && hh < 1.0f) {
    r = c;
    g = x;
  } else if (hh >= 1.0f && hh < 2.0f) {
    r = x;
    g = c;
  } else if (hh >= 2.0f && hh < 3.0f) {
    g = c;
    b = x;
  } else if (hh >= 3.0f && hh < 4.0f) {
    g = x;
    b = c;
  } else if (hh >= 4.0f && hh < 5.0f) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }

  const float m = v - c;
  return {r + m, g + m, b + m};
}

static std::array<float, 3> MakeDeterministicColor(std::string_view key) {
  if (key.empty())
    key = "default";
  uint32_t hash = HashString(key);
  float hue = static_cast<float>(hash % 360u) / 360.0f;
  float sat = 0.55f + static_cast<float>((hash >> 8) & 0xFFu) / 255.0f * 0.35f;
  float val = 0.7f + static_cast<float>((hash >> 16) & 0xFFu) / 255.0f * 0.25f;
  return HsvToRgb(hue, sat, val);
}

static std::string ResolveGdtfPath(const std::string &base,
                                   const std::string &spec) {
  std::string norm = NormalizePath(spec);
  fs::path p = base.empty() ? fs::path(norm) : fs::path(base) / norm;
  if (fs::exists(p))
    return p.string();
  return FindFileRecursive(base, fs::path(norm).filename().string());
}

static std::string ResolveModelPath(const std::string &base,
                                    const std::string &file) {
  if (file.empty())
    return {};

  std::string norm = NormalizePath(file);
  fs::path p = base.empty() ? fs::path(norm) : fs::path(base) / norm;
  if (fs::exists(p))
    return p.string();

  if (p.extension().empty()) {
    fs::path p3ds = p;
    p3ds += ".3ds";
    if (fs::exists(p3ds))
      return p3ds.string();

    fs::path pglb = p;
    pglb += ".glb";
    if (fs::exists(pglb))
      return pglb.string();

    std::string fn3ds = FindFileRecursive(base, p.filename().string() + ".3ds");
    if (!fn3ds.empty())
      return fn3ds;
    return FindFileRecursive(base, p.filename().string() + ".glb");
  }

  return FindFileRecursive(base, p.filename().string());
}

static SymbolBounds ComputeSymbolBounds(const CommandBuffer &buffer) {
  SymbolBounds bounds{};
  bool hasPoint = false;

  auto addPoint = [&](float x, float y) {
    if (!hasPoint) {
      bounds.min = {x, y};
      bounds.max = {x, y};
      hasPoint = true;
      return;
    }
    bounds.min.x = std::min(bounds.min.x, x);
    bounds.min.y = std::min(bounds.min.y, y);
    bounds.max.x = std::max(bounds.max.x, x);
    bounds.max.y = std::max(bounds.max.y, y);
  };

  auto addPointWithPadding = [&](float x, float y, float padding) {
    if (padding <= 0.0f) {
      addPoint(x, y);
      return;
    }
    addPoint(x - padding, y - padding);
    addPoint(x + padding, y + padding);
  };

  auto addPoints = [&](const std::vector<float> &points, float padding) {
    for (size_t i = 0; i + 1 < points.size(); i += 2)
      addPointWithPadding(points[i], points[i + 1], padding);
  };

  for (const auto &cmd : buffer.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      float padding = line->stroke.width * 0.5f;
      addPointWithPadding(line->x0, line->y0, padding);
      addPointWithPadding(line->x1, line->y1, padding);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      float padding = polyline->stroke.width * 0.5f;
      addPoints(polyline->points, padding);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      float padding = poly->stroke.width * 0.5f;
      addPoints(poly->points, padding);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      float padding = rect->stroke.width * 0.5f;
      addPoint(rect->x - padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y - padding);
      addPoint(rect->x + rect->w + padding, rect->y + rect->h + padding);
      addPoint(rect->x - padding, rect->y + rect->h + padding);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      float padding = circle->stroke.width * 0.5f;
      float radius = circle->radius + padding;
      addPoint(circle->cx - radius, circle->cy - radius);
      addPoint(circle->cx + radius, circle->cy + radius);
    }
  }

  if (!hasPoint) {
    bounds.min = {};
    bounds.max = {};
  }
  return bounds;
}

static std::string FormatMeters(float mm) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << mm / 1000.0f;
  std::string s = oss.str();
  s.erase(s.find_last_not_of('0') + 1, std::string::npos);
  if (!s.empty() && s.back() == '.')
    s.pop_back();
  return s;
}

static bool HexToRGB(const std::string &hex, float &r, float &g, float &b) {
  if (hex.size() != 7 || hex[0] != '#')
    return false;
  unsigned int value = 0;
  std::istringstream iss(hex.substr(1));
  iss >> std::hex >> value;
  if (iss.fail())
    return false;
  r = ((value >> 16) & 0xFF) / 255.0f;
  g = ((value >> 8) & 0xFF) / 255.0f;
  b = (value & 0xFF) / 255.0f;
  return true;
}

static void MatrixToArray(const Matrix &m, float out[16]) {
  out[0] = m.u[0];
  out[1] = m.u[1];
  out[2] = m.u[2];
  out[3] = 0.0f;
  out[4] = m.v[0];
  out[5] = m.v[1];
  out[6] = m.v[2];
  out[7] = 0.0f;
  out[8] = m.w[0];
  out[9] = m.w[1];
  out[10] = m.w[2];
  out[11] = 0.0f;
  out[12] = m.o[0];
  out[13] = m.o[1];
  out[14] = m.o[2];
  out[15] = 1.0f;
}

struct ScreenRect {
  double minX = DBL_MAX;
  double minY = DBL_MAX;
  double maxX = -DBL_MAX;
  double maxY = -DBL_MAX;
};

// Inserts a line break every two words in the provided text.
static wxString WrapEveryTwoWords(const wxString &text) {
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

// Draws a text string at screen coordinates using NanoVG. The font size and
// maximum width are specified in pixels.
static void DrawText2D(NVGcontext *vg, int font, const std::string &text, int x,
                       int y, float fontSize = LABEL_FONT_SIZE_3D,
                       float maxWidth = LABEL_MAX_WIDTH,
                       bool drawBackground = true, bool drawBorder = true,
                       NVGcolor textColor = nvgRGBAf(1.f, 1.f, 1.f, 1.f)) {
  if (!vg || font < 0 || text.empty())
    return;

  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);

  nvgBeginFrame(vg, vp[2], vp[3], 1.0f);
  nvgSave(vg);
  nvgFontSize(vg, fontSize);
  nvgFontFaceId(vg, font);
  // Center text for multiline labels
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  // Determine the width based on the actual text content so the background
  // box tightly fits the rendered label. We measure each line separately to
  // avoid the fixed width imposed by nvgTextBoxBounds and clamp to a maximum
  // so long names wrap instead of growing indefinitely.

  float textWidth = 0.0f;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    std::string line = text.substr(start, end - start);
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

  // Calculate the exact bounding box for the text using the same alignment
  // and width that will be used when rendering it. This ensures the
  // background rectangle matches the visual position of the text.
  float bounds[4];
  nvgTextBoxBounds(vg, (float)x, (float)y, textWidth, text.c_str(), nullptr,
                   bounds);

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
  // Draw multi-line label using textWidth to avoid excessive empty space.
  nvgTextBox(vg, (float)x, (float)y, textWidth, text.c_str(), nullptr);
  nvgRestore(vg);
  nvgEndFrame(vg);
}

struct LabelLine2D {
  int font;
  std::string text;
  float size;
};

static void
DrawLabelLines2D(NVGcontext *vg, const std::vector<LabelLine2D> &lines, int x,
                 int y, NVGcolor textColor = nvgRGBAf(1.f, 1.f, 1.f, 1.f),
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
    nvgText(vg, (float)x, currentY, lines[i].text.c_str(), nullptr);
    currentY += heights[i] + lineSpacing;
  }

  nvgRestore(vg);
  nvgEndFrame(vg);
}

static std::array<float, 3> TransformPoint(const Matrix &m,
                                           const std::array<float, 3> &p) {
  return {m.u[0] * p[0] + m.v[0] * p[1] + m.w[0] * p[2] + m.o[0],
          m.u[1] * p[0] + m.v[1] * p[1] + m.w[1] * p[2] + m.o[1],
          m.u[2] * p[0] + m.v[2] * p[1] + m.w[2] * p[2] + m.o[2]};
}

static Transform2D BuildInstanceTransform2D(const Matrix &m, Viewer2DView view) {
  Transform2D t{};
  switch (view) {
  case Viewer2DView::Top:
  case Viewer2DView::Bottom:
    t.a = m.u[0];
    t.b = m.u[1];
    t.c = m.v[0];
    t.d = m.v[1];
    t.tx = m.o[0];
    t.ty = m.o[1];
    break;
  case Viewer2DView::Front:
    t.a = m.u[0];
    t.b = m.u[2];
    t.c = m.w[0];
    t.d = m.w[2];
    t.tx = m.o[0];
    t.ty = m.o[2];
    break;
  case Viewer2DView::Side:
    t.a = -m.v[1];
    t.b = m.v[2];
    t.c = -m.w[1];
    t.d = m.w[2];
    t.tx = -m.o[1];
    t.ty = m.o[2];
    break;
  }
  return t;
}

// Maps a 3D point expressed in meters to the 2D canvas coordinates used by the
// simplified viewer. The mapping mirrors the orthographic camera setup in
// Render() so exporters can rebuild the same projection.
std::array<float, 2>
Viewer3DController::ProjectToCanvas(const std::array<float, 3> &p) const {
  switch (m_captureView) {
  case Viewer2DView::Top:
    return {p[0], p[1]};
  case Viewer2DView::Bottom:
    return {p[0], p[1]};
  case Viewer2DView::Front:
    return {p[0], p[2]};
  case Viewer2DView::Side:
    // The side camera looks towards +X with +Z as the up vector, which makes
    // the screen X axis point towards -Y in world space. Mirror the Y
    // coordinate accordingly so the recorded commands match the on-screen
    // orientation used by the 2D viewer.
    return {-p[1], p[2]};
  }
  return {p[0], p[1]};
}

void Viewer3DController::RecordLine(const std::array<float, 3> &a,
                                    const std::array<float, 3> &b,
                                    const CanvasStroke &stroke) const {
  if (!m_captureCanvas)
    return;
  auto p0 = ProjectToCanvas(a);
  auto p1 = ProjectToCanvas(b);
  m_captureCanvas->DrawLine(p0[0], p0[1], p1[0], p1[1], stroke);
}

void Viewer3DController::RecordPolyline(
    const std::vector<std::array<float, 3>> &points,
    const CanvasStroke &stroke) const {
  if (!m_captureCanvas || points.size() < 2)
    return;
  std::vector<float> flat;
  flat.reserve(points.size() * 2);
  for (const auto &p : points) {
    auto q = ProjectToCanvas(p);
    flat.push_back(q[0]);
    flat.push_back(q[1]);
  }
  m_captureCanvas->DrawPolyline(flat, stroke);
}

void Viewer3DController::RecordPolygon(
    const std::vector<std::array<float, 3>> &points,
    const CanvasStroke &stroke, const CanvasFill *fill) const {
  if (!m_captureCanvas || points.size() < 3)
    return;
  std::vector<float> flat;
  flat.reserve(points.size() * 2);
  for (const auto &p : points) {
    auto q = ProjectToCanvas(p);
    flat.push_back(q[0]);
    flat.push_back(q[1]);
  }
  m_captureCanvas->DrawPolygon(flat, stroke, fill);
}

void Viewer3DController::RecordText(float x, float y, const std::string &text,
                                    const CanvasTextStyle &style) const {
  if (!m_captureCanvas)
    return;
  m_captureCanvas->DrawText(x, y, text, style);
}

Viewer3DController::Viewer3DController() {
  // Actual initialization of OpenGL-dependent resources is delayed
  // until a valid context is available.
}

Viewer3DController::~Viewer3DController() {
  if (m_vg)
    nvgDeleteGL2(m_vg);
}

void Viewer3DController::InitializeGL() {
  if (m_vg)
    return; // Already initialized

  m_vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  if (m_vg) {
    const char *fontPaths[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf",
#endif
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", nullptr};

    for (const char **p = fontPaths; *p; ++p) {
      if (fs::exists(*p)) {
        m_font = nvgCreateFont(m_vg, "sans", *p);
        if (m_font >= 0)
          break;
      }
    }
    if (m_font < 0)
      Logger::Instance().Log("Failed to load font for labels");
  } else {
    Logger::Instance().Log("Failed to create NanoVG context");
  }
}

void Viewer3DController::SetHighlightUuid(const std::string &uuid) {
  m_highlightUuid = uuid;
}

void Viewer3DController::SetSelectedUuids(
    const std::vector<std::string> &uuids) {
  m_selectedUuids.clear();
  for (const auto &u : uuids)
    m_selectedUuids.insert(u);
}

// Loads meshes or GDTF models referenced by scene objects. Called when the
// scene is updated.
void Viewer3DController::Update() {
  const std::string &base = ConfigManager::Get().GetScene().basePath;

  if (m_lastSceneBasePath != base) {
    m_loadedGdtf.clear();
    m_failedGdtfReasons.clear();
    m_reportedGdtfFailureCounts.clear();
    m_reportedGdtfFailureReasons.clear();
    m_lastSceneBasePath = base;
  }

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  for (const auto &[uuid, t] : trusses) {
    if (t.symbolFile.empty())
      continue;

    std::string path = ResolveModelPath(base, t.symbolFile);
    if (path.empty())
      continue;
    if (m_loadedMeshes.find(path) == m_loadedMeshes.end()) {
      Mesh mesh;
      bool loaded = false;
      std::string ext = fs::path(path).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (ext == ".3ds")
        loaded = Load3DS(path, mesh);
      else if (ext == ".glb")
        loaded = LoadGLB(path, mesh);

      if (loaded) {
        m_loadedMeshes[path] = std::move(mesh);
      } else if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Failed to load model: %s",
                                        wxString::FromUTF8(path));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
    }
  }

  const auto &objects = SceneDataManager::Instance().GetSceneObjects();
  for (const auto &[uuid, obj] : objects) {
    if (obj.modelFile.empty())
      continue;
    std::string path = ResolveModelPath(base, obj.modelFile);
    if (path.empty())
      continue;
    if (m_loadedMeshes.find(path) == m_loadedMeshes.end()) {
      Mesh mesh;
      bool loaded = false;
      std::string ext = fs::path(path).extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (ext == ".3ds")
        loaded = Load3DS(path, mesh);
      else if (ext == ".glb")
        loaded = LoadGLB(path, mesh);

      if (loaded) {
        m_loadedMeshes[path] = std::move(mesh);
      } else if (ConsolePanel::Instance()) {
        wxString msg = wxString::Format("Failed to load model: %s",
                                        wxString::FromUTF8(path));
        ConsolePanel::Instance()->AppendMessage(msg);
      }
    }
  }

  std::unordered_map<std::string, size_t> gdtfErrorCounts;
  std::unordered_map<std::string, std::string> gdtfErrorReasons;
  std::unordered_set<std::string> processedGdtfPaths;
  std::unordered_set<std::string> missingGdtfSpecs;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  for (const auto &[uuid, f] : fixtures) {
    if (f.gdtfSpec.empty())
      continue;
    std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
    if (gdtfPath.empty()) {
      ++gdtfErrorCounts[f.gdtfSpec];
      gdtfErrorReasons[f.gdtfSpec] = "GDTF file not found";
      if (!missingGdtfSpecs.insert(f.gdtfSpec).second)
        continue;
      continue;
    }
    auto failedIt = m_failedGdtfReasons.find(gdtfPath);
    if (failedIt != m_failedGdtfReasons.end()) {
      ++gdtfErrorCounts[gdtfPath];
      gdtfErrorReasons[gdtfPath] = failedIt->second;
      continue;
    }
    if (processedGdtfPaths.find(gdtfPath) != processedGdtfPaths.end()) {
      auto reasonIt = m_failedGdtfReasons.find(gdtfPath);
      if (reasonIt != m_failedGdtfReasons.end()) {
        ++gdtfErrorCounts[gdtfPath];
        gdtfErrorReasons[gdtfPath] = reasonIt->second;
      }
      continue;
    }
    if (m_loadedGdtf.find(gdtfPath) == m_loadedGdtf.end()) {
      std::vector<GdtfObject> objs;
      std::string gdtfError;
      if (LoadGdtf(gdtfPath, objs, &gdtfError)) {
        m_loadedGdtf[gdtfPath] = std::move(objs);
      } else {
        std::string reason = gdtfError.empty() ? "Failed to load GDTF"
                                               : gdtfError;
        m_failedGdtfReasons[gdtfPath] = std::move(reason);
        ++gdtfErrorCounts[gdtfPath];
        gdtfErrorReasons[gdtfPath] = m_failedGdtfReasons[gdtfPath];
      }
    }
    processedGdtfPaths.insert(gdtfPath);
  }

  if (!gdtfErrorCounts.empty() && ConsolePanel::Instance()) {
    for (const auto &[path, count] : gdtfErrorCounts) {
      auto reasonIt = gdtfErrorReasons.find(path);
      const std::string &reasonStr =
          reasonIt != gdtfErrorReasons.end() ? reasonIt->second : "Unknown";

      auto prevCountIt = m_reportedGdtfFailureCounts.find(path);
      auto prevReasonIt = m_reportedGdtfFailureReasons.find(path);
      bool sameCount = prevCountIt != m_reportedGdtfFailureCounts.end() &&
                       prevCountIt->second == count;
      bool sameReason = prevReasonIt != m_reportedGdtfFailureReasons.end() &&
                        prevReasonIt->second == reasonStr;
      if (sameCount && sameReason)
        continue;

      wxString reason = wxString::FromUTF8(reasonStr);
      wxString gdtfPath = wxString::FromUTF8(path);
      wxString msg;
      if (count > 1) {
        msg = wxString::Format("Failed to load GDTF %s (%zu fixtures): %s",
                               gdtfPath, count, reason);
      } else {
        msg = wxString::Format("Failed to load GDTF %s: %s", gdtfPath, reason);
      }
      ConsolePanel::Instance()->AppendMessage(msg);
      m_reportedGdtfFailureCounts[path] = count;
      m_reportedGdtfFailureReasons[path] = reasonStr;
    }
  }

  // Precompute bounding boxes for hover detection
  m_fixtureBounds.clear();
  for (const auto &[uuid, f] : fixtures) {
    Viewer3DController::BoundingBox bb;
    Matrix fix = f.transform;
    fix.o[0] *= RENDER_SCALE;
    fix.o[1] *= RENDER_SCALE;
    fix.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
    auto itg = m_loadedGdtf.find(gdtfPath);
    if (itg != m_loadedGdtf.end()) {
      for (const auto &obj : itg->second) {
        for (size_t vi = 0; vi + 2 < obj.mesh.vertices.size(); vi += 3) {
          std::array<float, 3> p = {obj.mesh.vertices[vi] * RENDER_SCALE,
                                    obj.mesh.vertices[vi + 1] * RENDER_SCALE,
                                    obj.mesh.vertices[vi + 2] * RENDER_SCALE};
          p = TransformPoint(obj.transform, p);
          p = TransformPoint(fix, p);
          bb.min[0] = std::min(bb.min[0], p[0]);
          bb.min[1] = std::min(bb.min[1], p[1]);
          bb.min[2] = std::min(bb.min[2], p[2]);
          bb.max[0] = std::max(bb.max[0], p[0]);
          bb.max[1] = std::max(bb.max[1], p[1]);
          bb.max[2] = std::max(bb.max[2], p[2]);
          found = true;
        }
      }
    }

    if (!found) {
      float half = 0.1f;
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{-half, -half, -half},
          {half, -half, -half},
          {-half, half, -half},
          {half, half, -half},
          {-half, -half, half},
          {half, -half, half},
          {-half, half, half},
          {half, half, half}};
      for (const auto &c : corners) {
        auto p = TransformPoint(fix, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }

    m_fixtureBounds[uuid] = bb;
  }

  m_trussBounds.clear();
  for (const auto &[uuid, t] : trusses) {
    BoundingBox bb;
    Matrix tm = t.transform;
    tm.o[0] *= RENDER_SCALE;
    tm.o[1] *= RENDER_SCALE;
    tm.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!t.symbolFile.empty()) {
      std::string path = ResolveModelPath(base, t.symbolFile);
      auto it = m_loadedMeshes.find(path);
      if (it != m_loadedMeshes.end()) {
        for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
          std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                    it->second.vertices[vi + 1] * RENDER_SCALE,
                                    it->second.vertices[vi + 2] * RENDER_SCALE};
          p = TransformPoint(tm, p);
          bb.min[0] = std::min(bb.min[0], p[0]);
          bb.min[1] = std::min(bb.min[1], p[1]);
          bb.min[2] = std::min(bb.min[2], p[2]);
          bb.max[0] = std::max(bb.max[0], p[0]);
          bb.max[1] = std::max(bb.max[1], p[1]);
          bb.max[2] = std::max(bb.max[2], p[2]);
          found = true;
        }
      }
    }

    if (!found) {
      float len = (t.lengthMm > 0 ? t.lengthMm * RENDER_SCALE : 0.3f);
      float halfy = (t.widthMm > 0 ? t.widthMm * RENDER_SCALE * 0.5f : 0.15f);
      float z1 = (t.heightMm > 0 ? t.heightMm * RENDER_SCALE : 0.3f);
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{0.0f, -halfy, 0.0f},
          {len, -halfy, 0.0f},
          {0.0f, halfy, 0.0f},
          {len, halfy, 0.0f},
          {0.0f, -halfy, z1},
          {len, -halfy, z1},
          {0.0f, halfy, z1},
          {len, halfy, z1}};
      for (const auto &c : corners) {
        auto p = TransformPoint(tm, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }

    m_trussBounds[uuid] = bb;
  }

  m_objectBounds.clear();
  for (const auto &[uuid, obj] : objects) {
    BoundingBox bb;
    Matrix tm = obj.transform;
    tm.o[0] *= RENDER_SCALE;
    tm.o[1] *= RENDER_SCALE;
    tm.o[2] *= RENDER_SCALE;

    bool found = false;
    bb.min = {FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    if (!obj.modelFile.empty()) {
      std::string path = ResolveModelPath(base, obj.modelFile);
      auto it = m_loadedMeshes.find(path);
      if (it != m_loadedMeshes.end()) {
        for (size_t vi = 0; vi + 2 < it->second.vertices.size(); vi += 3) {
          std::array<float, 3> p = {it->second.vertices[vi] * RENDER_SCALE,
                                    it->second.vertices[vi + 1] * RENDER_SCALE,
                                    it->second.vertices[vi + 2] * RENDER_SCALE};
          p = TransformPoint(tm, p);
          bb.min[0] = std::min(bb.min[0], p[0]);
          bb.min[1] = std::min(bb.min[1], p[1]);
          bb.min[2] = std::min(bb.min[2], p[2]);
          bb.max[0] = std::max(bb.max[0], p[0]);
          bb.max[1] = std::max(bb.max[1], p[1]);
          bb.max[2] = std::max(bb.max[2], p[2]);
          found = true;
        }
      }
    }

    if (!found) {
      float half = 0.15f;
      std::array<std::array<float, 3>, 8> corners = {
          std::array<float, 3>{-half, -half, -half},
          {half, -half, -half},
          {-half, half, -half},
          {half, half, -half},
          {-half, -half, half},
          {half, -half, half},
          {-half, half, half},
          {half, half, half}};
      for (const auto &c : corners) {
        auto p = TransformPoint(tm, c);
        bb.min[0] = std::min(bb.min[0], p[0]);
        bb.min[1] = std::min(bb.min[1], p[1]);
        bb.min[2] = std::min(bb.min[2], p[2]);
        bb.max[0] = std::max(bb.max[0], p[0]);
        bb.max[1] = std::max(bb.max[1], p[1]);
        bb.max[2] = std::max(bb.max[2], p[2]);
      }
    }

    m_objectBounds[uuid] = bb;
  }
}

// Renders all scene objects using their transformMatrix
void Viewer3DController::RenderScene(bool wireframe, Viewer2DRenderMode mode,
                                     Viewer2DView view, bool showGrid,
                                     int gridStyle, float gridR, float gridG,
                                     float gridB, bool gridOnTop) {
  if (wireframe)
    glDisable(GL_LIGHTING);
  else
    SetupBasicLighting();
  auto getTypeColor = [&](const std::string &key, const std::string &hex) {
    std::array<float, 3> c;
    // Always honor user-specified colors, updating the cache if needed
    if (!hex.empty() && HexToRGB(hex, c[0], c[1], c[2])) {
      m_typeColors[key] = c;
      return c;
    }
    c = MakeDeterministicColor("type:" + key);
    m_typeColors[key] = c;
    return c;
  };
  auto getLayerColor = [&](const std::string &key) {
    std::array<float, 3> c;
    auto opt = ConfigManager::Get().GetLayerColor(key);
    if (opt && HexToRGB(*opt, c[0], c[1], c[2])) {
      m_layerColors[key] = c;
      return c;
    }
    c = MakeDeterministicColor("layer:" + key);
    m_layerColors[key] = c;
    return c;
  };
  if (showGrid && !gridOnTop)
    DrawGrid(gridStyle, gridR, gridG, gridB, view);
  const std::string &base = ConfigManager::Get().GetScene().basePath;
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

  // Scene objects first
  glShadeModel(GL_FLAT);
  const auto &sceneObjects = SceneDataManager::Instance().GetSceneObjects();
  std::vector<const std::pair<const std::string, SceneObject> *> sortedObjs;
  sortedObjs.reserve(sceneObjects.size());
  for (const auto &obj : sceneObjects)
    sortedObjs.push_back(&obj);
  std::sort(sortedObjs.begin(), sortedObjs.end(),
            [](const auto *a, const auto *b) {
              return a->second.transform.o[2] < b->second.transform.o[2];
            });
  for (const auto *entry : sortedObjs) {
    const auto &uuid = entry->first;
    const auto &m = entry->second;
    if (!ConfigManager::Get().IsLayerVisible(m.layer))
      continue;
    glPushMatrix();

    std::string objectCaptureKey;
    if (m_captureCanvas) {
      objectCaptureKey = m.modelFile.empty() ? m.name : m.modelFile;
      if (objectCaptureKey.empty())
        objectCaptureKey = "scene_object";
      m_captureCanvas->SetSourceKey(objectCaptureKey);
    }

    bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
    bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(m.transform, matrix);
    ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto obit = m_objectBounds.find(uuid);
    if (obit != m_objectBounds.end()) {
      cx = (obit->second.min[0] + obit->second.max[0]) * 0.5f;
      cy = (obit->second.min[1] + obit->second.max[1]) * 0.5f;
      cz = (obit->second.min[2] + obit->second.max[2]) * 0.5f;
      cx -= m.transform.o[0] * RENDER_SCALE;
      cy -= m.transform.o[1] * RENDER_SCALE;
      cz -= m.transform.o[2] * RENDER_SCALE;
    }

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (wireframe && mode == Viewer2DRenderMode::ByLayer) {
      auto c = getLayerColor(m.layer);
      r = c[0];
      g = c[1];
      b = c[2];
    }

    Matrix captureTransform = m.transform;
    captureTransform.o[0] *= RENDER_SCALE;
    captureTransform.o[1] *= RENDER_SCALE;
    captureTransform.o[2] *= RENDER_SCALE;
    auto applyCapture = [captureTransform](const std::array<float, 3> &p) {
      return TransformPoint(captureTransform, p);
    };

    const Mesh *objectMesh = nullptr;
    std::string objectPath;
    if (!m.modelFile.empty()) {
      objectPath = ResolveModelPath(base, m.modelFile);
      if (!objectPath.empty()) {
        auto it = m_loadedMeshes.find(objectPath);
        if (it != m_loadedMeshes.end())
          objectMesh = &it->second;
      }
    }

    auto drawSceneObjectGeometry =
        [&](const std::function<std::array<float, 3>(
                const std::array<float, 3> &)> &captureTransform,
            bool isHighlighted, bool isSelected) {
          if (objectMesh) {
            DrawMeshWithOutline(*objectMesh, r, g, b, RENDER_SCALE,
                                isHighlighted, isSelected, cx, cy, cz,
                                wireframe, mode, captureTransform);
          } else {
            DrawCubeWithOutline(0.3f, r, g, b, isHighlighted, isSelected, cx,
                                cy, cz, wireframe, mode, captureTransform);
          }
        };

    bool suppressCapture = false;
    const bool useSymbolInstancing =
        (m_captureUseSymbols &&
         (m_captureView == Viewer2DView::Bottom ||
          m_captureView == Viewer2DView::Top ||
          m_captureView == Viewer2DView::Front ||
          m_captureView == Viewer2DView::Side) &&
         !highlight && !selected);
    bool placedInstance = false;
    if (useSymbolInstancing && m_captureCanvas) {
      std::string modelKey;
      if (!objectPath.empty())
        modelKey = NormalizeModelKey(objectPath);
      else if (!m.modelFile.empty())
        modelKey = NormalizeModelKey(m.modelFile);
      if (modelKey.empty() && !m.name.empty())
        modelKey = m.name;

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = "object:" + modelKey;
        symbolKey.viewKind = resolveSymbolView(m_captureView);
        symbolKey.styleVersion = 1;

        const auto &symbol =
            m_bottomSymbolCache.GetOrCreate(symbolKey, [&](const SymbolKey &,
                                                           uint32_t symbolId) {
              SymbolDefinition definition{};
              definition.symbolId = symbolId;
              auto localCanvas =
                  CreateRecordingCanvas(definition.localCommands, false);
              CanvasTransform transform{};
              localCanvas->BeginFrame();
              localCanvas->SetTransform(transform);

              ICanvas2D *prevCanvas = m_captureCanvas;
              Viewer2DView prevView = m_captureView;
              bool prevCaptureOnly = m_captureOnly;
              bool prevIncludeGrid = m_captureIncludeGrid;
              m_captureCanvas = localCanvas.get();
              m_captureView = prevView;
              m_captureOnly = true;
              m_captureIncludeGrid = false;

              m_captureCanvas->SetSourceKey(
                  objectCaptureKey.empty() ? "scene_object" : objectCaptureKey);
              drawSceneObjectGeometry({}, false, false);
              localCanvas->EndFrame();
              definition.bounds =
                  ComputeSymbolBounds(definition.localCommands);

              m_captureCanvas = prevCanvas;
              m_captureView = prevView;
              m_captureOnly = prevCaptureOnly;
              m_captureIncludeGrid = prevIncludeGrid;
              return definition;
            });

        Transform2D instanceTransform =
            BuildInstanceTransform2D(captureTransform, m_captureView);
        m_captureCanvas->PlaceSymbolInstance(symbol.symbolId, instanceTransform);
        placedInstance = true;
      }
    }
    suppressCapture = placedInstance;

    if (suppressCapture) {
      ICanvas2D *prevCanvas = m_captureCanvas;
      bool prevCaptureOnly = m_captureOnly;
      m_captureCanvas = nullptr;
      m_captureOnly = false;
      drawSceneObjectGeometry(applyCapture, highlight, selected);
      m_captureCanvas = prevCanvas;
      m_captureOnly = prevCaptureOnly;
    } else {
      drawSceneObjectGeometry(applyCapture, highlight, selected);
    }

    glPopMatrix();
  }

  // Trusses next
  glShadeModel(GL_SMOOTH); // keep smooth shading for trusses
  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  std::vector<const std::pair<const std::string, Truss> *> sortedTrusses;
  sortedTrusses.reserve(trusses.size());
  for (const auto &t : trusses)
    sortedTrusses.push_back(&t);
  std::sort(sortedTrusses.begin(), sortedTrusses.end(),
            [](const auto *a, const auto *b) {
              return a->second.transform.o[2] < b->second.transform.o[2];
            });
  for (const auto *entry : sortedTrusses) {
    const auto &uuid = entry->first;
    const auto &t = entry->second;
    if (!ConfigManager::Get().IsLayerVisible(t.layer))
      continue;
    glPushMatrix();

    std::string trussCaptureKey;
    if (m_captureCanvas) {
      trussCaptureKey = t.model.empty() ? t.name : t.model;
      if (trussCaptureKey.empty())
        trussCaptureKey = "truss";
      m_captureCanvas->SetSourceKey(trussCaptureKey);
    }

    bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
    bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(t.transform, matrix);
    ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto tbit = m_trussBounds.find(uuid);
    if (tbit != m_trussBounds.end()) {
      cx = (tbit->second.min[0] + tbit->second.max[0]) * 0.5f;
      cy = (tbit->second.min[1] + tbit->second.max[1]) * 0.5f;
      cz = (tbit->second.min[2] + tbit->second.max[2]) * 0.5f;
      cx -= t.transform.o[0] * RENDER_SCALE;
      cy -= t.transform.o[1] * RENDER_SCALE;
      cz -= t.transform.o[2] * RENDER_SCALE;
    }

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (wireframe && mode == Viewer2DRenderMode::ByLayer) {
      auto c = getLayerColor(t.layer);
      r = c[0];
      g = c[1];
      b = c[2];
    }

    Matrix captureTransform = t.transform;
    captureTransform.o[0] *= RENDER_SCALE;
    captureTransform.o[1] *= RENDER_SCALE;
    captureTransform.o[2] *= RENDER_SCALE;
    auto applyCapture = [captureTransform](const std::array<float, 3> &p) {
      return TransformPoint(captureTransform, p);
    };

    const Mesh *trussMesh = nullptr;
    std::string trussPath;
    if (!t.symbolFile.empty()) {
      trussPath = ResolveModelPath(base, t.symbolFile);
      if (!trussPath.empty()) {
        auto it = m_loadedMeshes.find(trussPath);
        if (it != m_loadedMeshes.end())
          trussMesh = &it->second;
      }
    }

    float trussLen = t.lengthMm * RENDER_SCALE;
    float trussWid = (t.widthMm > 0 ? t.widthMm : 400.0f) * RENDER_SCALE;
    float trussHei = (t.heightMm > 0 ? t.heightMm : 400.0f) * RENDER_SCALE;
    float trussWidthMm = (t.widthMm > 0 ? t.widthMm : 400.0f);
    float trussHeightMm = (t.heightMm > 0 ? t.heightMm : 400.0f);

    auto drawTrussGeometry =
        [&](const std::function<std::array<float, 3>(
                const std::array<float, 3> &)> &captureTransform,
            bool isHighlighted, bool isSelected) {
          if (trussMesh) {
            DrawMeshWithOutline(*trussMesh, r, g, b, RENDER_SCALE,
                                isHighlighted, isSelected, cx, cy, cz,
                                wireframe, mode, captureTransform);
          } else {
            DrawWireframeBox(trussLen, trussHei, trussWid, isHighlighted,
                             isSelected, wireframe, mode, captureTransform);
          }
        };

    bool suppressCapture = false;
    const bool useSymbolInstancing =
        (m_captureUseSymbols &&
         (m_captureView == Viewer2DView::Bottom ||
          m_captureView == Viewer2DView::Top ||
          m_captureView == Viewer2DView::Front ||
          m_captureView == Viewer2DView::Side) &&
         !highlight && !selected);
    bool placedInstance = false;
    if (useSymbolInstancing && m_captureCanvas) {
      std::string modelKey;
      if (!trussPath.empty())
        modelKey = NormalizeModelKey(trussPath);
      else if (!t.symbolFile.empty())
        modelKey = NormalizeModelKey(t.symbolFile);
      if (modelKey.empty() && !trussMesh) {
        std::ostringstream boxKey;
        boxKey << "box:" << t.lengthMm << "x" << trussWidthMm << "x"
               << trussHeightMm;
        modelKey = boxKey.str();
      }
      if (modelKey.empty() && !t.model.empty())
        modelKey = t.model;
      if (modelKey.empty() && !t.name.empty())
        modelKey = t.name;

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = "truss:" + modelKey;
        symbolKey.viewKind = resolveSymbolView(m_captureView);
        symbolKey.styleVersion = 1;

        const auto &symbol =
            m_bottomSymbolCache.GetOrCreate(symbolKey, [&](const SymbolKey &,
                                                           uint32_t symbolId) {
              SymbolDefinition definition{};
              definition.symbolId = symbolId;
              auto localCanvas =
                  CreateRecordingCanvas(definition.localCommands, false);
              CanvasTransform transform{};
              localCanvas->BeginFrame();
              localCanvas->SetTransform(transform);

              ICanvas2D *prevCanvas = m_captureCanvas;
              Viewer2DView prevView = m_captureView;
              bool prevCaptureOnly = m_captureOnly;
              bool prevIncludeGrid = m_captureIncludeGrid;
              m_captureCanvas = localCanvas.get();
              m_captureView = prevView;
              m_captureOnly = true;
              m_captureIncludeGrid = false;

              m_captureCanvas->SetSourceKey(
                  trussCaptureKey.empty() ? "truss" : trussCaptureKey);
              drawTrussGeometry({}, false, false);
              localCanvas->EndFrame();
              definition.bounds =
                  ComputeSymbolBounds(definition.localCommands);

              m_captureCanvas = prevCanvas;
              m_captureView = prevView;
              m_captureOnly = prevCaptureOnly;
              m_captureIncludeGrid = prevIncludeGrid;
              return definition;
            });

        Transform2D instanceTransform =
            BuildInstanceTransform2D(captureTransform, m_captureView);
        m_captureCanvas->PlaceSymbolInstance(symbol.symbolId, instanceTransform);
        placedInstance = true;
      }
    }
    suppressCapture = placedInstance;

    if (suppressCapture) {
      ICanvas2D *prevCanvas = m_captureCanvas;
      bool prevCaptureOnly = m_captureOnly;
      m_captureCanvas = nullptr;
      m_captureOnly = false;
      drawTrussGeometry(applyCapture, highlight, selected);
      m_captureCanvas = prevCanvas;
      m_captureOnly = prevCaptureOnly;
    } else {
      drawTrussGeometry(applyCapture, highlight, selected);
    }

    glPopMatrix();
  }

  // Fixtures last
  glShadeModel(GL_FLAT);
  const bool forceFixturesOnTop = wireframe;
  GLboolean depthEnabled = GL_FALSE;
  if (forceFixturesOnTop) {
    depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled)
      glDisable(GL_DEPTH_TEST);
  }
  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  std::vector<const std::pair<const std::string, Fixture> *> sortedFixtures;
  sortedFixtures.reserve(fixtures.size());
  for (const auto &f : fixtures)
    sortedFixtures.push_back(&f);
  std::sort(sortedFixtures.begin(), sortedFixtures.end(),
            [](const auto *a, const auto *b) {
              return a->second.transform.o[2] < b->second.transform.o[2];
            });
  for (const auto *entry : sortedFixtures) {
    const auto &uuid = entry->first;
    const auto &f = entry->second;
    if (!ConfigManager::Get().IsLayerVisible(f.layer))
      continue;
    glPushMatrix();

    std::string fixtureCaptureKey;
    if (m_captureCanvas) {
      fixtureCaptureKey = !f.typeName.empty()
                              ? f.typeName
                              : (!f.gdtfSpec.empty() ? f.gdtfSpec : "unknown");
      // Default to the whole fixture key; individual GDTF parts will override
      // this key below so outlines remain visible in captured PDFs.
      m_captureCanvas->SetSourceKey(fixtureCaptureKey);
    }

    bool highlight = (!m_highlightUuid.empty() && uuid == m_highlightUuid);
    bool selected = (m_selectedUuids.find(uuid) != m_selectedUuids.end());

    float matrix[16];
    MatrixToArray(f.transform, matrix);
    ApplyTransform(matrix, true);

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    auto fbit = m_fixtureBounds.find(uuid);
    if (fbit != m_fixtureBounds.end()) {
      cx = (fbit->second.min[0] + fbit->second.max[0]) * 0.5f;
      cy = (fbit->second.min[1] + fbit->second.max[1]) * 0.5f;
      cz = (fbit->second.min[2] + fbit->second.max[2]) * 0.5f;
      cx -= f.transform.o[0] * RENDER_SCALE;
      cy -= f.transform.o[1] * RENDER_SCALE;
      cz -= f.transform.o[2] * RENDER_SCALE;
    }

    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (wireframe) {
      if (mode == Viewer2DRenderMode::ByFixtureType) {
        auto c = getTypeColor(f.gdtfSpec, f.color);
        r = c[0];
        g = c[1];
        b = c[2];
      } else if (mode == Viewer2DRenderMode::ByLayer) {
        auto c = getLayerColor(f.layer);
        r = c[0];
        g = c[1];
        b = c[2];
      }
    }

    Matrix fixtureTransform = f.transform;
    fixtureTransform.o[0] *= RENDER_SCALE;
    fixtureTransform.o[1] *= RENDER_SCALE;
    fixtureTransform.o[2] *= RENDER_SCALE;

    auto applyFixtureCapture = [fixtureTransform](
                                   const std::array<float, 3> &p) {
      return TransformPoint(fixtureTransform, p);
    };

    std::string gdtfPath = ResolveGdtfPath(base, f.gdtfSpec);
    auto itg = m_loadedGdtf.find(gdtfPath);

    bool suppressCapture = false;
    const bool useSymbolInstancing =
        (m_captureUseSymbols &&
         (m_captureView == Viewer2DView::Bottom ||
          m_captureView == Viewer2DView::Top ||
          m_captureView == Viewer2DView::Front ||
          m_captureView == Viewer2DView::Side) &&
         !highlight && !selected);
    bool placedInstance = false;
    if (useSymbolInstancing && m_captureCanvas) {
      std::string modelKey = NormalizeModelKey(gdtfPath);
      if (modelKey.empty() && !f.gdtfSpec.empty())
        modelKey = NormalizeModelKey(f.gdtfSpec);
      if (modelKey.empty() && !f.typeName.empty())
        modelKey = f.typeName;

      if (!modelKey.empty()) {
        SymbolKey symbolKey;
        symbolKey.modelKey = modelKey;
        symbolKey.viewKind = resolveSymbolView(m_captureView);
        symbolKey.styleVersion = 1;

        const auto &symbol =
            m_bottomSymbolCache.GetOrCreate(symbolKey, [&](const SymbolKey &,
                                                         uint32_t symbolId) {
              SymbolDefinition definition{};
              definition.symbolId = symbolId;
              auto localCanvas =
                  CreateRecordingCanvas(definition.localCommands, false);
              CanvasTransform transform{};
              localCanvas->BeginFrame();
              localCanvas->SetTransform(transform);

              ICanvas2D *prevCanvas = m_captureCanvas;
              Viewer2DView prevView = m_captureView;
              bool prevCaptureOnly = m_captureOnly;
              bool prevIncludeGrid = m_captureIncludeGrid;
              m_captureCanvas = localCanvas.get();
              m_captureView = prevView;
              m_captureOnly = true;
              m_captureIncludeGrid = false;

              if (itg != m_loadedGdtf.end()) {
                size_t partIndex = 0;
                for (const auto &obj : itg->second) {
                  m_captureCanvas->SetSourceKey(fixtureCaptureKey + "_part" +
                                                std::to_string(partIndex));
                  auto applyCapture =
                      [objTransform = obj.transform](
                          const std::array<float, 3> &p) {
                        return TransformPoint(objTransform, p);
                      };
                  DrawMeshWithOutline(obj.mesh, r, g, b, RENDER_SCALE, false,
                                      false, 0.0f, 0.0f, 0.0f, wireframe, mode,
                                      applyCapture);
                  ++partIndex;
                }
              } else {
                m_captureCanvas->SetSourceKey(fixtureCaptureKey);
                DrawCubeWithOutline(0.2f, r, g, b, false, false, 0.0f, 0.0f,
                                    0.0f, wireframe, mode,
                                    [](const std::array<float, 3> &p) {
                                      return p;
                                    });
              }

              localCanvas->EndFrame();
              definition.bounds = ComputeSymbolBounds(definition.localCommands);

              m_captureCanvas = prevCanvas;
              m_captureView = prevView;
              m_captureOnly = prevCaptureOnly;
              m_captureIncludeGrid = prevIncludeGrid;
              return definition;
            });

        Transform2D instanceTransform =
            BuildInstanceTransform2D(fixtureTransform, m_captureView);
        m_captureCanvas->PlaceSymbolInstance(symbol.symbolId,
                                             instanceTransform);
        placedInstance = true;
      }
    }
    suppressCapture = placedInstance;

    auto drawFixtureGeometry = [&]() {
      if (itg != m_loadedGdtf.end()) {
        size_t partIndex = 0;
        for (const auto &obj : itg->second) {
          glPushMatrix();
          if (m_captureCanvas) {
            // Capture each GDTF geometry as its own source so fills do not hide
            // outlines from sibling parts when exported to PDF.
            m_captureCanvas->SetSourceKey(fixtureCaptureKey + "_part" +
                                          std::to_string(partIndex));
          }
          float m2[16];
          MatrixToArray(obj.transform, m2);
          // GDTF geometry offsets are defined relative to the fixture
          // in meters. Only the vertex coordinates need unit scaling.
          ApplyTransform(m2, false);
          auto applyCapture =
              [fixtureTransform, objTransform = obj.transform](
                  const std::array<float, 3> &p) {
                auto local = TransformPoint(objTransform, p);
                return TransformPoint(fixtureTransform, local);
              };
          DrawMeshWithOutline(obj.mesh, r, g, b, RENDER_SCALE, highlight,
                              selected, cx, cy, cz, wireframe, mode,
                              applyCapture);
          glPopMatrix();
          ++partIndex;
        }
      } else {
        DrawCubeWithOutline(0.2f, r, g, b, highlight, selected, cx, cy, cz,
                            wireframe, mode, applyFixtureCapture);
      }
    };

    if (suppressCapture) {
      ICanvas2D *prevCanvas = m_captureCanvas;
      bool prevCaptureOnly = m_captureOnly;
      m_captureCanvas = nullptr;
      m_captureOnly = false;
      drawFixtureGeometry();
      m_captureCanvas = prevCanvas;
      m_captureOnly = prevCaptureOnly;
    } else {
      drawFixtureGeometry();
    }

    glPopMatrix();

    if (m_captureCanvas)
      m_captureCanvas->SetSourceKey("unknown");
  }
  if (forceFixturesOnTop && depthEnabled)
    glEnable(GL_DEPTH_TEST);

  const auto &groups = SceneDataManager::Instance().GetGroupObjects();
  for (const auto &[uuid, g] : groups) {
    (void)uuid;
    (void)g; // groups not implemented
  }

  if (showGrid && gridOnTop) {
    glDisable(GL_DEPTH_TEST);
    DrawGrid(gridStyle, gridR, gridG, gridB, view);
    glEnable(GL_DEPTH_TEST);
  }

  DrawAxes();
}

void Viewer3DController::SetDarkMode(bool enabled) { m_darkMode = enabled; }

std::array<float, 3> Viewer3DController::AdjustColor(float r, float g,
                                                     float b) const {
  if (!m_darkMode)
    return {r, g, b};
  return {r, g, b};
}

void Viewer3DController::SetGLColor(float r, float g, float b) const {
  auto adjusted = AdjustColor(r, g, b);
  glColor3f(adjusted[0], adjusted[1], adjusted[2]);
}

// Draws a solid cube centered at origin with given size and color
void Viewer3DController::DrawCube(float size, float r, float g, float b) {
  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

  if (!m_captureOnly) {
    SetGLColor(r, g, b);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1); // Front
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(x1, y0, z0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0); // Back
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z0); // Left
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1); // Right
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y1, z0); // Top
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z1); // Bottom
    glEnd();
  }
}

// Draws a wireframe cube centered at origin with given size and color
void Viewer3DController::DrawWireframeCube(
    float size, float r, float g, float b, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

  float lineWidth = (mode == Viewer2DRenderMode::Wireframe) ? 1.0f : 2.0f;
  if (!m_captureOnly) {
    glLineWidth(lineWidth);
    SetGLColor(r, g, b);
  }
  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = lineWidth;
  if (!m_captureOnly) {
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glEnd();
  }
  if (m_captureCanvas) {
    std::vector<std::array<float, 3>> verts = {{x0, y0, z0}, {x1, y0, z0},
                                               {x0, y1, z0}, {x1, y1, z0},
                                               {x0, y0, z1}, {x1, y0, z1},
                                               {x0, y1, z1}, {x1, y1, z1}};
    if (captureTransform) {
      for (auto &p : verts)
        p = captureTransform(p);
    }
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2},
                              {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5},
                              {2, 6}, {3, 7}};
    for (auto &e : edges)
      RecordLine(verts[e[0]], verts[e[1]], stroke);
  }
  glLineWidth(1.0f);
  if (mode != Viewer2DRenderMode::Wireframe) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    SetGLColor(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

// Draws a wireframe box whose origin sits at the left end of the span.
// The box extends along +X for the given length and is centered in Y/Z.
void Viewer3DController::DrawWireframeBox(
    float length, float height, float width, bool highlight, bool selected,
    bool wireframe, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  float x0 = 0.0f, x1 = length;
  float y0 = -width * 0.5f, y1 = width * 0.5f;
  float z0 = 0.0f, z1 = height;

  if (wireframe) {
    float lineWidth = (mode == Viewer2DRenderMode::Wireframe) ? 1.0f : 2.0f;
    if (!m_captureOnly) {
      glLineWidth(lineWidth);
      SetGLColor(0.0f, 0.0f, 0.0f);
    }
    CanvasStroke stroke;
    stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
    stroke.width = lineWidth;
    if (!m_captureOnly) {
      glBegin(GL_LINES);
      glVertex3f(x0, y0, z0);
      glVertex3f(x1, y0, z0);
      glVertex3f(x0, y1, z0);
      glVertex3f(x1, y1, z0);
      glVertex3f(x0, y0, z1);
      glVertex3f(x1, y0, z1);
      glVertex3f(x0, y1, z1);
      glVertex3f(x1, y1, z1);
      glVertex3f(x0, y0, z0);
      glVertex3f(x0, y1, z0);
      glVertex3f(x1, y0, z0);
      glVertex3f(x1, y1, z0);
      glVertex3f(x0, y0, z1);
      glVertex3f(x0, y1, z1);
      glVertex3f(x1, y0, z1);
      glVertex3f(x1, y1, z1);
      glVertex3f(x0, y0, z0);
      glVertex3f(x0, y0, z1);
      glVertex3f(x1, y0, z0);
      glVertex3f(x1, y0, z1);
      glVertex3f(x0, y1, z0);
      glVertex3f(x0, y1, z1);
      glVertex3f(x1, y1, z0);
      glVertex3f(x1, y1, z1);
      glEnd();
    }
    if (m_captureCanvas) {
      std::vector<std::array<float, 3>> verts = {{x0, y0, z0}, {x1, y0, z0},
                                                 {x0, y1, z0}, {x1, y1, z0},
                                                 {x0, y0, z1}, {x1, y0, z1},
                                                 {x0, y1, z1}, {x1, y1, z1}};
      const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2},
                                {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5},
                                {2, 6}, {3, 7}};
      for (auto &e : edges)
        RecordLine(verts[e[0]], verts[e[1]], stroke);
    }
    if (!m_captureOnly) {
      glLineWidth(1.0f);
      if (mode != Viewer2DRenderMode::Wireframe) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        SetGLColor(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex3f(x0, y0, z1);
        glVertex3f(x1, y0, z1);
        glVertex3f(x1, y1, z1);
        glVertex3f(x0, y1, z1);
        glEnd();
        glDisable(GL_POLYGON_OFFSET_FILL);
      }
    }
    return;
  } else if (!m_captureOnly) {
    if (selected)
      SetGLColor(0.0f, 1.0f, 1.0f);
    else if (highlight)
      SetGLColor(0.0f, 1.0f, 0.0f);
    else
      SetGLColor(1.0f, 1.0f, 0.0f);
  }

  CanvasStroke stroke;
  stroke.width = 1.0f;
  if (selected)
    stroke.color = {0.0f, 1.0f, 1.0f, 1.0f};
  else if (highlight)
    stroke.color = {0.0f, 1.0f, 0.0f, 1.0f};
  else
    stroke.color = {1.0f, 1.0f, 0.0f, 1.0f};

  if (!m_captureOnly) {
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glEnd();
  }
  if (m_captureCanvas) {
    std::vector<std::array<float, 3>> verts = {{x0, y0, z0}, {x1, y0, z0},
                                               {x0, y1, z0}, {x1, y1, z0},
                                               {x0, y0, z1}, {x1, y0, z1},
                                               {x0, y1, z1}, {x1, y1, z1}};
    if (captureTransform) {
      for (auto &p : verts)
        p = captureTransform(p);
    }
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2},
                              {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5},
                              {2, 6}, {3, 7}};
    for (auto &e : edges)
      RecordLine(verts[e[0]], verts[e[1]], stroke);
  }
}

// Draws a colored cube. If selected or highlighted it is tinted
// in cyan or green respectively instead of its original color.
void Viewer3DController::DrawCubeWithOutline(
    float size, float r, float g, float b, bool highlight, bool selected,
    float cx, float cy, float cz, bool wireframe, Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  (void)cx;
  (void)cy;
  (void)cz; // parameters no longer used

  if (wireframe) {
    if (mode == Viewer2DRenderMode::Wireframe) {
      DrawWireframeCube(size, 0.0f, 0.0f, 0.0f, mode, captureTransform);
      return;
    }
    DrawWireframeCube(size, 0.0f, 0.0f, 0.0f, mode, captureTransform);
    if (m_captureCanvas) {
      float half = size / 2.0f;
      float x0 = -half, x1 = half;
      float y0 = -half, y1 = half;
      float z0 = -half, z1 = half;
      std::vector<std::array<float, 3>> verts = {
          {x0, y0, z0}, {x1, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
          {x0, y0, z1}, {x1, y0, z1}, {x0, y1, z1}, {x1, y1, z1}};
      if (captureTransform) {
        for (auto &p : verts)
          p = captureTransform(p);
      }
      float lineWidth = (mode == Viewer2DRenderMode::Wireframe) ? 1.0f : 2.0f;
      CanvasStroke stroke;
      stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
      stroke.width = lineWidth;
      CanvasFill fill;
      fill.color = {r, g, b, 1.0f};
      const int faces[6][4] = {{0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4},
                               {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}};
      for (const auto &face : faces) {
        std::vector<std::array<float, 3>> pts = {
            verts[face[0]], verts[face[1]], verts[face[2]], verts[face[3]]};
        RecordPolygon(pts, stroke, &fill);
      }
    }
    if (!m_captureOnly) {
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(-1.0f, -1.0f);
      SetGLColor(r, g, b);
      DrawCube(size, r, g, b);
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    return;
  }

  if (selected)
    DrawCube(size, 0.0f, 1.0f, 1.0f);
  else if (highlight)
    DrawCube(size, 0.0f, 1.0f, 0.0f);
  else
    DrawCube(size, r, g, b);
}

// Draws a mesh using the given color. When selected or highlighted the
// mesh is rendered entirely in cyan or green respectively.
void Viewer3DController::DrawMeshWithOutline(
    const Mesh &mesh, float r, float g, float b, float scale, bool highlight,
    bool selected, float cx, float cy, float cz, bool wireframe,
    Viewer2DRenderMode mode,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  (void)cx;
  (void)cy;
  (void)cz; // parameters kept for compatibility

  if (wireframe) {
    float lineWidth = (mode == Viewer2DRenderMode::Wireframe) ? 1.0f : 2.0f;
    if (!m_captureOnly) {
      glLineWidth(lineWidth);
      SetGLColor(0.0f, 0.0f, 0.0f);
    }
    CanvasStroke stroke;
    stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
    stroke.width = lineWidth;
    DrawMeshWireframe(mesh, scale, captureTransform);
    if (m_captureCanvas && mode != Viewer2DRenderMode::Wireframe) {
      CanvasFill fill;
      fill.color = {r, g, b, 1.0f};
      for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        unsigned short i0 = mesh.indices[i];
        unsigned short i1 = mesh.indices[i + 1];
        unsigned short i2 = mesh.indices[i + 2];
        std::vector<std::array<float, 3>> pts = {
            {mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
             mesh.vertices[i0 * 3 + 2] * scale},
            {mesh.vertices[i1 * 3] * scale, mesh.vertices[i1 * 3 + 1] * scale,
             mesh.vertices[i1 * 3 + 2] * scale},
            {mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
             mesh.vertices[i2 * 3 + 2] * scale}};
        if (captureTransform) {
          for (auto &p : pts)
            p = captureTransform(p);
        }
        RecordPolygon(pts, stroke, &fill);
      }
    }
    if (!m_captureOnly) {
      glLineWidth(1.0f);
      if (mode != Viewer2DRenderMode::Wireframe) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        SetGLColor(r, g, b);
        DrawMesh(mesh, scale);
        glDisable(GL_POLYGON_OFFSET_FILL);
      }
    }
    return;
  }

  if (!m_captureOnly) {
    if (selected)
      SetGLColor(0.0f, 1.0f, 1.0f);
    else if (highlight)
      SetGLColor(0.0f, 1.0f, 0.0f);
    else
      SetGLColor(r, g, b);

    DrawMesh(mesh, scale);
  }
  if (m_captureCanvas) {
    CanvasStroke stroke;
    stroke.color = {r, g, b, 1.0f};
    stroke.width = 0.0f;
    CanvasFill fill;
    fill.color = {r, g, b, 1.0f};
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      unsigned short i0 = mesh.indices[i];
      unsigned short i1 = mesh.indices[i + 1];
      unsigned short i2 = mesh.indices[i + 2];
      std::vector<std::array<float, 3>> pts = {
          {mesh.vertices[i0 * 3] * scale, mesh.vertices[i0 * 3 + 1] * scale,
           mesh.vertices[i0 * 3 + 2] * scale},
          {mesh.vertices[i1 * 3] * scale, mesh.vertices[i1 * 3 + 1] * scale,
           mesh.vertices[i1 * 3 + 2] * scale},
          {mesh.vertices[i2 * 3] * scale, mesh.vertices[i2 * 3 + 1] * scale,
           mesh.vertices[i2 * 3 + 2] * scale}};
      if (captureTransform) {
        for (auto &p : pts)
          p = captureTransform(p);
      }
      RecordPolygon(pts, stroke, &fill);
    }
  }
}

// Draws a mesh using GL triangles. The optional scale parameter allows
// converting vertex units (e.g. millimeters) to meters.
void Viewer3DController::DrawMeshWireframe(
    const Mesh &mesh, float scale,
    const std::function<std::array<float, 3>(const std::array<float, 3> &)> &
        captureTransform) {
  if (!m_captureOnly) {
    glBegin(GL_LINES);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      unsigned short i0 = mesh.indices[i];
      unsigned short i1 = mesh.indices[i + 1];
      unsigned short i2 = mesh.indices[i + 2];

      float v0x = mesh.vertices[i0 * 3] * scale;
      float v0y = mesh.vertices[i0 * 3 + 1] * scale;
      float v0z = mesh.vertices[i0 * 3 + 2] * scale;

      float v1x = mesh.vertices[i1 * 3] * scale;
      float v1y = mesh.vertices[i1 * 3 + 1] * scale;
      float v1z = mesh.vertices[i1 * 3 + 2] * scale;

      float v2x = mesh.vertices[i2 * 3] * scale;
      float v2y = mesh.vertices[i2 * 3 + 1] * scale;
      float v2z = mesh.vertices[i2 * 3 + 2] * scale;

      glVertex3f(v0x, v0y, v0z);
      glVertex3f(v1x, v1y, v1z);

      glVertex3f(v1x, v1y, v1z);
      glVertex3f(v2x, v2y, v2z);

      glVertex3f(v2x, v2y, v2z);
      glVertex3f(v0x, v0y, v0z);
    }
    glEnd();
  }
  if (m_captureCanvas) {
    CanvasStroke stroke;
    stroke.color = {0.0f, 0.0f, 0.0f, 1.0f};
    stroke.width = 1.0f;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      unsigned short i0 = mesh.indices[i];
      unsigned short i1 = mesh.indices[i + 1];
      unsigned short i2 = mesh.indices[i + 2];

      std::array<float, 3> p0 = {mesh.vertices[i0 * 3] * scale,
                                 mesh.vertices[i0 * 3 + 1] * scale,
                                 mesh.vertices[i0 * 3 + 2] * scale};
      std::array<float, 3> p1 = {mesh.vertices[i1 * 3] * scale,
                                 mesh.vertices[i1 * 3 + 1] * scale,
                                 mesh.vertices[i1 * 3 + 2] * scale};
      std::array<float, 3> p2 = {mesh.vertices[i2 * 3] * scale,
                                 mesh.vertices[i2 * 3 + 1] * scale,
                                 mesh.vertices[i2 * 3 + 2] * scale};
      if (captureTransform) {
        p0 = captureTransform(p0);
        p1 = captureTransform(p1);
        p2 = captureTransform(p2);
      }
      RecordLine(p0, p1, stroke);
      RecordLine(p1, p2, stroke);
      RecordLine(p2, p0, stroke);
    }
  }
}

// Draws a mesh using GL triangles. The optional scale parameter allows
// converting vertex units (e.g. millimeters) to meters.
void Viewer3DController::DrawMesh(const Mesh &mesh, float scale) {
  if (!m_captureOnly) {
    glBegin(GL_TRIANGLES);
    bool hasNormals = mesh.normals.size() >= mesh.vertices.size();
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      unsigned short i0 = mesh.indices[i];
      unsigned short i1 = mesh.indices[i + 1];
      unsigned short i2 = mesh.indices[i + 2];

      float v0x = mesh.vertices[i0 * 3] * scale;
      float v0y = mesh.vertices[i0 * 3 + 1] * scale;
      float v0z = mesh.vertices[i0 * 3 + 2] * scale;

      float v1x = mesh.vertices[i1 * 3] * scale;
      float v1y = mesh.vertices[i1 * 3 + 1] * scale;
      float v1z = mesh.vertices[i1 * 3 + 2] * scale;

      float v2x = mesh.vertices[i2 * 3] * scale;
      float v2y = mesh.vertices[i2 * 3 + 1] * scale;
      float v2z = mesh.vertices[i2 * 3 + 2] * scale;

      if (hasNormals) {
        glNormal3f(mesh.normals[i0 * 3], mesh.normals[i0 * 3 + 1],
                   mesh.normals[i0 * 3 + 2]);
        glVertex3f(v0x, v0y, v0z);
        glNormal3f(mesh.normals[i1 * 3], mesh.normals[i1 * 3 + 1],
                   mesh.normals[i1 * 3 + 2]);
        glVertex3f(v1x, v1y, v1z);
        glNormal3f(mesh.normals[i2 * 3], mesh.normals[i2 * 3 + 1],
                   mesh.normals[i2 * 3 + 2]);
        glVertex3f(v2x, v2y, v2z);
      } else {
        float ux = v1x - v0x;
        float uy = v1y - v0y;
        float uz = v1z - v0z;

        float vx = v2x - v0x;
        float vy = v2y - v0y;
        float vz = v2z - v0z;

        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;

        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) {
          nx /= len;
          ny /= len;
          nz /= len;
        }

        glNormal3f(nx, ny, nz);
        glVertex3f(v0x, v0y, v0z);
        glVertex3f(v1x, v1y, v1z);
        glVertex3f(v2x, v2y, v2z);
      }
    }
    glEnd();
  }
}

// Draws the reference grid on one of the principal planes
void Viewer3DController::DrawGrid(int style, float r, float g, float b,
                                  Viewer2DView view) {
  const float size = 20.0f;
  const float step = 1.0f;

  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = 1.0f;

  SetGLColor(r, g, b);
  if (style == 0) {
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (float i = -size; i <= size; i += step) {
      switch (view) {
      case Viewer2DView::Top:
      case Viewer2DView::Bottom:
        glVertex3f(i, -size, 0.0f);
        glVertex3f(i, size, 0.0f);
        glVertex3f(-size, i, 0.0f);
        glVertex3f(size, i, 0.0f);
        if (m_captureCanvas && m_captureIncludeGrid) {
          RecordLine({i, -size, 0.0f}, {i, size, 0.0f}, stroke);
          RecordLine({-size, i, 0.0f}, {size, i, 0.0f}, stroke);
        }
        break;
      case Viewer2DView::Front:
        glVertex3f(i, 0.0f, -size);
        glVertex3f(i, 0.0f, size);
        glVertex3f(-size, 0.0f, i);
        glVertex3f(size, 0.0f, i);
        if (m_captureCanvas && m_captureIncludeGrid) {
          RecordLine({i, 0.0f, -size}, {i, 0.0f, size}, stroke);
          RecordLine({-size, 0.0f, i}, {size, 0.0f, i}, stroke);
        }
        break;
      case Viewer2DView::Side:
        glVertex3f(0.0f, i, -size);
        glVertex3f(0.0f, i, size);
        glVertex3f(0.0f, -size, i);
        glVertex3f(0.0f, size, i);
        if (m_captureCanvas && m_captureIncludeGrid) {
          RecordLine({0.0f, i, -size}, {0.0f, i, size}, stroke);
          RecordLine({0.0f, -size, i}, {0.0f, size, i}, stroke);
        }
        break;
      }
    }
    glEnd();
  } else if (style == 1) {
    GLboolean pointSmooth = glIsEnabled(GL_POINT_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        switch (view) {
        case Viewer2DView::Top:
        case Viewer2DView::Bottom:
          glVertex3f(x, y, 0.0f);
          if (m_captureCanvas && m_captureIncludeGrid)
            RecordLine({x, y, 0.0f}, {x, y, 0.0f}, stroke);
          break;
        case Viewer2DView::Front:
          glVertex3f(x, 0.0f, y);
          if (m_captureCanvas && m_captureIncludeGrid)
            RecordLine({x, 0.0f, y}, {x, 0.0f, y}, stroke);
          break;
        case Viewer2DView::Side:
          glVertex3f(0.0f, x, y);
          if (m_captureCanvas && m_captureIncludeGrid)
            RecordLine({0.0f, x, y}, {0.0f, x, y}, stroke);
          break;
        }
      }
    }
    glEnd();
    if (pointSmooth)
      glEnable(GL_POINT_SMOOTH);
  } else {
    float half = step * 0.1f;
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (float x = -size; x <= size; x += step) {
      for (float y = -size; y <= size; y += step) {
        switch (view) {
        case Viewer2DView::Top:
        case Viewer2DView::Bottom:
          glVertex3f(x - half, y, 0.0f);
          glVertex3f(x + half, y, 0.0f);
          glVertex3f(x, y - half, 0.0f);
          glVertex3f(x, y + half, 0.0f);
          if (m_captureCanvas && m_captureIncludeGrid) {
            RecordLine({x - half, y, 0.0f}, {x + half, y, 0.0f}, stroke);
            RecordLine({x, y - half, 0.0f}, {x, y + half, 0.0f}, stroke);
          }
          break;
        case Viewer2DView::Front:
          glVertex3f(x - half, 0.0f, y);
          glVertex3f(x + half, 0.0f, y);
          glVertex3f(x, 0.0f, y - half);
          glVertex3f(x, 0.0f, y + half);
          if (m_captureCanvas && m_captureIncludeGrid) {
            RecordLine({x - half, 0.0f, y}, {x + half, 0.0f, y}, stroke);
            RecordLine({x, 0.0f, y - half}, {x, 0.0f, y + half}, stroke);
          }
          break;
        case Viewer2DView::Side:
          glVertex3f(0.0f, x - half, y);
          glVertex3f(0.0f, x + half, y);
          glVertex3f(0.0f, x, y - half);
          glVertex3f(0.0f, x, y + half);
          if (m_captureCanvas && m_captureIncludeGrid) {
            RecordLine({0.0f, x - half, y}, {0.0f, x + half, y}, stroke);
            RecordLine({0.0f, x, y - half}, {0.0f, x, y + half}, stroke);
          }
          break;
        }
      }
    }
    glEnd();
  }
}

// Draws the XYZ axes centered at origin
void Viewer3DController::DrawAxes() {
  glLineWidth(2.0f);
  glBegin(GL_LINES);
  SetGLColor(1.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(1.0f, 0.0f, 0.0f); // X
  SetGLColor(0.0f, 1.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 1.0f, 0.0f); // Y
  SetGLColor(0.0f, 0.0f, 1.0f);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 1.0f); // Z
  glEnd();
  if (m_captureCanvas) {
    CanvasStroke stroke;
    stroke.width = 2.0f;
    stroke.color = {1.0f, 0.0f, 0.0f, 1.0f};
    RecordLine({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, stroke);
    stroke.color = {0.0f, 1.0f, 0.0f, 1.0f};
    RecordLine({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, stroke);
    stroke.color = {0.0f, 0.0f, 1.0f, 1.0f};
    RecordLine({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, stroke);
  }
}

// Multiplies the current matrix by the given transform. When
// scaleTranslation is true the translation part is converted from
// millimeters to meters using RENDER_SCALE.
void Viewer3DController::ApplyTransform(const float matrix[16],
                                        bool scaleTranslation) {
  float m[16];
  std::copy(matrix, matrix + 16, m);
  if (scaleTranslation) {
    m[12] *= RENDER_SCALE;
    m[13] *= RENDER_SCALE;
    m[14] *= RENDER_SCALE;
  }
  glMultMatrixf(m);
}

// Initializes simple lighting for the scene
void Viewer3DController::SetupBasicLighting() {
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
  GLfloat diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
  GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
  GLfloat position[] = {2.0f, -4.0f, 5.0f, 0.0f};

  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
  glLightfv(GL_LIGHT0, GL_POSITION, position);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glShadeModel(GL_SMOOTH);
}

void Viewer3DController::SetupMaterialFromRGB(float r, float g, float b) {
  SetGLColor(r, g, b);
}

void Viewer3DController::DrawFixtureLabels(int width, int height) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  ConfigManager &cfg = ConfigManager::Get();
  bool showName = cfg.GetFloat("label_show_name") != 0.0f;
  bool showId = cfg.GetFloat("label_show_id") != 0.0f;
  bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;
  // The 3D view does not scale labels with any zoom factor, but the
  // configuration values are multiplied by a zoom parameter in the 2D view.
  // Use a fixed zoom of 1.0f here so that the code compiles and label sizes
  // are consistent with the configured defaults.
  float zoom = 1.0f;
  float nameSize = cfg.GetFloat("label_font_size_name") * zoom;
  float idSize = cfg.GetFloat("label_font_size_id") * zoom;
  float dmxSize = cfg.GetFloat("label_font_size_dmx") * zoom;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  for (const auto &[uuid, f] : fixtures) {
    if (!cfg.IsLayerVisible(f.layer))
      continue;
    if (uuid != m_highlightUuid)
      continue;

    double sx, sy, sz;
    // Use bounding box center if available
    auto bit = m_fixtureBounds.find(uuid);
    if (bit != m_fixtureBounds.end()) {
      const BoundingBox &bb = bit->second;
      double wx = (bb.min[0] + bb.max[0]) * 0.5;
      double wy = (bb.min[1] + bb.max[1]) * 0.5;
      double wz = (bb.min[2] + bb.max[2]) * 0.5;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    } else {
      double wx = f.transform.o[0] * RENDER_SCALE;
      double wy = f.transform.o[1] * RENDER_SCALE;
      double wz = f.transform.o[2] * RENDER_SCALE;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    }

    int x = static_cast<int>(sx);
    int y = height - static_cast<int>(sy);
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
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y);
  }
}

// Renders labels for all fixtures in the current scene. Each label displays the
// fixture's instance name (or UUID), numeric ID and DMX address. The label
// position is determined by a configurable distance and angle from the fixture
// center so that, by default, labels appear slightly below the fixture in the
// 2D top-down view. The label width derives from the fixture bounds so the
// drawn tag roughly matches the fixture size, and the optional zoom parameter
// scales the label like regular geometry when zooming the 2D view.
void Viewer3DController::DrawAllFixtureLabels(int width, int height,
                                              float zoom) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  ConfigManager &cfg = ConfigManager::Get();
  const std::array<const char *, 4> nameKeys = {"label_show_name_top",
                                               "label_show_name_front",
                                               "label_show_name_side",
                                               "label_show_name_top"};
  const std::array<const char *, 4> idKeys = {"label_show_id_top",
                                             "label_show_id_front",
                                             "label_show_id_side",
                                             "label_show_id_top"};
  const std::array<const char *, 4> dmxKeys = {"label_show_dmx_top",
                                              "label_show_dmx_front",
                                              "label_show_dmx_side",
                                              "label_show_dmx_top"};
  const std::array<const char *, 4> distKeys = {"label_offset_distance_top",
                                                "label_offset_distance_front",
                                                "label_offset_distance_side",
                                                "label_offset_distance_top"};
  const std::array<const char *, 4> angleKeys = {"label_offset_angle_top",
                                                 "label_offset_angle_front",
                                                 "label_offset_angle_side",
                                                 "label_offset_angle_top"};
  int viewIdx = static_cast<int>(cfg.GetFloat("view2d_view"));
  bool showName = cfg.GetFloat(nameKeys[viewIdx]) != 0.0f;
  bool showId = cfg.GetFloat(idKeys[viewIdx]) != 0.0f;
  bool showDmx = cfg.GetFloat(dmxKeys[viewIdx]) != 0.0f;
  float nameSize = cfg.GetFloat("label_font_size_name") * zoom;
  float idSize = cfg.GetFloat("label_font_size_id") * zoom;
  float dmxSize = cfg.GetFloat("label_font_size_dmx") * zoom;
  float labelDist = cfg.GetFloat(distKeys[viewIdx]);
  float labelAngle = cfg.GetFloat(angleKeys[viewIdx]);
  constexpr float deg2rad = 3.14159265358979323846f / 180.0f;
  float angRad = labelAngle * deg2rad;
  float offX = 0.0f;
  float offY = 0.0f;
  float offZ = 0.0f;
  switch (static_cast<Viewer2DView>(viewIdx)) {
  case Viewer2DView::Top:
    offX = labelDist * std::sin(angRad);
    offY = labelDist * std::cos(angRad);
    break;
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

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();
  for (const auto &[uuid, f] : fixtures) {
    if (!cfg.IsLayerVisible(f.layer))
      continue;

    double wx, wy, wz;
    auto bit = m_fixtureBounds.find(uuid);
    if (bit != m_fixtureBounds.end()) {
      const BoundingBox &bb = bit->second;
      double cx = (bb.min[0] + bb.max[0]) * 0.5;
      double cy = (bb.min[1] + bb.max[1]) * 0.5;
      double cz = (bb.min[2] + bb.max[2]) * 0.5;

      // Anchor label offsets from the top of the fixture relative to the active
      // view so the distance matches the PDF output expectation. Top view uses
      // +Y as "up" while front/side views share +Z.
      switch (static_cast<Viewer2DView>(viewIdx)) {
      case Viewer2DView::Top:
        cy = bb.max[1];
        break;
      case Viewer2DView::Bottom:
        cy = bb.max[1];
        break;
      case Viewer2DView::Front:
      case Viewer2DView::Side:
        cz = bb.max[2];
        break;
      }

      wx = cx + offX;
      wy = cy + offY;
      wz = cz + offZ;
    } else {
      double cx = f.transform.o[0] * RENDER_SCALE;
      double cy = f.transform.o[1] * RENDER_SCALE;
      double cz = f.transform.o[2] * RENDER_SCALE;
      wx = cx + offX;
      wy = cy + offY;
      wz = cz + offZ;
    }

    double sx, sy, sz;
    if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) != GL_TRUE)
      continue;

    int x = static_cast<int>(sx);
    // Convert OpenGL's origin to top-left.
    int y = height - static_cast<int>(sy);

    std::vector<LabelLine2D> lines;
    if (showName) {
      wxString baseName = f.instanceName.empty()
                              ? wxString::FromUTF8(uuid)
                              : wxString::FromUTF8(f.instanceName);
      wxString wrapped = WrapEveryTwoWords(baseName);
      wxStringTokenizer nameLines(wrapped, "\n");
      while (nameLines.HasMoreTokens()) {
        wxString line = nameLines.GetNextToken();
        auto utf8 = line.ToUTF8();
        lines.push_back(
            {m_font, std::string(utf8.data(), utf8.length()), nameSize});
      }
    }
    if (showId) {
      wxString idLine = "ID: " + wxString::Format("%d", f.fixtureId);
      auto utf8 = idLine.ToUTF8();
      lines.push_back(
          {m_font, std::string(utf8.data(), utf8.length()), idSize});
    }
    if (showDmx && !f.address.empty()) {
      wxString addrLine = wxString::FromUTF8(f.address);
      auto utf8 = addrLine.ToUTF8();
      lines.push_back(
          {m_font, std::string(utf8.data(), utf8.length()), dmxSize});
    }
    if (lines.empty())
      continue;

    if (m_captureCanvas) {
      std::string labelSourceKey = "label:" + uuid;
      m_captureCanvas->SetSourceKey(labelSourceKey);

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

      // Measure each line with NanoVG so the PDF layout matches the on-screen
      // rendering. Measurements happen in pixel space and are converted to the
      // world-space units consumed by the PDF exporter.
      for (const auto &ln : lines) {
        worldFontSizes.push_back(ln.size * pxToWorld);
        nvgFontSize(m_vg, ln.size);
        nvgFontFaceId(m_vg, m_font);
        nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        float bounds[4];
        nvgTextBounds(m_vg, 0.f, 0.f, ln.text.c_str(), nullptr, bounds);
        lineHeightsWorld.push_back((bounds[3] - bounds[1]) * pxToWorld);
        float ascender = 0.0f;
        float descender = 0.0f;
        float lineh = 0.0f;
        nvgTextMetrics(m_vg, &ascender, &descender, &lineh);
        ascentsWorld.push_back(ascender * pxToWorld);
        descentsWorld.push_back(-descender * pxToWorld);
      }

      float totalHeight = 0.0f;
      for (size_t i = 0; i < lineHeightsWorld.size(); ++i) {
        totalHeight += lineHeightsWorld[i];
        if (i + 1 < lineHeightsWorld.size())
          totalHeight += lineSpacingWorld;
      }

      auto toPlan2D = [](double wx, double wy, double wz, Viewer2DView view) {
        switch (view) {
        case Viewer2DView::Top:
          return std::array<float, 2>{static_cast<float>(wx),
                                      static_cast<float>(wy)};
        case Viewer2DView::Bottom:
          return std::array<float, 2>{static_cast<float>(wx),
                                      static_cast<float>(wy)};
        case Viewer2DView::Front:
          return std::array<float, 2>{static_cast<float>(wx),
                                      static_cast<float>(wz)};
        case Viewer2DView::Side:
          return std::array<float, 2>{static_cast<float>(-wy),
                                      static_cast<float>(wz)};
        }
        return std::array<float, 2>{static_cast<float>(wx),
                                    static_cast<float>(wy)};
      };

      auto anchor =
          toPlan2D(wx, wy, wz, static_cast<Viewer2DView>(viewIdx));
      // Start from the top of the label block instead of the bottom.
      // We subtract the ascent from currentY so the baseline matches the
      // top-aligned position.
      // After recording each line, we move downward by the full line height
      // plus spacing.
      float currentY = anchor[1] + totalHeight * 0.5f;
      for (size_t i = 0; i < lines.size(); ++i) {
        CanvasTextStyle style;
        style.fontFamily = "sans";
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
          trace << "[label-capture] fixture=" << uuid << " source="
                << labelSourceKey << " text=\"" << lines[i].text << "\" x="
                << anchor[0] << " baseline=" << baseline
                << " size=" << style.fontSize << " vAlign=Baseline";
          Logger::Instance().Log(trace.str());
        }
        RecordText(anchor[0], baseline, lines[i].text, style);
        if (i + 1 < lines.size())
          currentY -= lineHeightsWorld[i] + lineSpacingWorld;
      }
    }

    NVGcolor textColor =
        m_darkMode ? nvgRGBAf(1.f, 1.f, 1.f, 1.f)
                   : nvgRGBAf(0.f, 0.f, 0.f, 1.f);
    NVGcolor outlineColor =
        m_darkMode ? nvgRGBAf(0.f, 0.f, 0.f, 1.f)
                   : nvgRGBAf(1.f, 1.f, 1.f, 1.f);
    DrawLabelLines2D(m_vg, lines, x, y, textColor, outlineColor, true);
  }
}

bool Viewer3DController::GetFixtureLabelAt(int mouseX, int mouseY, int width,
                                           int height, wxString &outLabel,
                                           wxPoint &outPos,
                                           std::string *outUuid) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);
  ConfigManager &cfg = ConfigManager::Get();
  bool showName = cfg.GetFloat("label_show_name") != 0.0f;
  bool showId = cfg.GetFloat("label_show_id") != 0.0f;
  bool showDmx = cfg.GetFloat("label_show_dmx") != 0.0f;

  const auto &fixtures = SceneDataManager::Instance().GetFixtures();

  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;

  for (const auto &[uuid, f] : fixtures) {
    if (!cfg.IsLayerVisible(f.layer))
      continue;
    auto bit = m_fixtureBounds.find(uuid);
    if (bit == m_fixtureBounds.end())
      continue;

    const BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
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
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = label;
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

void Viewer3DController::DrawTrussLabels(int width, int height) {

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  for (const auto &[uuid, t] : trusses) {
    if (!ConfigManager::Get().IsLayerVisible(t.layer))
      continue;
    if (uuid != m_highlightUuid)
      continue;

    double sx, sy, sz;
    auto bit = m_trussBounds.find(uuid);
    if (bit != m_trussBounds.end()) {
      const BoundingBox &bb = bit->second;
      double wx = (bb.min[0] + bb.max[0]) * 0.5;
      double wy = (bb.min[1] + bb.max[1]) * 0.5;
      double wz = (bb.min[2] + bb.max[2]) * 0.5;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    } else {
      double wx = t.transform.o[0] * RENDER_SCALE;
      double wy = t.transform.o[1] * RENDER_SCALE;
      double wz = t.transform.o[2] * RENDER_SCALE;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    }

    int x = static_cast<int>(sx);
    int y = height - static_cast<int>(sy);
    wxString label =
        t.name.empty() ? wxString::FromUTF8(uuid) : wxString::FromUTF8(t.name);
    float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
    std::string hStr = FormatMeters(baseHeight);
    label += wxString::Format("\nh = %s m", hStr.c_str());

    auto utf8 = label.ToUTF8();
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y);
  }
}

void Viewer3DController::DrawSceneObjectLabels(int width, int height) {

  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  for (const auto &[uuid, o] : objs) {
    if (!ConfigManager::Get().IsLayerVisible(o.layer))
      continue;
    if (uuid != m_highlightUuid)
      continue;

    double sx, sy, sz;
    auto bit = m_objectBounds.find(uuid);
    if (bit != m_objectBounds.end()) {
      const BoundingBox &bb = bit->second;
      double wx = (bb.min[0] + bb.max[0]) * 0.5;
      double wy = (bb.min[1] + bb.max[1]) * 0.5;
      double wz = (bb.min[2] + bb.max[2]) * 0.5;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    } else {
      double wx = o.transform.o[0] * RENDER_SCALE;
      double wy = o.transform.o[1] * RENDER_SCALE;
      double wz = o.transform.o[2] * RENDER_SCALE;
      if (gluProject(wx, wy, wz, model, proj, viewport, &sx, &sy, &sz) !=
          GL_TRUE)
        continue;
    }

    int x = static_cast<int>(sx);
    int y = height - static_cast<int>(sy);
    wxString label =
        o.name.empty() ? wxString::FromUTF8(uuid) : wxString::FromUTF8(o.name);

    auto utf8 = label.ToUTF8();
    DrawText2D(m_vg, m_font, std::string(utf8.data(), utf8.length()), x, y);
  }
}

bool Viewer3DController::GetTrussLabelAt(int mouseX, int mouseY, int width,
                                         int height, wxString &outLabel,
                                         wxPoint &outPos,
                                         std::string *outUuid) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &trusses = SceneDataManager::Instance().GetTrusses();
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &[uuid, t] : trusses) {
    if (!ConfigManager::Get().IsLayerVisible(t.layer))
      continue;
    auto bit = m_trussBounds.find(uuid);
    if (bit == m_trussBounds.end())
      continue;

    const BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = t.name.empty() ? wxString::FromUTF8(uuid)
                                   : wxString::FromUTF8(t.name);
        float baseHeight = t.transform.o[2] - t.heightMm * 0.5f;
        std::string hStr = FormatMeters(baseHeight);
        bestLabel += wxString::Format("\nh = %s m", hStr.c_str());
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

bool Viewer3DController::GetSceneObjectLabelAt(int mouseX, int mouseY,
                                               int width, int height,
                                               wxString &outLabel,
                                               wxPoint &outPos,
                                               std::string *outUuid) {
  double model[16];
  double proj[16];
  int viewport[4];
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  const auto &objs = SceneDataManager::Instance().GetSceneObjects();
  bool found = false;
  double bestDepth = DBL_MAX;
  wxString bestLabel;
  wxPoint bestPos;
  std::string bestUuid;
  for (const auto &[uuid, o] : objs) {
    if (!ConfigManager::Get().IsLayerVisible(o.layer))
      continue;
    auto bit = m_objectBounds.find(uuid);
    if (bit == m_objectBounds.end())
      continue;

    const BoundingBox &bb = bit->second;
    std::array<std::array<float, 3>, 8> corners = {
        std::array<float, 3>{bb.min[0], bb.min[1], bb.min[2]},
        {bb.max[0], bb.min[1], bb.min[2]},
        {bb.min[0], bb.max[1], bb.min[2]},
        {bb.max[0], bb.max[1], bb.min[2]},
        {bb.min[0], bb.min[1], bb.max[2]},
        {bb.max[0], bb.min[1], bb.max[2]},
        {bb.min[0], bb.max[1], bb.max[2]},
        {bb.max[0], bb.max[1], bb.max[2]}};

    ScreenRect rect;
    double minDepth = DBL_MAX;
    bool visible = false;
    for (const auto &c : corners) {
      double sx, sy, sz;
      if (gluProject(c[0], c[1], c[2], model, proj, viewport, &sx, &sy, &sz) ==
          GL_TRUE) {
        rect.minX = std::min(rect.minX, sx);
        rect.maxX = std::max(rect.maxX, sx);
        double sy2 = height - sy;
        rect.minY = std::min(rect.minY, sy2);
        rect.maxY = std::max(rect.maxY, sy2);
        if (sz >= 0.0 && sz <= 1.0) {
          visible = true;
          minDepth = std::min(minDepth, sz);
        }
      }
    }

    if (!visible)
      continue;

    if (mouseX >= rect.minX && mouseX <= rect.maxX && mouseY >= rect.minY &&
        mouseY <= rect.maxY) {
      if (minDepth < bestDepth) {
        bestDepth = minDepth;
        bestPos.x = static_cast<int>((rect.minX + rect.maxX) * 0.5);
        bestPos.y = static_cast<int>((rect.minY + rect.maxY) * 0.5);
        bestLabel = o.name.empty() ? wxString::FromUTF8(uuid)
                                   : wxString::FromUTF8(o.name);
        bestUuid = uuid;
        found = true;
      }
    }
  }

  if (found) {
    outPos = bestPos;
    outLabel = bestLabel;
    if (outUuid)
      *outUuid = bestUuid;
  }
  return found;
}

void Viewer3DController::SetLayerColor(const std::string &layer,
                                       const std::string &hex) {
  std::array<float, 3> c;
  if (HexToRGB(hex, c[0], c[1], c[2]))
    m_layerColors[layer] = c;
  else
    m_layerColors.erase(layer);
}

std::shared_ptr<const SymbolDefinitionSnapshot>
Viewer3DController::GetBottomSymbolCacheSnapshot() const {
  return m_bottomSymbolCache.Snapshot();
}
