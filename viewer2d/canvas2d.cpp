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

#include "canvas2d.h"

#include "symbolcache.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/gl.h>
#include <wx/glcanvas.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace {
// Applies a stroke color and width to the OpenGL state for immediate mode
// drawing. The 2D viewer uses an orthographic projection so the width maps to
// logical world units consistently.
void ApplyStroke(const CanvasStroke &stroke) {
  glColor4f(stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a);
  glLineWidth(stroke.width);
}

void ApplyFill(const CanvasFill &fill) {
  glColor4f(fill.color.r, fill.color.g, fill.color.b, fill.color.a);
}
} // namespace

class RasterCanvas : public ICanvas2D {
public:
  explicit RasterCanvas(const CanvasTransform &transform) : m_transform(transform) {}

  void BeginFrame() override { glPushMatrix(); ApplyTransform(); }
  void EndFrame() override { glPopMatrix(); }

  void Save() override { glPushMatrix(); }
  void Restore() override { glPopMatrix(); }

  void SetTransform(const CanvasTransform &transform) override {
    m_transform = transform;
    glLoadIdentity();
    ApplyTransform();
  }

  void SetSourceKey(const std::string &key) override {
    (void)key;
  }

  void BeginSymbol(const std::string &key) override { (void)key; }
  void EndSymbol(const std::string &key) override { (void)key; }
  void PlaceSymbol(const std::string &key,
                   const CanvasTransform &transform) override {
    (void)key;
    (void)transform;
  }
  void PlaceSymbolInstance(uint32_t symbolId,
                           const Transform2D &transform) override {
    (void)symbolId;
    (void)transform;
  }

  void DrawLine(float x0, float y0, float x1, float y1,
                const CanvasStroke &stroke) override {
    ApplyStroke(stroke);
    glBegin(GL_LINES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
  }

  void DrawPolyline(const std::vector<float> &points,
                    const CanvasStroke &stroke) override {
    if (points.size() < 4)
      return;
    ApplyStroke(stroke);
    glBegin(GL_LINE_STRIP);
    for (size_t i = 0; i + 1 < points.size(); i += 2)
      glVertex2f(points[i], points[i + 1]);
    glEnd();
  }

  void DrawPolygon(const std::vector<float> &points, const CanvasStroke &stroke,
                   const CanvasFill *fill) override {
    if (points.size() < 6)
      return;
    if (fill) {
      ApplyFill(*fill);
      glBegin(GL_POLYGON);
      for (size_t i = 0; i + 1 < points.size(); i += 2)
        glVertex2f(points[i], points[i + 1]);
      glEnd();
    }
    ApplyStroke(stroke);
    glBegin(GL_LINE_LOOP);
    for (size_t i = 0; i + 1 < points.size(); i += 2)
      glVertex2f(points[i], points[i + 1]);
    glEnd();
  }

  void DrawRectangle(float x, float y, float w, float h,
                     const CanvasStroke &stroke,
                     const CanvasFill *fill) override {
    float x1 = x + w;
    float y1 = y + h;
    if (fill) {
      ApplyFill(*fill);
      glBegin(GL_QUADS);
      glVertex2f(x, y);
      glVertex2f(x1, y);
      glVertex2f(x1, y1);
      glVertex2f(x, y1);
      glEnd();
    }
    ApplyStroke(stroke);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x1, y);
    glVertex2f(x1, y1);
    glVertex2f(x, y1);
    glEnd();
  }

  void DrawCircle(float cx, float cy, float radius, const CanvasStroke &stroke,
                  const CanvasFill *fill) override {
    constexpr int SEGMENTS = 48;
    if (fill) {
      ApplyFill(*fill);
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(cx, cy);
      for (int i = 0; i <= SEGMENTS; ++i) {
        float angle = static_cast<float>(i) / SEGMENTS * 2.0f * 3.14159265f;
        glVertex2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
      }
      glEnd();
    }
    ApplyStroke(stroke);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < SEGMENTS; ++i) {
      float angle = static_cast<float>(i) / SEGMENTS * 2.0f * 3.14159265f;
      glVertex2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
    }
    glEnd();
  }

  void DrawText(float x, float y, const std::string &text,
                const CanvasTextStyle &style) override {
    // Text rendering is delegated to wxWidgets so we simply store the relevant
    // parameters. In the on-screen path we rely on wxGLCanvas drawing to the
    // overlay. The RecordingCanvas keeps the data for exporters.
    (void)x;
    (void)y;
    (void)text;
    (void)style;
  }

