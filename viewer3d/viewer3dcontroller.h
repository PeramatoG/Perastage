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
 * File: viewer3dcontroller.h
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: Controller class for 3D viewer logic and state.
 */

#pragma once

#include "gdtfloader.h"
#include "mesh.h"
#include "scenedatamanager.h"
#include "canvas2d.h"
#include "symbolcache.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <wx/gdicmn.h>
#include <wx/string.h>

struct NVGcontext;

// MVR coordinates are defined in millimeters. This constant converts
// them to meters when rendering.
static constexpr float RENDER_SCALE = 0.001f;

// Rendering options for the simplified 2D top-down view
enum class Viewer2DRenderMode {
  Wireframe = 0,
  White,
  ByFixtureType,
  ByLayer
};

// Available orientations for the 2D viewer
enum class Viewer2DView {
  Top = 0,
  Front,
  Side,
  Bottom
};

class Viewer3DController {
public:
  Viewer3DController();
  ~Viewer3DController();

  // Initializes OpenGL-dependent resources (NanoVG context, fonts)
  void InitializeGL();

  // Updates the internal logic if needed (e.g. animation, camera)
  void Update();

  // Renders all scene objects. When wireframe is true lighting is disabled
  // and geometry is drawn using black lines only. The optional mode controls
  // coloring when rendering the simplified 2D view.
  // Renders all scene objects. When wireframe is true lighting is disabled
  // and geometry is drawn using black lines only. The optional mode and view
  // parameters control coloring and orientation for the simplified 2D view.
  // Grid rendering can be customized for the 2D viewer through the additional
  // parameters. The 3D viewer relies on the defaults and remains unaffected.
  void RenderScene(bool wireframe = false,
                   Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                   Viewer2DView view = Viewer2DView::Top,
                   bool showGrid = true,
                   int gridStyle = 0,
                   float gridR = 0.35f,
                   float gridG = 0.35f,
                   float gridB = 0.35f,
                   bool gridOnTop = false);

  // Fixture UUID currently highlighted (hovered)
  void SetHighlightUuid(const std::string &uuid);
  void SetSelectedUuids(const std::vector<std::string> &uuids);

  // Draws object names at their projected positions using OpenGL
  void DrawFixtureLabels(int width, int height);
  void DrawTrussLabels(int width, int height);
  void DrawSceneObjectLabels(int width, int height);
  // Renders labels for all fixtures with a uniform font size that scales
  // with the provided zoom factor. Labels have no outline and always show
  // name, ID and DMX address on three separate lines.
  void DrawAllFixtureLabels(int width, int height, float zoom = 1.0f);

  // Returns true and outputs label and screen position of the fixture
  // under the given mouse coordinates (if any)
  bool GetFixtureLabelAt(int mouseX, int mouseY, int width, int height,
                         wxString &outLabel, wxPoint &outPos,
                         std::string *outUuid = nullptr);
  bool GetTrussLabelAt(int mouseX, int mouseY, int width, int height,
                       wxString &outLabel, wxPoint &outPos,
                       std::string *outUuid = nullptr);
  bool GetSceneObjectLabelAt(int mouseX, int mouseY, int width, int height,
                             wxString &outLabel, wxPoint &outPos,
                             std::string *outUuid = nullptr);

  // Update cached layer color for rendering
  void SetLayerColor(const std::string &layer, const std::string &hex);
  const SymbolCache &GetBottomSymbolCache() const { return m_bottomSymbolCache; }
  SymbolCache &GetBottomSymbolCache() { return m_bottomSymbolCache; }
  std::shared_ptr<const SymbolDefinitionSnapshot>
  GetBottomSymbolCacheSnapshot() const;

private:
  // Draws a solid cube centered at origin with given size and color
  void DrawCube(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                float b = 1.0f);

  // Draws a wireframe cube centered at origin with given size and color.
  // Line thickness is reduced only for the pure wireframe render mode.
  void DrawWireframeCube(float size = 0.3f, float r = 1.0f, float g = 1.0f,
                         float b = 0.0f,
                         Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                         const std::function<std::array<float, 3>(
                             const std::array<float, 3> &)> &captureTransform =
                             {});

  // Draws a wireframe box with independent dimensions. Length corresponds
  // to the X axis, width to Y and height to Z. The box's origin is at the
  // left end (X = 0), centered in Y and resting on Z = 0 so its base aligns
  // with the ground. It is tinted based on selection/highlight state.
  void DrawWireframeBox(float length, float height, float width,
                        bool highlight = false, bool selected = false,
                        bool wireframe = false,
                        Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                        const std::function<std::array<float, 3>(
                            const std::array<float, 3> &)> &captureTransform =
                            {});

  // Draws a colored mesh. When selected or highlighted the mesh is tinted
  // in cyan or green. The optional center offset parameters are kept for
  // backwards compatibility but currently unused.
  void DrawMeshWithOutline(const Mesh &mesh, float r = 1.0f, float g = 1.0f,
                           float b = 1.0f, float scale = RENDER_SCALE,
                           bool highlight = false, bool selected = false,
                           float cx = 0.0f, float cy = 0.0f, float cz = 0.0f,
                           bool wireframe = false,
                           Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                           const std::function<std::array<float, 3>(
                               const std::array<float, 3> &)> &captureTransform =
                               {});

