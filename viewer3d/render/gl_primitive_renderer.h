#pragma once

#include "canvas2d.h"
#include "viewer3d_types.h"

#include <array>
#include <functional>
#include <vector>

namespace GLPrimitiveRenderer {

using CaptureTransform =
    std::function<std::array<float, 3>(const std::array<float, 3> &)>;
using SetColorFn = std::function<void(float, float, float)>;
using RecordLineFn = std::function<void(const std::array<float, 3> &,
                                        const std::array<float, 3> &,
                                        const CanvasStroke &)>;
using RecordPolygonFn = std::function<void(
    const std::vector<std::array<float, 3>> &, const CanvasStroke &,
    const CanvasFill *)>;

void DrawCube(float size, float r, float g, float b, bool captureOnly,
              const SetColorFn &setColor);

void DrawWireframeCube(float size, float r, float g, float b,
                       Viewer2DRenderMode mode, const CaptureTransform &captureTransform,
                       float lineWidth, float lineWidthOverride,
                       bool recordCapture, bool captureOnly, bool captureCanvas,
                       const SetColorFn &setColor, const RecordLineFn &recordLine);

void DrawWireframeBox(float length, float height, float width, bool highlight,
                      bool selected, bool wireframe, Viewer2DRenderMode mode,
                      const CaptureTransform &captureTransform,
                      bool skipOutlinesForCurrentFrame,
                      bool showSelectionOutline2D, bool captureOnly,
                      bool captureCanvas, float lineWidth,
                      const SetColorFn &setColor,
                      const RecordLineFn &recordLine);

void DrawCubeWithOutline(float size, float r, float g, float b, bool highlight,
                         bool selected, bool wireframe,
                         Viewer2DRenderMode mode,
                         const CaptureTransform &captureTransform,
                         bool skipOutlinesForCurrentFrame,
                         bool showSelectionOutline2D, bool captureOnly,
                         bool captureCanvas, float lineWidth,
                         const SetColorFn &setColor,
                         const RecordLineFn &recordLine,
                         const RecordPolygonFn &recordPolygon);

} // namespace GLPrimitiveRenderer
