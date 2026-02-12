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

#include "scenedatamanager.h"
#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer3d_types.h"
#include "irendercontext.h"
#include "iselectioncontext.h"
#include "ivisibilitycontext.h"
#include "resource_sync_system.h"
#include <array>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wx/gdicmn.h>
#include <wx/string.h>

struct NVGcontext;
class Mesh;

class SceneRenderer;
class VisibilitySystem;
class SelectionSystem;
class LabelRenderSystem;
class OpaqueFixturePass;
class OpaqueTrussPass;
class OpaqueObjectPass;

class Viewer3DController : public IRenderContext,
                           public ISelectionContext,
                           public IVisibilityContext {
public:
  using ItemType = Viewer3DItemType;
  using VisibleSet = Viewer3DVisibleSet;
  using ViewFrustumSnapshot = Viewer3DViewFrustumSnapshot;
  using BoundingBox = Viewer3DBoundingBox;

  Viewer3DController();
  ~Viewer3DController();

  // Initializes OpenGL-dependent resources (NanoVG context, fonts)
  void InitializeGL();

  // Updates the internal logic if needed (e.g. animation, camera)
  void Update();
  void UpdateResourcesIfDirty();
  void UpdateFrameStateLightweight();
  void ResetDebugPerFrameCounters();
  int GetDebugUpdateResourcesCallsPerFrame() const;

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
                   bool gridOnTop = false,
                   bool is2DViewer = false);

  // Enables color tweaks for dark mode in the 2D viewer only.
  void SetDarkMode(bool enabled);
  void SetInteracting(bool interacting);
  void SetCameraMoving(bool moving);
  bool IsCameraMoving() const override;
  void SetSelectionOutlineEnabled(bool enabled) {
    m_showSelectionOutline2D = enabled;
  }

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
  void DrawAllFixtureLabels(int width, int height, Viewer2DView view,
                            float zoom = 1.0f);

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

  // Selection/culling query helpers used by subsystem blocks. They expose
  // read-only access and keep Viewer3DController as façade/orchestrator.
  void ApplyHighlightUuid(const std::string &uuid) override;
  void ReplaceSelectedUuids(const std::vector<std::string> &uuids) override;
  const BoundingBox *FindFixtureBounds(const std::string &uuid) const override;
  const BoundingBox *FindTrussBounds(const std::string &uuid) const override;
  const BoundingBox *FindObjectBounds(const std::string &uuid) const override;

  const VisibleSet &
  GetVisibleSet(const ViewFrustumSnapshot &frustum,
                const std::unordered_set<std::string> &hiddenLayers,
                bool useFrustumCulling, float minPixels) const override;

  void RebuildVisibleSetCache();
  std::vector<std::string> GetFixturesInScreenRect(int x1, int y1, int x2,
                                                   int y2, int width,
                                                   int height) const;
  std::vector<std::string> GetTrussesInScreenRect(int x1, int y1, int x2,
                                                  int y2, int width,
                                                  int height) const;
  std::vector<std::string> GetSceneObjectsInScreenRect(int x1, int y1, int x2,
                                                       int y2, int width,
                                                       int height) const;

  // Update cached layer color for rendering
  void SetLayerColor(const std::string &layer, const std::string &hex);
  const SymbolCache &GetBottomSymbolCache() const { return m_bottomSymbolCache; }
  SymbolCache &GetBottomSymbolCache() { return m_bottomSymbolCache; }
  std::shared_ptr<const SymbolDefinitionSnapshot>
  GetBottomSymbolCacheSnapshot() const;

  // Internal render pipeline API. Kept explicit so RenderPipeline does not
  // need friend access.
  const VisibleSet &PrepareRenderFrame(const RenderFrameContext &context,
                                       ViewFrustumSnapshot &frustum);
  void RenderOpaqueFrame(const RenderFrameContext &context,
                         const VisibleSet &visibleSet);
  void RenderOverlayFrame(const RenderFrameContext &context,
                          const VisibleSet &visibleSet);

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
                             {},
                         float lineWidthOverride = -1.0f,
                         bool recordCapture = true);

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
                               {},
                           bool unlit = false,
                           const float *modelMatrix = nullptr);

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
  void SetGLColor(float r, float g, float b) const override;
  std::array<float, 3> AdjustColor(float r, float g, float b) const;

  // Helpers used only when recording the 2D view to a canvas command buffer.
  std::array<float, 2> ProjectToCanvas(const std::array<float, 3> &p) const;
  void RecordLine(const std::array<float, 3> &a, const std::array<float, 3> &b,
                  const CanvasStroke &stroke) const override;
  void RecordPolyline(const std::vector<std::array<float, 3>> &points,
                      const CanvasStroke &stroke) const;
  void RecordPolygon(const std::vector<std::array<float, 3>> &points,
                     const CanvasStroke &stroke,
                     const CanvasFill *fill) const override;
  void RecordText(float x, float y, const std::string &text,
                  const CanvasTextStyle &style) const override;

  // Creates VAO/VBO/EBO objects for the provided mesh. The function keeps
  // the triangle EBO bound in the VAO to avoid losing indexed draw state.
  void SetupMeshBuffers(Mesh &mesh);

  // Releases all OpenGL resources owned by the mesh and clears buffer flags.
  void ReleaseMeshBuffers(Mesh &mesh);

  // Draws a mesh loaded from a 3DS file using the given scale factor
  // for vertex positions. GDTF models may already be defined in meters
  // so they can use a scale of 1.0f
  void DrawMesh(const Mesh &mesh, float scale = RENDER_SCALE,
                const float *modelMatrix = nullptr);

  // Resource synchronization state (loaded assets + resolution caches).
  ResourceSyncState m_resourceSyncState;

  // Cache of local-space model bounds keyed by resolved source path.
  std::unordered_map<std::string, BoundingBox> m_modelBounds;

  // Scene versioning used to skip expensive bounds recomputation when only
  // the camera changes.
  size_t m_sceneVersion = 0;
  size_t m_cachedVersion = static_cast<size_t>(-1);
  bool m_sceneChangedDirty = true;
  bool m_assetsChangedDirty = true;
  bool m_visibilityChangedDirty = true;

  // Precomputed world-space bounds for each fixture by UUID
  std::unordered_map<std::string, BoundingBox> m_fixtureBounds;
  std::unordered_map<std::string, BoundingBox> m_trussBounds;
  std::unordered_map<std::string, BoundingBox> m_objectBounds;
  std::unordered_set<std::string> m_boundsHiddenLayers;

  // Cached draw order to avoid re-sorting unchanged scenes every frame.
  std::vector<const std::pair<const std::string, Fixture> *> m_sortedFixtures;
  std::vector<const std::pair<const std::string, Truss> *> m_sortedTrusses;
  std::vector<const std::pair<const std::string, SceneObject> *>
      m_sortedObjects;
  // Visible-only variants of sorted lists. They are rebuilt whenever sorted
  // lists change (scene update) or hidden layer visibility changes so the
  // render loop avoids per-item visibility checks every frame.
  std::vector<const std::pair<const std::string, Fixture> *>
      m_visibleSortedFixtures;
  std::vector<const std::pair<const std::string, Truss> *> m_visibleSortedTrusses;
  std::vector<const std::pair<const std::string, SceneObject> *>
      m_visibleSortedObjects;
  std::unordered_set<std::string> m_lastHiddenLayers;
  size_t m_hiddenLayersVersion = 0;
  bool m_sortedListsDirty = true;
  mutable std::mutex m_sortedListsMutex;

  // Cached deterministic colors for fixture types and layers in 2D view
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
  int m_fontBold = -1;

  // Optional capture target used by the 2D viewer to record drawing commands
  // without altering the OpenGL on-screen output. When null, no recording
  // takes place and the render path behaves as before.
  ICanvas2D *m_captureCanvas = nullptr;
  Viewer2DView m_captureView = Viewer2DView::Top;
  bool m_captureIncludeGrid = true;
  bool m_captureOnly = false;
  bool m_captureUseSymbols = false;
  SymbolCache m_bottomSymbolCache;
  bool m_darkMode = false;
  bool m_showSelectionOutline2D = false;
  bool m_isInteracting = false;
  bool m_cameraMoving = false;
  bool m_useAdaptiveLineProfile = true;
  bool m_skipOutlinesForCurrentFrame = false;
  int m_updateResourcesCallsPerFrame = 0;

  friend class OpaqueFixturePass;
  friend class OpaqueTrussPass;
  friend class OpaqueObjectPass;


  bool TryBuildLayerVisibleCandidates(
      const std::unordered_set<std::string> &hiddenLayers,
      VisibleSet &out) const;
  bool TryBuildVisibleSet(const ViewFrustumSnapshot &frustum,
                          bool useFrustumCulling, float minPixels,
                          const VisibleSet &layerVisibleCandidates,
                          VisibleSet &out) const;

  bool EnsureBoundsComputed(const std::string &uuid, ItemType type,
                            const std::unordered_set<std::string> &hiddenLayers);


  mutable VisibleSet m_cachedVisibleSet;
  mutable VisibleSet m_cachedLayerVisibleCandidates;
  mutable size_t m_layerVisibleCandidatesSceneVersion = static_cast<size_t>(-1);
  mutable std::unordered_set<std::string> m_layerVisibleCandidatesHiddenLayers;
  mutable size_t m_layerVisibleCandidatesRevision = 0;
  mutable size_t m_visibleSetLayerCandidatesRevision = static_cast<size_t>(-1);
  mutable bool m_visibleSetFrustumCulling = false;
  mutable float m_visibleSetMinPixels = -1.0f;
  mutable std::array<int, 4> m_visibleSetViewport = {0, 0, 0, 0};
  mutable std::array<double, 16> m_visibleSetModel = {};
  mutable std::array<double, 16> m_visibleSetProjection = {};

  std::unique_ptr<SceneRenderer> m_sceneRenderer;
  std::unique_ptr<VisibilitySystem> m_visibilitySystem;
  std::unique_ptr<SelectionSystem> m_selectionSystem;
  std::unique_ptr<LabelRenderSystem> m_labelRenderSystem;

  // IRenderContext implementation
  bool IsInteracting() const override;
  bool UseAdaptiveLineProfile() const override;
  bool SkipOutlinesForCurrentFrame() const override;
  bool IsSelectionOutlineEnabled2D() const override;
  bool IsCaptureOnly() const override;
  ICanvas2D *GetCaptureCanvas() const override;
  bool CaptureIncludesGrid() const override;

  // ISelectionContext implementation
  const std::string &GetHighlightUuid() const override;
  const std::unordered_map<std::string, BoundingBox> &
  GetFixtureBoundsMap() const override;
  const std::unordered_map<std::string, BoundingBox> &
  GetTrussBoundsMap() const override;
  const std::unordered_map<std::string, BoundingBox> &
  GetObjectBoundsMap() const override;
  NVGcontext *GetNanoVGContext() const override;
  int GetLabelFont() const override;
  int GetLabelBoldFont() const override;
  bool IsDarkMode() const override;

  // IVisibilityContext implementation
  ResourceSyncState &GetResourceSyncState() override;
  std::unordered_map<std::string, BoundingBox> &GetModelBounds() override;
  std::unordered_map<std::string, BoundingBox> &GetFixtureBounds() override;
  std::unordered_map<std::string, BoundingBox> &GetTrussBounds() override;
  std::unordered_map<std::string, BoundingBox> &GetObjectBounds() override;
  size_t GetSceneVersion() const override;
  const std::vector<const std::pair<const std::string, Fixture> *> &
  GetSortedFixtures() const override;
  const std::vector<const std::pair<const std::string, Truss> *> &
  GetSortedTrusses() const override;
  const std::vector<const std::pair<const std::string, SceneObject> *> &
  GetSortedObjects() const override;
  std::mutex &GetSortedListsMutex() const override;
  VisibleSet &GetCachedVisibleSet() const override;
  VisibleSet &GetCachedLayerVisibleCandidates() const override;
  size_t &GetLayerVisibleCandidatesSceneVersion() const override;
  std::unordered_set<std::string> &
  GetLayerVisibleCandidatesHiddenLayers() const override;
  size_t &GetLayerVisibleCandidatesRevision() const override;
  size_t &GetVisibleSetLayerCandidatesRevision() const override;
  bool &GetVisibleSetFrustumCulling() const override;
  float &GetVisibleSetMinPixels() const override;
  std::array<int, 4> &GetVisibleSetViewport() const override;
  std::array<double, 16> &GetVisibleSetModel() const override;
  std::array<double, 16> &GetVisibleSetProjection() const override;
};