private:
  void ApplyTransform() {
    glTranslatef(m_transform.offsetX, m_transform.offsetY, 0.0f);
    glScalef(m_transform.scale, m_transform.scale, 1.0f);
  }

  CanvasTransform m_transform{};
};

class RecordingCanvas : public ICanvas2D {
public:
  RecordingCanvas(CommandBuffer &buffer, bool simplifyFootprints)
      : m_buffer(buffer), m_simplifyFootprints(simplifyFootprints) {}

  void BeginFrame() override {
    m_buffer.Clear();
    m_pendingGroup.reset();
    m_transformStack.clear();
    m_currentTransform = {};
    m_definedSymbols.clear();
    m_capturingSymbol.clear();
  }
  void EndFrame() override { FlushPendingGroup(); }

  void Save() override {
    FlushPendingGroup();
    PushCommand(SaveCommand{}, {});
    m_transformStack.push_back(m_currentTransform);
  }
  void Restore() override {
    FlushPendingGroup();
    PushCommand(RestoreCommand{}, {});
    if (!m_transformStack.empty()) {
      m_currentTransform = m_transformStack.back();
      m_transformStack.pop_back();
    }
  }
  void SetTransform(const CanvasTransform &transform) override {
    FlushPendingGroup();
    PushCommand(TransformCommand{transform}, {});
    m_currentTransform = transform;
  }

  void SetSourceKey(const std::string &key) override {
    if (m_simplifyFootprints)
      FlushPendingGroup();
    m_buffer.currentSourceKey = key.empty() ? "unknown" : key;
  }

  void DrawLine(float x0, float y0, float x1, float y1,
                const CanvasStroke &stroke) override {
    AddCommand(LineCommand{x0, y0, x1, y1, stroke},
               {stroke.width > 0.0f, false});
  }

  void DrawPolyline(const std::vector<float> &points,
                    const CanvasStroke &stroke) override {
    AddCommand(PolylineCommand{points, stroke}, {stroke.width > 0.0f, false});
  }

  void DrawPolygon(const std::vector<float> &points, const CanvasStroke &stroke,
                   const CanvasFill *fill) override {
    PolygonCommand cmd{points, stroke, {}, false};
    if (fill) {
      cmd.fill = *fill;
      cmd.hasFill = true;
    }
    AddCommand(std::move(cmd), {stroke.width > 0.0f, fill != nullptr});
  }

  void DrawRectangle(float x, float y, float w, float h,
                     const CanvasStroke &stroke,
                     const CanvasFill *fill) override {
    RectangleCommand cmd{x, y, w, h, stroke, {}, false};
    if (fill) {
      cmd.fill = *fill;
      cmd.hasFill = true;
    }
    AddCommand(std::move(cmd), {stroke.width > 0.0f, fill != nullptr});
  }

  void DrawCircle(float cx, float cy, float radius, const CanvasStroke &stroke,
                  const CanvasFill *fill) override {
    CircleCommand cmd{cx, cy, radius, stroke, {}, false};
    if (fill) {
      cmd.fill = *fill;
      cmd.hasFill = true;
    }
    AddCommand(std::move(cmd), {stroke.width > 0.0f, fill != nullptr});
  }

  void DrawText(float x, float y, const std::string &text,
                const CanvasTextStyle &style) override {
    if (m_simplifyFootprints)
      FlushPendingGroup();
    PushCommand(TextCommand{x, y, text, style}, {});
  }

  void BeginSymbol(const std::string &key) override {
    if (m_simplifyFootprints)
      FlushPendingGroup();
    m_buffer.currentSourceKey = key.empty() ? "unknown" : key;
    m_capturingSymbol = key;
  }

  void EndSymbol(const std::string &key) override {
    if (m_capturingSymbol != key)
      return;
    FlushPendingGroup();
    m_capturingSymbol.clear();
  }

