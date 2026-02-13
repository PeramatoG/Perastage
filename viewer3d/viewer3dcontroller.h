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

#include "viewer3d_types.h"
#include "irendercontext.h"
#include "iselectioncontext.h"
#include "ivisibilitycontext.h"
#include "symbolcache.h"
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wx/gdicmn.h>
#include <wx/string.h>

class Mesh;
class SceneRenderer;
class VisibilitySystem;
class SelectionSystem;
class LabelRenderSystem;
class OpaqueFixturePass;
class OpaqueTrussPass;
class OpaqueObjectPass;
class RenderPipeline;

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

  void InitializeGL();
  void Update();
  void UpdateResourcesIfDirty();
  void UpdateFrameStateLightweight();
  void ResetDebugPerFrameCounters();
  int GetDebugUpdateResourcesCallsPerFrame() const;

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

  void SetDarkMode(bool enabled);
  void SetInteracting(bool interacting);
  void SetCameraMoving(bool moving);
  bool IsCameraMoving() const override;
  void SetSelectionOutlineEnabled(bool enabled);

  void SetHighlightUuid(const std::string &uuid);
  void SetSelectedUuids(const std::vector<std::string> &uuids);

  void DrawFixtureLabels(int width, int height);
  void DrawTrussLabels(int width, int height);
  void DrawSceneObjectLabels(int width, int height);
  void DrawAllFixtureLabels(int width, int height, Viewer2DView view,
                            float zoom = 1.0f);

  bool GetFixtureLabelAt(int mouseX, int mouseY, int width, int height,
                         wxString &outLabel, wxPoint &outPos,
                         std::string *outUuid = nullptr);
  bool GetTrussLabelAt(int mouseX, int mouseY, int width, int height,
                       wxString &outLabel, wxPoint &outPos,
                       std::string *outUuid = nullptr);
  bool GetSceneObjectLabelAt(int mouseX, int mouseY, int width, int height,
                             wxString &outLabel, wxPoint &outPos,
                             std::string *outUuid = nullptr);

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

  void SetLayerColor(const std::string &layer, const std::string &hex);
  std::shared_ptr<const SymbolDefinitionSnapshot>
  GetBottomSymbolCacheSnapshot() const;

  void SetCaptureCanvas(ICanvas2D *canvas, Viewer2DView view,
                        bool includeGrid = true,
                        bool useSymbolInstancing = false);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  const VisibleSet &PrepareRenderFrame(const RenderFrameContext &context,
                                       ViewFrustumSnapshot &frustum);
  void RenderOpaqueFrame(const RenderFrameContext &context,
                         const VisibleSet &visibleSet);
  void RenderOverlayFrame(const RenderFrameContext &context,
                          const VisibleSet &visibleSet);
  void FinalizeRenderFrame();

  void DrawCube(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                float b = 1.0f);
  void DrawWireframeCube(float size = 0.3f, float r = 1.0f, float g = 1.0f,
                         float b = 0.0f,
                         Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                         const std::function<std::array<float, 3>(
                             const std::array<float, 3> &)> &captureTransform =
                             {},
                         float lineWidthOverride = -1.0f,
                         bool recordCapture = true);
  void DrawWireframeBox(float length, float height, float width,
                        bool highlight = false, bool selected = false,
                        bool wireframe = false,
                        Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                        const std::function<std::array<float, 3>(
                            const std::array<float, 3> &)> &captureTransform =
                            {});
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
  void DrawMeshWireframe(
      const Mesh &mesh, float scale = RENDER_SCALE,
      const std::function<std::array<float, 3>(
          const std::array<float, 3> &)> &captureTransform = {});
  void DrawCubeWithOutline(float size = 0.2f, float r = 1.0f, float g = 1.0f,
                           float b = 1.0f, bool highlight = false,
                           bool selected = false, float cx = 0.0f,
                           float cy = 0.0f, float cz = 0.0f,
                           bool wireframe = false,
                           Viewer2DRenderMode mode = Viewer2DRenderMode::White,
                           const std::function<std::array<float, 3>(
                               const std::array<float, 3> &)> &captureTransform =
                               {});
  void ApplyTransform(const float matrix[16], bool scaleTranslation = true);
  void DrawGrid(int style, float r, float g, float b,
                Viewer2DView view = Viewer2DView::Top);
  void DrawAxes();
  void SetupBasicLighting();
  void SetupMaterialFromRGB(float r, float g, float b);
  void SetGLColor(float r, float g, float b) const override;
  std::array<float, 3> AdjustColor(float r, float g, float b) const;
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
  void SetupMeshBuffers(Mesh &mesh);
  void ReleaseMeshBuffers(Mesh &mesh);
  void DrawMesh(const Mesh &mesh, float scale = RENDER_SCALE,
                const float *modelMatrix = nullptr);

  bool TryBuildLayerVisibleCandidates(
      const std::unordered_set<std::string> &hiddenLayers,
      VisibleSet &out) const;
  bool TryBuildVisibleSet(const ViewFrustumSnapshot &frustum,
                          bool useFrustumCulling, float minPixels,
                          const VisibleSet &layerVisibleCandidates,
                          VisibleSet &out) const;
  bool EnsureBoundsComputed(const std::string &uuid, ItemType type,
                            const std::unordered_set<std::string> &hiddenLayers);

  bool IsInteracting() const override;
  bool UseAdaptiveLineProfile() const override;
  bool SkipOutlinesForCurrentFrame() const override;
  bool IsSelectionOutlineEnabled2D() const override;
  bool IsCaptureOnly() const override;
  ICanvas2D *GetCaptureCanvas() const override;
  bool CaptureIncludesGrid() const override;

  void ApplyHighlightUuid(const std::string &uuid) override;
  void ReplaceSelectedUuids(const std::vector<std::string> &uuids) override;
  const BoundingBox *FindFixtureBounds(const std::string &uuid) const override;
  const BoundingBox *FindTrussBounds(const std::string &uuid) const override;
  const BoundingBox *FindObjectBounds(const std::string &uuid) const override;
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

  friend class OpaqueFixturePass;
  friend class OpaqueTrussPass;
  friend class OpaqueObjectPass;
  friend class RenderPipeline;
};
