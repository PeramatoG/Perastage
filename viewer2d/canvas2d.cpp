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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/gl.h>
#include <wx/glcanvas.h>
#include <cmath>

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
  explicit RecordingCanvas(CommandBuffer &buffer) : m_buffer(buffer) {}

  void BeginFrame() override { m_buffer.commands.clear(); }
  void EndFrame() override {}

  void Save() override { m_buffer.commands.emplace_back(SaveCommand{}); }
  void Restore() override { m_buffer.commands.emplace_back(RestoreCommand{}); }
  void SetTransform(const CanvasTransform &transform) override {
    m_buffer.commands.emplace_back(TransformCommand{transform});
  }

  void DrawLine(float x0, float y0, float x1, float y1,
                const CanvasStroke &stroke) override {
    m_buffer.commands.emplace_back(LineCommand{x0, y0, x1, y1, stroke});
  }

  void DrawPolyline(const std::vector<float> &points,
                    const CanvasStroke &stroke) override {
    m_buffer.commands.emplace_back(PolylineCommand{points, stroke});
  }

  void DrawPolygon(const std::vector<float> &points, const CanvasStroke &stroke,
                   const CanvasFill *fill) override {
    PolygonCommand cmd{points, stroke, {}, false};
    if (fill) {
      cmd.fill = *fill;
      cmd.hasFill = true;
    }
    m_buffer.commands.emplace_back(std::move(cmd));
  }

  void DrawRectangle(float x, float y, float w, float h,
                     const CanvasStroke &stroke,
                     const CanvasFill *fill) override {
    RectangleCommand cmd{x, y, w, h, stroke, {}, false};
    if (fill) {
      cmd.fill = *fill;
      cmd.hasFill = true;
    }
    m_buffer.commands.emplace_back(std::move(cmd));
  }

  void DrawCircle(float cx, float cy, float radius, const CanvasStroke &stroke,
                  const CanvasFill *fill) override {
    CircleCommand cmd{cx, cy, radius, stroke, {}, false};
    if (fill) {
      cmd.fill = *fill;
      cmd.hasFill = true;
    }
    m_buffer.commands.emplace_back(std::move(cmd));
  }

  void DrawText(float x, float y, const std::string &text,
                const CanvasTextStyle &style) override {
    m_buffer.commands.emplace_back(TextCommand{x, y, text, style});
  }

private:
  CommandBuffer &m_buffer;
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

std::unique_ptr<ICanvas2D> CreateRecordingCanvas(CommandBuffer &buffer) {
  return std::make_unique<RecordingCanvas>(buffer);
}

std::unique_ptr<ICanvas2D> CreateMultiCanvas(
    const std::vector<ICanvas2D *> &canvases) {
  auto multi = std::make_unique<MultiCanvas>();
  for (auto *c : canvases)
    multi->AddCanvas(c);
  return multi;
}