  void PlaceSymbol(const std::string &key,
                   const CanvasTransform &transform) override {
    if (m_simplifyFootprints)
      FlushPendingGroup();
    PushCommand(PlaceSymbolCommand{key, transform}, {});
  }

  void PlaceSymbolInstance(uint32_t symbolId,
                           const Transform2D &transform) override {
    if (m_simplifyFootprints)
      FlushPendingGroup();
    PushCommand(SymbolInstanceCommand{symbolId, transform}, {});
  }

private:
  struct PendingGroup {
    std::string key;
    std::vector<CanvasCommand> commands;
    std::vector<CommandMetadata> metadata;
  };

  struct FootprintTemplate {
    enum class Shape { Rectangle, Circle, Hull } shape = Shape::Rectangle;
    float baseWidth = 0.0f;
    float baseHeight = 0.0f;
    float radius = 0.0f;
    std::vector<std::array<float, 2>> hull;
    CanvasStroke stroke{};
    CanvasFill fill{};
    bool hasFill = false;
    bool hasStroke = false;
  };

  void AddCommand(CanvasCommand &&cmd, const CommandMetadata &meta) {
    if (!m_simplifyFootprints) {
      PushCommand(std::move(cmd), meta);
      return;
    }

    if (!m_pendingGroup)
      m_pendingGroup = PendingGroup{m_buffer.currentSourceKey, {}, {}};

    m_pendingGroup->commands.emplace_back(std::move(cmd));
    m_pendingGroup->metadata.push_back(meta);
  }

  void PushCommand(const CanvasCommand &cmd, const CommandMetadata &meta) {
    m_buffer.commands.push_back(cmd);
    m_buffer.sources.push_back(m_buffer.currentSourceKey);
    m_buffer.metadata.push_back(meta);
  }

  void PushCommand(CanvasCommand &&cmd, const CommandMetadata &meta) {
    m_buffer.commands.emplace_back(std::move(cmd));
    m_buffer.sources.push_back(m_buffer.currentSourceKey);
    m_buffer.metadata.push_back(meta);
  }

  void PushCommandsWithSource(const std::vector<CanvasCommand> &cmds,
                              const std::vector<CommandMetadata> &meta,
                              const std::string &key) {
    std::string prevKey = m_buffer.currentSourceKey;
    m_buffer.currentSourceKey = key;
    for (size_t i = 0; i < cmds.size(); ++i) {
      PushCommand(cmds[i], meta[i]);
    }
    m_buffer.currentSourceKey = prevKey;
  }

  void FlushPendingGroup() {
    if (!m_simplifyFootprints)
      return;
    if (!m_pendingGroup || m_pendingGroup->commands.empty())
      return;

    const std::string key = m_pendingGroup->key;
    auto cmds = m_pendingGroup->commands;
    auto meta = m_pendingGroup->metadata;
    m_pendingGroup.reset();

    if (!m_capturingSymbol.empty() && m_capturingSymbol == key) {
      PushCommandsWithSource(cmds, meta, key);
      return;
    }

    auto simplified = TrySimplify(key, cmds, meta);
    bool newSymbol = m_definedSymbols.insert(key).second;
    if (newSymbol)
      PushCommand(BeginSymbolCommand{key}, {});

    if (simplified) {
      PushCommandsWithSource(simplified->first, simplified->second, key);
    } else {
      PushCommandsWithSource(cmds, meta, key);
    }

    if (newSymbol)
      PushCommand(EndSymbolCommand{key}, {});

    PushCommand(PlaceSymbolCommand{key, m_currentTransform}, {});
  }

