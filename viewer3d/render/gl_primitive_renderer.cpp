#include "gl_primitive_renderer.h"

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
#else
#include <GL/gl.h>
#endif

namespace GLPrimitiveRenderer {

namespace {
constexpr float kPrimaryHighlightR = 0.78f;
constexpr float kPrimaryHighlightG = 1.0f;
constexpr float kPrimaryHighlightB = 0.0f;

constexpr float kGroupHighlightR = 0.25f;
constexpr float kGroupHighlightG = 0.78f;
constexpr float kGroupHighlightB = 0.55f;

void DrawBoxEdges(float x0, float x1, float y0, float y1, float z0, float z1) {
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

void RecordBoxEdges(float x0, float x1, float y0, float y1, float z0, float z1,
                    const CaptureTransform &captureTransform,
                    const CanvasStroke &stroke,
                    const RecordLineFn &recordLine) {
  std::vector<std::array<float, 3>> verts = {
      {x0, y0, z0}, {x1, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
      {x0, y0, z1}, {x1, y0, z1}, {x0, y1, z1}, {x1, y1, z1}};
  if (captureTransform) {
    for (auto &p : verts)
      p = captureTransform(p);
  }
  const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                            {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (auto &e : edges)
    recordLine(verts[e[0]], verts[e[1]], stroke);
}

} // namespace

void DrawCube(float size, float r, float g, float b, bool captureOnly,
              const SetColorFn &setColor) {
  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

  if (!captureOnly) {
    setColor(r, g, b);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(x1, y0, z0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z0);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y1, z0);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z1);
    glEnd();
  }
}

void DrawWireframeCube(float size, float r, float g, float b,
                       Viewer2DRenderMode mode,
                       const CaptureTransform &captureTransform,
                       float lineWidth, float lineWidthOverride,
                       bool recordCapture, bool captureOnly, bool captureCanvas,
                       const SetColorFn &setColor,
                       const RecordLineFn &recordLine) {
  float half = size / 2.0f;
  float x0 = -half, x1 = half;
  float y0 = -half, y1 = half;
  float z0 = -half, z1 = half;

  if (lineWidthOverride > 0.0f)
    lineWidth = lineWidthOverride;
  if (!captureOnly) {
    glLineWidth(lineWidth);
    setColor(r, g, b);
    DrawBoxEdges(x0, x1, y0, y1, z0, z1);
  }
  CanvasStroke stroke;
  stroke.color = {r, g, b, 1.0f};
  stroke.width = lineWidth;
  if (captureCanvas && recordCapture)
    RecordBoxEdges(x0, x1, y0, y1, z0, z1, captureTransform, stroke,
                   recordLine);

  glLineWidth(1.0f);
  if (mode != Viewer2DRenderMode::Wireframe) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    setColor(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void DrawWireframeBox(float length, float height, float width, float r, float g,
                      float b, bool highlight, bool groupHighlight,
                      bool selected, bool wireframe, Viewer2DRenderMode mode,
                      const CaptureTransform &captureTransform,
                      bool skipOutlinesForCurrentFrame,
                      bool showSelectionOutline2D, bool captureOnly,
                      bool captureCanvas, float lineWidth,
                      const SetColorFn &setColor,
                      const RecordLineFn &recordLine) {
  float x0 = 0.0f, x1 = length;
  float y0 = -width * 0.5f, y1 = width * 0.5f;
  float z0 = 0.0f, z1 = height;

  if (wireframe) {
    const bool useWireframeModeColor = mode == Viewer2DRenderMode::Wireframe;
    const float strokeR = useWireframeModeColor ? r : 0.0f;
    const float strokeG = useWireframeModeColor ? g : 0.0f;
    const float strokeB = useWireframeModeColor ? b : 0.0f;
    const bool drawOutline = !skipOutlinesForCurrentFrame &&
                             showSelectionOutline2D &&
                             (highlight || groupHighlight || selected);
    if (!captureOnly) {
      if (drawOutline) {
        float glowWidth = lineWidth + 3.0f;
        glLineWidth(glowWidth);
        if (highlight)
          setColor(kPrimaryHighlightR, kPrimaryHighlightG, kPrimaryHighlightB);
        else if (groupHighlight)
          setColor(kGroupHighlightR, kGroupHighlightG, kGroupHighlightB);
        else if (selected)
          setColor(0.0f, 1.0f, 1.0f);
        DrawBoxEdges(x0, x1, y0, y1, z0, z1);
      }
      glLineWidth(lineWidth);
      setColor(strokeR, strokeG, strokeB);
      DrawBoxEdges(x0, x1, y0, y1, z0, z1);
    }
    CanvasStroke stroke;
    stroke.color = {strokeR, strokeG, strokeB, 1.0f};
    stroke.width = lineWidth;
    if (captureCanvas)
      RecordBoxEdges(x0, x1, y0, y1, z0, z1, CaptureTransform{}, stroke,
                     recordLine);
    if (!captureOnly) {
      glLineWidth(1.0f);
      if (mode != Viewer2DRenderMode::Wireframe) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        setColor(1.0f, 1.0f, 1.0f);
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
  } else if (!captureOnly) {
    if (highlight)
      setColor(kPrimaryHighlightR, kPrimaryHighlightG, kPrimaryHighlightB);
    else if (groupHighlight)
      setColor(kGroupHighlightR, kGroupHighlightG, kGroupHighlightB);
    else if (selected)
      setColor(0.0f, 1.0f, 1.0f);
    else
      setColor(1.0f, 1.0f, 0.0f);
  }

  CanvasStroke stroke;
  stroke.width = 1.0f;
  if (highlight)
    stroke.color = {kPrimaryHighlightR, kPrimaryHighlightG, kPrimaryHighlightB, 1.0f};
  else if (groupHighlight)
    stroke.color = {kGroupHighlightR, kGroupHighlightG, kGroupHighlightB, 1.0f};
  else if (selected)
    stroke.color = {0.0f, 1.0f, 1.0f, 1.0f};
  else
    stroke.color = {1.0f, 1.0f, 0.0f, 1.0f};

  if (!captureOnly)
    DrawBoxEdges(x0, x1, y0, y1, z0, z1);
  if (captureCanvas)
    RecordBoxEdges(x0, x1, y0, y1, z0, z1, captureTransform, stroke,
                   recordLine);
}

void DrawCubeWithOutline(
    float size, float r, float g, float b, bool highlight, bool groupHighlight,
    bool selected, bool wireframe, Viewer2DRenderMode mode,
    const CaptureTransform &captureTransform, bool skipOutlinesForCurrentFrame,
    bool showSelectionOutline2D, bool captureOnly, bool captureCanvas,
    float lineWidth, const SetColorFn &setColor, const RecordLineFn &recordLine,
    const RecordPolygonFn &recordPolygon) {
  if (wireframe) {
    if (mode == Viewer2DRenderMode::Wireframe) {
      const bool drawOutline = !skipOutlinesForCurrentFrame &&
                               showSelectionOutline2D &&
                               (highlight || groupHighlight || selected);
      float baseWidth = 1.0f;
      if (!captureOnly && drawOutline) {
        float glowWidth = baseWidth + 3.0f;
        if (highlight)
          DrawWireframeCube(size, kPrimaryHighlightR, kPrimaryHighlightG,
                            kPrimaryHighlightB, mode, captureTransform,
                            lineWidth, glowWidth, false, captureOnly,
                            captureCanvas, setColor, recordLine);
        else if (groupHighlight)
          DrawWireframeCube(size, kGroupHighlightR, kGroupHighlightG,
                            kGroupHighlightB, mode, captureTransform, lineWidth,
                            glowWidth, false, captureOnly, captureCanvas,
                            setColor, recordLine);
        else if (selected)
          DrawWireframeCube(size, 0.0f, 1.0f, 1.0f, mode, captureTransform,
                            lineWidth, glowWidth, false, captureOnly,
                            captureCanvas, setColor, recordLine);
      }
      DrawWireframeCube(size, r, g, b, mode, captureTransform, lineWidth, -1.0f,
                        true, captureOnly, captureCanvas, setColor, recordLine);
      return;
    }
    const bool drawOutline = !skipOutlinesForCurrentFrame &&
                             showSelectionOutline2D &&
                             (highlight || groupHighlight || selected);
    float baseWidth = 2.0f;
    if (!captureOnly && drawOutline) {
      float glowWidth = baseWidth + 3.0f;
      if (highlight)
        DrawWireframeCube(size, kPrimaryHighlightR, kPrimaryHighlightG,
                          kPrimaryHighlightB, mode, captureTransform, lineWidth,
                          glowWidth, false, captureOnly, captureCanvas,
                          setColor, recordLine);
      else if (groupHighlight)
        DrawWireframeCube(size, kGroupHighlightR, kGroupHighlightG,
                          kGroupHighlightB, mode, captureTransform, lineWidth,
                          glowWidth, false, captureOnly, captureCanvas,
                          setColor, recordLine);
      else if (selected)
        DrawWireframeCube(size, 0.0f, 1.0f, 1.0f, mode, captureTransform,
                          lineWidth, glowWidth, false, captureOnly,
                          captureCanvas, setColor, recordLine);
    }
    DrawWireframeCube(size, 0.0f, 0.0f, 0.0f, mode, captureTransform, lineWidth,
                      -1.0f, true, captureOnly, captureCanvas, setColor,
                      recordLine);
    if (captureCanvas) {
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
      CanvasStroke stroke;
      // Keep captured stroke color aligned with the primitive color so PDF
      // export preserves the pre-wireframe-color-change appearance.
      stroke.color = {r, g, b, 1.0f};
      stroke.width = lineWidth;
      CanvasFill fill;
      fill.color = {r, g, b, 1.0f};
      const int faces[6][4] = {{0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4},
                               {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}};
      for (const auto &face : faces) {
        std::vector<std::array<float, 3>> pts = {
            verts[face[0]], verts[face[1]], verts[face[2]], verts[face[3]]};
        recordPolygon(pts, stroke, &fill);
      }
    }
    if (!captureOnly) {
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(-1.0f, -1.0f);
      setColor(r, g, b);
      DrawCube(size, r, g, b, captureOnly, setColor);
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
    return;
  }

  if (highlight)
    DrawCube(size, kPrimaryHighlightR, kPrimaryHighlightG, kPrimaryHighlightB,
             captureOnly, setColor);
  else if (groupHighlight)
    DrawCube(size, kGroupHighlightR, kGroupHighlightG, kGroupHighlightB,
             captureOnly, setColor);
  else if (selected)
    DrawCube(size, 0.0f, 1.0f, 1.0f, captureOnly, setColor);
  else
    DrawCube(size, r, g, b, captureOnly, setColor);
}

} // namespace GLPrimitiveRenderer