  // Draws only the mesh edges for wireframe rendering
  void DrawMeshWireframe(
      const Mesh &mesh, float scale = RENDER_SCALE,
      const std::function<std::array<float, 3>(
          const std::array<float, 3> &)> &captureTransform = {});

  // Draws a colored cube tinted when selected or highlighted. The optional
  // center offset parameters are unused and kept for compatibility.
  void DrawCubeWithOutline(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                           float b = 1.0f, bool highlight = false,
                           bool selected = false, float cx = 0.0f,
                           float cy = 0.0f, float cz = 0.0f,
                           bool wireframe = false,
                           Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                           const std::function<std::array<float, 3>(
                               const std::array<float, 3> &)> &captureTransform =
                               {});

  // Applies the object's transformation matrix. When scaleTranslation
  // is true the translation part is converted from millimeters to
  // meters using the render scale factor.
  void ApplyTransform(const float matrix[16], bool scaleTranslation = true);

  // Draws the reference grid on the Z=0 plane
  void DrawGrid(int style, float r, float g, float b,
                Viewer2DView view = Viewer2DView::Top);

  // Draws the XYZ axis lines
  void DrawAxes();

  // Initializes simple lighting for the scene
  void SetupBasicLighting();
  void SetupMaterialFromRGB(float r, float g, float b);

  // Helpers used only when recording the 2D view to a canvas command buffer.
  std::array<float, 2> ProjectToCanvas(const std::array<float, 3> &p) const;
  void RecordLine(const std::array<float, 3> &a, const std::array<float, 3> &b,
                  const CanvasStroke &stroke) const;
  void RecordPolyline(const std::vector<std::array<float, 3>> &points,
                      const CanvasStroke &stroke) const;
  void RecordPolygon(const std::vector<std::array<float, 3>> &points,
                     const CanvasStroke &stroke, const CanvasFill *fill) const;
  void RecordText(float x, float y, const std::string &text,
                  const CanvasTextStyle &style) const;

  // Draws a mesh loaded from a 3DS file using the given scale factor
  // for vertex positions. GDTF models may already be defined in meters
  // so they can use a scale of 1.0f
  void DrawMesh(const Mesh &mesh, float scale = RENDER_SCALE);

  // Cache of already loaded meshes indexed by absolute file path
  std::unordered_map<std::string, Mesh> m_loadedMeshes;

  // Cache of loaded GDTF models indexed by absolute file path
  std::unordered_map<std::string, std::vector<GdtfObject>> m_loadedGdtf;
  // Cache of failed GDTF loads with their failure reason, indexed by absolute
  // file path
  std::unordered_map<std::string, std::string> m_failedGdtfReasons;
  // Tracks the last reported fixture counts per failed GDTF path to avoid
  // repeating identical console messages every update
  std::unordered_map<std::string, size_t> m_reportedGdtfFailureCounts;
  std::unordered_map<std::string, std::string> m_reportedGdtfFailureReasons;
  std::string m_lastSceneBasePath;

  struct BoundingBox {
    std::array<float, 3> min;
    std::array<float, 3> max;
  };

  // Precomputed world-space bounds for each fixture by UUID
  std::unordered_map<std::string, BoundingBox> m_fixtureBounds;
  std::unordered_map<std::string, BoundingBox> m_trussBounds;
  std::unordered_map<std::string, BoundingBox> m_objectBounds;

  // Randomly assigned colors for fixture types and layers in 2D view
  std::unordered_map<std::string, std::array<float, 3>> m_typeColors;
  std::unordered_map<std::string, std::array<float, 3>> m_layerColors;

  // Currently highlighted fixture UUID
  std::string m_highlightUuid;
  // Currently selected fixture UUIDs
  std::unordered_set<std::string> m_selectedUuids;

  // NanoVG context used to render on-screen labels
  NVGcontext *m_vg = nullptr;
  // Font handle for label rendering
  int m_font = -1;

  // Optional capture target used by the 2D viewer to record drawing commands
  // without altering the OpenGL on-screen output. When null, no recording
  // takes place and the render path behaves as before.
  ICanvas2D *m_captureCanvas = nullptr;
  Viewer2DView m_captureView = Viewer2DView::Top;
  bool m_captureIncludeGrid = true;
  bool m_captureOnly = false;
  bool m_captureUseSymbols = false;
  SymbolCache m_bottomSymbolCache;

public:
  // Enables recording of all primitives drawn during the next RenderScene
  // call. The caller owns the canvas lifetime and is responsible for calling
  // BeginFrame/EndFrame. Recording is disabled automatically after each
  // render pass by resetting the canvas to nullptr.
  void SetCaptureCanvas(ICanvas2D *canvas, Viewer2DView view,
                        bool includeGrid = true,
                        bool useSymbolInstancing = false) {
    m_captureCanvas = canvas;
    m_captureView = view;
    m_captureIncludeGrid = includeGrid;
    m_captureUseSymbols = canvas ? useSymbolInstancing : false;
  }
};