  std::optional<std::pair<std::vector<CanvasCommand>,
                          std::vector<CommandMetadata>>>
  TrySimplify(const std::string &key, const std::vector<CanvasCommand> &cmds,
              const std::vector<CommandMetadata> &meta) {
    if (key.empty() || key == "unknown")
      return std::nullopt;

    auto points = CollectPoints(cmds);
    if (points.size() < 3)
      return std::nullopt;

    auto styles = ExtractStyles(cmds, meta);
    if (!styles.has_value())
      return std::nullopt;

    auto centroid = ComputeCentroid(points);
    float angle = ComputeOrientation(points, centroid);
    auto axis = UnitVector(angle);
    auto perp = std::array<float, 2>{-axis[1], axis[0]};

    float minAxis = std::numeric_limits<float>::max();
    float maxAxis = std::numeric_limits<float>::lowest();
    float minPerp = std::numeric_limits<float>::max();
    float maxPerp = std::numeric_limits<float>::lowest();
    for (const auto &p : points) {
      float da = Dot(p, axis);
      float dp = Dot(p, perp);
      minAxis = std::min(minAxis, da);
      maxAxis = std::max(maxAxis, da);
      minPerp = std::min(minPerp, dp);
      maxPerp = std::max(maxPerp, dp);
    }

    float width = maxAxis - minAxis;
    float height = maxPerp - minPerp;
    if (width <= 0.0f || height <= 0.0f)
      return std::nullopt;

    auto hull = ComputeHull(points);
    if (hull.size() < 3)
      return std::nullopt;
    float hullArea = std::fabs(ComputePolygonArea(hull));
    float rectArea = width * height;

    FootprintTemplate *tpl = nullptr;
    auto it = m_footprintCache.find(key);
    if (it == m_footprintCache.end()) {
      FootprintTemplate entry;
      entry.stroke = styles->stroke;
      entry.hasStroke = styles->hasStroke;
      if (styles->fill)
        entry.fill = *styles->fill;
      entry.hasFill = styles->fill.has_value();

      float aspectDiff = std::fabs(width - height) /
                         std::max(width, height);
      if (aspectDiff < 0.1f) {
        entry.shape = FootprintTemplate::Shape::Circle;
        entry.radius = (width + height) * 0.25f;
        entry.baseWidth = entry.baseHeight = std::max(width, height);
      } else if (rectArea > 0.0f && hullArea / rectArea < 0.6f) {
        entry.shape = FootprintTemplate::Shape::Hull;
        entry.baseWidth = width;
        entry.baseHeight = height;
        entry.hull = NormalizePoints(hull, centroid, angle);
      } else {
        entry.shape = FootprintTemplate::Shape::Rectangle;
        entry.baseWidth = width;
        entry.baseHeight = height;
      }
      tpl = &m_footprintCache.emplace(key, std::move(entry)).first->second;
    } else {
      tpl = &it->second;
    }

    if (!tpl)
      return std::nullopt;

    std::vector<CanvasCommand> simplified;
    std::vector<CommandMetadata> simplifiedMeta;

    switch (tpl->shape) {
    case FootprintTemplate::Shape::Circle: {
      float radius = std::max(width, height) * 0.5f;
      CircleCommand circle{centroid[0], centroid[1], radius, tpl->stroke,
                           tpl->fill, tpl->hasFill};
      simplified.emplace_back(circle);
      simplifiedMeta.push_back({tpl->hasStroke, tpl->hasFill});
      break;
    }
    case FootprintTemplate::Shape::Rectangle: {
      float hw = width * 0.5f;
      float hh = height * 0.5f;
      std::vector<float> pts = {
          -hw, -hh, hw, -hh, hw, hh, -hw, hh};
      auto rotated = RotateAndTranslate(pts, centroid, angle);
      PolygonCommand poly{rotated, tpl->stroke, tpl->fill, tpl->hasFill};
      simplified.emplace_back(std::move(poly));
      simplifiedMeta.push_back({tpl->hasStroke, tpl->hasFill});
      break;
    }
    case FootprintTemplate::Shape::Hull: {
      if (tpl->baseWidth <= 0.0f || tpl->baseHeight <= 0.0f ||
          tpl->hull.empty())
        return std::nullopt;
      float sx = width / tpl->baseWidth;
      float sy = height / tpl->baseHeight;
      if (sx <= 0.0f || sy <= 0.0f)
        return std::nullopt;
      std::vector<float> local;
      local.reserve(tpl->hull.size() * 2);
      for (const auto &p : tpl->hull) {
        local.push_back(p[0] * sx);
        local.push_back(p[1] * sy);
      }
      auto rotated = RotateAndTranslate(local, centroid, angle);
      PolygonCommand poly{rotated, tpl->stroke, tpl->fill, tpl->hasFill};
      simplified.emplace_back(std::move(poly));
      simplifiedMeta.push_back({tpl->hasStroke, tpl->hasFill});
      break;
    }
    }

    return std::make_pair(std::move(simplified), std::move(simplifiedMeta));
  }

