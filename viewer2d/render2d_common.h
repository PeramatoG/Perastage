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

#include <string>
#include <vector>

#include "canvas2d.h"
#include "symbolcache.h"

struct Viewer2DViewState;

namespace viewer2d {

constexpr double kViewer2DPixelsPerMeter = 25.0;

struct Viewer2DViewBounds {
  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
  double width = 0.0;
  double height = 0.0;
};

struct Viewer2DRenderMapping {
  double minX = 0.0;
  double minY = 0.0;
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
  double drawHeight = 0.0;
  bool flipY = true;
};

struct Viewer2DRenderPoint {
  double x = 0.0;
  double y = 0.0;
};

struct Viewer2DRenderText {
  Viewer2DRenderPoint anchor{};
  std::string text;
  CanvasTextStyle style{};
  double fontSizePx = 0.0;
  double lineHeightPx = 0.0;
  double outlineWidthPx = 0.0;
};

class IViewer2DCommandBackend {
public:
  virtual ~IViewer2DCommandBackend() = default;

  virtual void DrawLine(const Viewer2DRenderPoint &p0,
                        const Viewer2DRenderPoint &p1,
                        const CanvasStroke &stroke,
                        double strokeWidthPx) = 0;
  virtual void DrawPolyline(const std::vector<Viewer2DRenderPoint> &points,
                            const CanvasStroke &stroke,
                            double strokeWidthPx) = 0;
  virtual void DrawPolygon(const std::vector<Viewer2DRenderPoint> &points,
                           const CanvasStroke &stroke, const CanvasFill *fill,
                           double strokeWidthPx) = 0;
  virtual void DrawCircle(const Viewer2DRenderPoint &center, double radiusPx,
                          const CanvasStroke &stroke, const CanvasFill *fill,
                          double strokeWidthPx) = 0;
  virtual void DrawText(const Viewer2DRenderText &text) = 0;
};

bool ComputeViewBounds(const Viewer2DViewState &viewState,
                       Viewer2DViewBounds &bounds);

bool BuildViewMapping(const Viewer2DViewState &viewState, double targetWidth,
                      double targetHeight, double margin,
                      Viewer2DRenderMapping &mapping);

class Viewer2DCommandRenderer {
public:
  Viewer2DCommandRenderer(const Viewer2DRenderMapping &mapping,
                          IViewer2DCommandBackend &backend,
                          const SymbolDefinitionSnapshot *symbols = nullptr);

  void Render(const CommandBuffer &buffer,
              const Transform2D &localTransform = Transform2D::Identity());

private:
  void RenderInternal(const CommandBuffer &buffer,
                      const Transform2D &localTransform);

  Viewer2DRenderMapping mapping_{};
  IViewer2DCommandBackend &backend_;
  const SymbolDefinitionSnapshot *symbols_ = nullptr;
};

} // namespace viewer2d
