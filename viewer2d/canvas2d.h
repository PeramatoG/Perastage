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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// Simple RGBA color container expressed in floating point values.
struct CanvasColor {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
};

// Basic line style description shared by commands that involve strokes.
struct CanvasStroke {
  CanvasColor color{};
  float width = 1.0f; // Width expressed in the same logical units as the scene
};

// Fill style used by polygons, rectangles and circles.
struct CanvasFill {
  CanvasColor color{};
};

// Describes text appearance. Alignment flags follow the conventional meaning
// of left/center/right for horizontal alignment and baseline for vertical
// positioning. The coordinate passed to text commands is the anchor point that
// respects these alignment hints.
struct CanvasTextStyle {
  std::string fontFamily;
  float fontSize = 12.0f;
  // Optional font metrics measured at capture time (expressed in the same
  // logical units as the scene). When provided they allow exporters to align
  // text using the exact ascender/descender reported by the live renderer
  // instead of relying on generic font constants.
  float ascent = 0.0f;
  float descent = 0.0f;
  float lineHeight = 0.0f;
  float extraLineSpacing = 0.0f;
  CanvasColor color{};
  CanvasColor outlineColor{};
  float outlineWidth = 0.0f;
  enum class HorizontalAlign { Left, Center, Right } hAlign =
      HorizontalAlign::Left;
  enum class VerticalAlign { Baseline, Middle, Top, Bottom } vAlign =
      VerticalAlign::Baseline;
};

// Represents an orthographic transform used by the 2D viewer to convert from
// world coordinates (already projected to a 2D plane) into the logical canvas
// space. This is intentionally simple so export backends can reproduce the same
// mapping without depending on wxWidgets or OpenGL specific matrices.
struct CanvasTransform {
  float scale = 1.0f;  // Uniform scale applied after the camera zoom
  float offsetX = 0.0f; // Translation applied after scaling
  float offsetY = 0.0f;
};

// Simple 2D affine transform expressed as a 2x3 matrix. This is intended to be
// reusable by commands that need full control over rotation, scaling and
// translation in a single structure.
struct Transform2D {
  float a = 1.0f;
  float b = 0.0f;
  float c = 0.0f;
  float d = 1.0f;
  float tx = 0.0f;
  float ty = 0.0f;

  static Transform2D Identity() { return Transform2D{}; }
};

class SymbolCache;

// Abstract interface representing a 2D drawing surface. The coordinate space
// is always the logical world space used by the 2D viewer after applying the
// active view orientation and camera transform. Implementations may draw on
// screen, record commands, or forward calls elsewhere.
class ICanvas2D {
public:
  virtual ~ICanvas2D() = default;

  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;

  virtual void Save() = 0;
  virtual void Restore() = 0;
  virtual void SetTransform(const CanvasTransform &transform) = 0;
  virtual void SetSourceKey(const std::string &key) = 0;
  virtual void BeginSymbol(const std::string &key) = 0;
  virtual void EndSymbol(const std::string &key) = 0;
  virtual void PlaceSymbol(const std::string &key,
                           const CanvasTransform &transform) = 0;
  virtual void PlaceSymbolInstance(uint32_t symbolId,
                                   const Transform2D &transform) = 0;

  virtual void DrawLine(float x0, float y0, float x1, float y1,
                        const CanvasStroke &stroke) = 0;
  virtual void DrawPolyline(const std::vector<float> &points,
                            const CanvasStroke &stroke) = 0;
  virtual void DrawPolygon(const std::vector<float> &points,
                           const CanvasStroke &stroke,
                           const CanvasFill *fill) = 0;
  virtual void DrawRectangle(float x, float y, float w, float h,
                             const CanvasStroke &stroke,
                             const CanvasFill *fill) = 0;
  virtual void DrawCircle(float cx, float cy, float radius,
                          const CanvasStroke &stroke,
                          const CanvasFill *fill) = 0;
  virtual void DrawText(float x, float y, const std::string &text,
                        const CanvasTextStyle &style) = 0;
};

// Command types used by the RecordingCanvas. Each command stores all required
// data to reproduce the drawing in the same coordinate space used by the 2D
// viewer. Exporters can iterate the buffer in order to rebuild the scene on a
// vector backend.
struct LineCommand {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
  CanvasStroke stroke{};
};

struct PolylineCommand {
  std::vector<float> points;
  CanvasStroke stroke{};
};

struct PolygonCommand {
  std::vector<float> points;
  CanvasStroke stroke{};
  CanvasFill fill{};
  bool hasFill = false;
};

struct RectangleCommand {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
  CanvasStroke stroke{};
  CanvasFill fill{};
  bool hasFill = false;
};

struct CircleCommand {
  float cx = 0.0f;
  float cy = 0.0f;
  float radius = 0.0f;
  CanvasStroke stroke{};
  CanvasFill fill{};
  bool hasFill = false;
};

struct TextCommand {
  float x = 0.0f;
  float y = 0.0f;
  std::string text;
  CanvasTextStyle style{};
};

struct CommandMetadata {
  bool hasStroke = false;
  bool hasFill = false;
};

struct SaveCommand {};
struct RestoreCommand {};
struct TransformCommand { CanvasTransform transform; };
struct BeginSymbolCommand {
  std::string key;
};
struct EndSymbolCommand {
  std::string key;
};
struct PlaceSymbolCommand {
  std::string key;
  CanvasTransform transform{};
};
struct SymbolInstanceCommand {
  uint32_t symbolId = 0;
  Transform2D transform = Transform2D::Identity();
  // TODO: Add style overrides when cached symbol instances need them.
};

using CanvasCommand =
    std::variant<LineCommand, PolylineCommand, PolygonCommand,
                 RectangleCommand, CircleCommand, TextCommand, SaveCommand,
                 RestoreCommand, TransformCommand, BeginSymbolCommand,
                 EndSymbolCommand, PlaceSymbolCommand, SymbolInstanceCommand>;

// Container preserving the order of issued drawing commands. It is deliberately
// lightweight so it can be handed over to future SVG/PDF/printing code without
// pulling in rendering dependencies.
struct CommandBuffer {
  std::vector<CanvasCommand> commands;
  std::vector<std::string> sources;
  std::vector<CommandMetadata> metadata;

  std::string currentSourceKey = "unknown";

  void Clear() {
    commands.clear();
    sources.clear();
    metadata.clear();
    currentSourceKey = "unknown";
  }
};

// Factory helpers implemented in canvas2d.cpp so callers do not need to know
// the concrete canvas classes.
std::unique_ptr<ICanvas2D> CreateRasterCanvas(const CanvasTransform &transform);
std::unique_ptr<ICanvas2D> CreateRecordingCanvas(CommandBuffer &buffer,
                                                 bool simplifyFootprints =
                                                     false);
std::unique_ptr<ICanvas2D> CreateMultiCanvas(
    const std::vector<ICanvas2D *> &canvases);

void ReplayCommandBuffer(const CommandBuffer &buffer, ICanvas2D &canvas,
                         const SymbolCache *symbolCache = nullptr);