  static std::array<float, 2>
  ComputeCentroid(const std::vector<std::array<float, 2>> &pts) {
    std::array<float, 2> c{0.0f, 0.0f};
    for (const auto &p : pts) {
      c[0] += p[0];
      c[1] += p[1];
    }
    c[0] /= static_cast<float>(pts.size());
    c[1] /= static_cast<float>(pts.size());
    return c;
  }

  static float ComputeOrientation(const std::vector<std::array<float, 2>> &pts,
                                  const std::array<float, 2> &centroid) {
    float sumXX = 0.0f, sumXY = 0.0f, sumYY = 0.0f;
    for (const auto &p : pts) {
      float dx = p[0] - centroid[0];
      float dy = p[1] - centroid[1];
      sumXX += dx * dx;
      sumXY += dx * dy;
      sumYY += dy * dy;
    }
    float a = sumXX / pts.size();
    float b = sumXY / pts.size();
    float c = sumYY / pts.size();

    float trace = a + c;
    float det = a * c - b * b;
    float angle = 0.0f;
    if (det < 1e-6f) {
      angle = 0.0f;
    } else {
      float term = std::sqrt(std::max(0.0f, (trace * trace) / 4.0f - det));
      float lambda1 = trace / 2.0f + term;
      float vx = (lambda1 - c);
      float vy = b;
      if (std::abs(vx) < 1e-6f && std::abs(vy) < 1e-6f)
        angle = 0.0f;
      else
        angle = std::atan2(vy, vx);
    }
    return angle;
  }

  static std::array<float, 2> UnitVector(float angle) {
    return {std::cos(angle), std::sin(angle)};
  }

  static float Dot(const std::array<float, 2> &p, const std::array<float, 2> &q) {
    return p[0] * q[0] + p[1] * q[1];
  }

  static std::vector<std::array<float, 2>>
  CollectPoints(const std::vector<CanvasCommand> &cmds) {
    std::vector<std::array<float, 2>> pts;
    auto addPoint = [&pts](float x, float y) { pts.push_back({x, y}); };

    for (const auto &cmd : cmds) {
      if (auto line = std::get_if<LineCommand>(&cmd)) {
        addPoint(line->x0, line->y0);
        addPoint(line->x1, line->y1);
      } else if (auto pl = std::get_if<PolylineCommand>(&cmd)) {
        for (size_t i = 0; i + 1 < pl->points.size(); i += 2)
          addPoint(pl->points[i], pl->points[i + 1]);
      } else if (auto pg = std::get_if<PolygonCommand>(&cmd)) {
        for (size_t i = 0; i + 1 < pg->points.size(); i += 2)
          addPoint(pg->points[i], pg->points[i + 1]);
      } else if (auto rc = std::get_if<RectangleCommand>(&cmd)) {
        addPoint(rc->x, rc->y);
        addPoint(rc->x + rc->w, rc->y);
        addPoint(rc->x + rc->w, rc->y + rc->h);
        addPoint(rc->x, rc->y + rc->h);
      } else if (auto cc = std::get_if<CircleCommand>(&cmd)) {
        constexpr int kSegments = 12;
        constexpr float kPi = 3.14159265358979323846f;
        for (int i = 0; i < kSegments; ++i) {
          float ang = static_cast<float>(i) / kSegments * 2.0f * kPi;
          addPoint(cc->cx + cc->radius * std::cos(ang),
                   cc->cy + cc->radius * std::sin(ang));
        }
      }
    }

    return pts;
  }

  struct StyleInfo {
    CanvasStroke stroke{};
    bool hasStroke = false;
    std::optional<CanvasFill> fill;
  };

  static std::optional<StyleInfo>
  ExtractStyles(const std::vector<CanvasCommand> &cmds,
                const std::vector<CommandMetadata> &meta) {
    for (size_t i = 0; i < cmds.size(); ++i) {
      if (auto poly = std::get_if<PolygonCommand>(&cmds[i]))
        return StyleInfo{poly->stroke, meta[i].hasStroke,
                         poly->hasFill ? std::optional(poly->fill)
                                        : std::optional<CanvasFill>()};
      if (auto rect = std::get_if<RectangleCommand>(&cmds[i]))
        return StyleInfo{rect->stroke, meta[i].hasStroke,
                         rect->hasFill ? std::optional(rect->fill)
                                       : std::optional<CanvasFill>()};
      if (auto circ = std::get_if<CircleCommand>(&cmds[i]))
        return StyleInfo{circ->stroke, meta[i].hasStroke,
                         circ->hasFill ? std::optional(circ->fill)
                                       : std::optional<CanvasFill>()};
      if (auto pl = std::get_if<PolylineCommand>(&cmds[i]))
        return StyleInfo{pl->stroke, meta[i].hasStroke,
                         std::optional<CanvasFill>()};
      if (auto ln = std::get_if<LineCommand>(&cmds[i]))
        return StyleInfo{ln->stroke, meta[i].hasStroke,
                         std::optional<CanvasFill>()};
    }
    return std::nullopt;
  }

  static std::vector<std::array<float, 2>>
  ComputeHull(std::vector<std::array<float, 2>> pts) {
    if (pts.size() < 3)
      return pts;
    std::sort(pts.begin(), pts.end(), [](const auto &a, const auto &b) {
      if (a[0] == b[0])
        return a[1] < b[1];
      return a[0] < b[0];
    });

    auto cross = [](const std::array<float, 2> &o, const std::array<float, 2> &a,
                    const std::array<float, 2> &b) {
      return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
    };

    std::vector<std::array<float, 2>> hull(pts.size() * 2);
    size_t k = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
      while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0)
        --k;
      hull[k++] = pts[i];
    }
    for (size_t i = pts.size() - 1, t = k + 1; i > 0; --i) {
      while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i - 1]) <= 0)
        --k;
      hull[k++] = pts[i - 1];
    }
    hull.resize(k ? k - 1 : 0);
    return hull;
  }

  static float ComputePolygonArea(const std::vector<std::array<float, 2>> &pts) {
    if (pts.size() < 3)
      return 0.0f;
    float area = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
      const auto &p0 = pts[i];
      const auto &p1 = pts[(i + 1) % pts.size()];
      area += p0[0] * p1[1] - p1[0] * p0[1];
    }
    return area * 0.5f;
  }

  static std::vector<std::array<float, 2>>
  NormalizePoints(const std::vector<std::array<float, 2>> &pts,
                  const std::array<float, 2> &center, float angle) {
    auto axis = UnitVector(angle);
    auto perp = std::array<float, 2>{-axis[1], axis[0]};
    std::vector<std::array<float, 2>> out;
    out.reserve(pts.size());
    for (const auto &p : pts) {
      float dx = p[0] - center[0];
      float dy = p[1] - center[1];
      out.push_back({dx * axis[0] + dy * axis[1], dx * perp[0] + dy * perp[1]});
    }
    return out;
  }

  static std::vector<float>
  RotateAndTranslate(const std::vector<float> &pts,
                     const std::array<float, 2> &center, float angle) {
    auto axis = UnitVector(angle);
    auto perp = std::array<float, 2>{-axis[1], axis[0]};
    std::vector<float> out;
    out.reserve(pts.size());
    for (size_t i = 0; i + 1 < pts.size(); i += 2) {
      float x = pts[i];
      float y = pts[i + 1];
      float rx = x * axis[0] + y * perp[0];
      float ry = x * axis[1] + y * perp[1];
      out.push_back(rx + center[0]);
      out.push_back(ry + center[1]);
    }
    return out;
  }

  CommandBuffer &m_buffer;
  bool m_simplifyFootprints = false;
  std::optional<PendingGroup> m_pendingGroup;
  std::unordered_map<std::string, FootprintTemplate> m_footprintCache;
  std::unordered_set<std::string> m_definedSymbols;
  std::vector<CanvasTransform> m_transformStack;
  CanvasTransform m_currentTransform{};
  std::string m_capturingSymbol;
};

class MultiCanvas : public ICanvas2D {
public:
  void AddCanvas(ICanvas2D *canvas) { m_canvases.push_back(canvas); }

  void BeginFrame() override {
    for (auto *c : m_canvases)
      c->BeginFrame();
  }
  void EndFrame() override {
    for (auto *c : m_canvases)
      c->EndFrame();
  }
  void Save() override {
    for (auto *c : m_canvases)
      c->Save();
  }
  void Restore() override {
    for (auto *c : m_canvases)
      c->Restore();
  }
  void SetTransform(const CanvasTransform &transform) override {
    for (auto *c : m_canvases)
      c->SetTransform(transform);
  }
  void SetSourceKey(const std::string &key) override {
    for (auto *c : m_canvases)
      c->SetSourceKey(key);
  }
  void BeginSymbol(const std::string &key) override {
    for (auto *c : m_canvases)
      c->BeginSymbol(key);
  }
  void EndSymbol(const std::string &key) override {
    for (auto *c : m_canvases)
      c->EndSymbol(key);
  }
  void PlaceSymbol(const std::string &key,
                   const CanvasTransform &transform) override {
    for (auto *c : m_canvases)
      c->PlaceSymbol(key, transform);
  }
  void PlaceSymbolInstance(uint32_t symbolId,
                           const Transform2D &transform) override {
    for (auto *c : m_canvases)
      c->PlaceSymbolInstance(symbolId, transform);
  }
  void DrawLine(float x0, float y0, float x1, float y1,
                const CanvasStroke &stroke) override {
    for (auto *c : m_canvases)
      c->DrawLine(x0, y0, x1, y1, stroke);
  }
  void DrawPolyline(const std::vector<float> &points,
                    const CanvasStroke &stroke) override {
    for (auto *c : m_canvases)
      c->DrawPolyline(points, stroke);
  }
  void DrawPolygon(const std::vector<float> &points, const CanvasStroke &stroke,
                   const CanvasFill *fill) override {
    for (auto *c : m_canvases)
      c->DrawPolygon(points, stroke, fill);
  }
  void DrawRectangle(float x, float y, float w, float h,
                     const CanvasStroke &stroke,
                     const CanvasFill *fill) override {
    for (auto *c : m_canvases)
      c->DrawRectangle(x, y, w, h, stroke, fill);
  }
  void DrawCircle(float cx, float cy, float radius, const CanvasStroke &stroke,
                  const CanvasFill *fill) override {
    for (auto *c : m_canvases)
      c->DrawCircle(cx, cy, radius, stroke, fill);
  }
  void DrawText(float x, float y, const std::string &text,
                const CanvasTextStyle &style) override {
    for (auto *c : m_canvases)
      c->DrawText(x, y, text, style);
  }

private:
  std::vector<ICanvas2D *> m_canvases;
};

std::unique_ptr<ICanvas2D> CreateRasterCanvas(const CanvasTransform &transform) {
  return std::make_unique<RasterCanvas>(transform);
}

std::unique_ptr<ICanvas2D> CreateRecordingCanvas(CommandBuffer &buffer,
                                                 bool simplifyFootprints) {
  return std::make_unique<RecordingCanvas>(buffer, simplifyFootprints);
}

std::unique_ptr<ICanvas2D> CreateMultiCanvas(
    const std::vector<ICanvas2D *> &canvases) {
  auto multi = std::make_unique<MultiCanvas>();
  for (auto *c : canvases)
    multi->AddCanvas(c);
  return multi;
}

namespace {
Transform2D ComposeTransform(const Transform2D &a, const Transform2D &b) {
  Transform2D out;
  out.a = a.a * b.a + a.c * b.b;
  out.b = a.b * b.a + a.d * b.b;
  out.c = a.a * b.c + a.c * b.d;
  out.d = a.b * b.c + a.d * b.d;
  out.tx = a.a * b.tx + a.c * b.ty + a.tx;
  out.ty = a.b * b.tx + a.d * b.ty + a.ty;
  return out;
}

SymbolPoint ApplyTransformPoint(const Transform2D &t, float x, float y) {
  return {t.a * x + t.c * y + t.tx, t.b * x + t.d * y + t.ty};
}

void ReplayCommandsWithTransform(const CommandBuffer &buffer, ICanvas2D &canvas,
                                 const Transform2D &transform,
                                 const SymbolCache *symbolCache) {
  for (const auto &cmd : buffer.commands) {
    if (const auto *line = std::get_if<LineCommand>(&cmd)) {
      auto p0 = ApplyTransformPoint(transform, line->x0, line->y0);
      auto p1 = ApplyTransformPoint(transform, line->x1, line->y1);
      canvas.DrawLine(p0.x, p0.y, p1.x, p1.y, line->stroke);
    } else if (const auto *polyline = std::get_if<PolylineCommand>(&cmd)) {
      std::vector<float> pts;
      pts.reserve(polyline->points.size());
      for (size_t i = 0; i + 1 < polyline->points.size(); i += 2) {
        auto p =
            ApplyTransformPoint(transform, polyline->points[i],
                                polyline->points[i + 1]);
        pts.push_back(p.x);
        pts.push_back(p.y);
      }
      canvas.DrawPolyline(pts, polyline->stroke);
    } else if (const auto *poly = std::get_if<PolygonCommand>(&cmd)) {
      std::vector<float> pts;
      pts.reserve(poly->points.size());
      for (size_t i = 0; i + 1 < poly->points.size(); i += 2) {
        auto p =
            ApplyTransformPoint(transform, poly->points[i], poly->points[i + 1]);
        pts.push_back(p.x);
        pts.push_back(p.y);
      }
      canvas.DrawPolygon(pts, poly->stroke, poly->hasFill ? &poly->fill : nullptr);
    } else if (const auto *rect = std::get_if<RectangleCommand>(&cmd)) {
      std::vector<float> pts = {rect->x, rect->y, rect->x + rect->w, rect->y,
                                rect->x + rect->w, rect->y + rect->h, rect->x,
                                rect->y + rect->h};
      std::vector<float> transformed;
      transformed.reserve(pts.size());
      for (size_t i = 0; i + 1 < pts.size(); i += 2) {
        auto p = ApplyTransformPoint(transform, pts[i], pts[i + 1]);
        transformed.push_back(p.x);
        transformed.push_back(p.y);
      }
      canvas.DrawPolygon(transformed, rect->stroke,
                         rect->hasFill ? &rect->fill : nullptr);
    } else if (const auto *circle = std::get_if<CircleCommand>(&cmd)) {
      auto center = ApplyTransformPoint(transform, circle->cx, circle->cy);
      float sx = std::sqrt(transform.a * transform.a + transform.b * transform.b);
      float sy = std::sqrt(transform.c * transform.c + transform.d * transform.d);
      float scale = (sx + sy) * 0.5f;
      canvas.DrawCircle(center.x, center.y, circle->radius * scale,
                        circle->stroke, circle->hasFill ? &circle->fill : nullptr);
    } else if (const auto *text = std::get_if<TextCommand>(&cmd)) {
      auto p = ApplyTransformPoint(transform, text->x, text->y);
      canvas.DrawText(p.x, p.y, text->text, text->style);
    } else if (const auto *save = std::get_if<SaveCommand>(&cmd)) {
      (void)save;
      canvas.Save();
    } else if (const auto *restore = std::get_if<RestoreCommand>(&cmd)) {
      (void)restore;
      canvas.Restore();
    } else if (const auto *tf = std::get_if<TransformCommand>(&cmd)) {
      canvas.SetTransform(tf->transform);
    } else if (const auto *begin = std::get_if<BeginSymbolCommand>(&cmd)) {
      canvas.BeginSymbol(begin->key);
    } else if (const auto *end = std::get_if<EndSymbolCommand>(&cmd)) {
      canvas.EndSymbol(end->key);
    } else if (const auto *place = std::get_if<PlaceSymbolCommand>(&cmd)) {
      canvas.PlaceSymbol(place->key, place->transform);
    } else if (const auto *instance = std::get_if<SymbolInstanceCommand>(&cmd)) {
      if (!symbolCache)
        continue;
      const auto *symbol = symbolCache->GetById(instance->symbolId);
      if (!symbol)
        continue;
      Transform2D combined = ComposeTransform(transform, instance->transform);
      ReplayCommandsWithTransform(symbol->localCommands, canvas, combined,
                                  symbolCache);
    }
  }
}
} // namespace

void ReplayCommandBuffer(const CommandBuffer &buffer, ICanvas2D &canvas,
                         const SymbolCache *symbolCache) {
  ReplayCommandsWithTransform(buffer, canvas, Transform2D::Identity(),
                              symbolCache);
}
