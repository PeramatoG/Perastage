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
 * File: viewer2dpanel.h
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: OpenGL-based top-down viewer sharing models with the 3D view.
 */

#pragma once

#include "canvas2d.h"
#include "viewer3dcontroller.h"
#include <wx/glcanvas.h>
#include <wx/wx.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Current viewport information used to rebuild the same projection when
// exporting or printing the 2D view.
struct Viewer2DViewState {
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  float zoom = 1.0f;
  int viewportWidth = 0;
  int viewportHeight = 0;
  Viewer2DView view = Viewer2DView::Top;
};

struct Viewer2DRenderOverrides {
  std::optional<bool> darkMode;
  std::optional<bool> showGrid;
  std::optional<bool> showRuler;
  std::optional<bool> drawFixtureLabels;
  std::optional<bool> forceBottomViewForTopFixtures;
  std::optional<bool> symbolCaptureRenderProfile;
  std::optional<bool> symbolCaptureIncludeCoplanarEdges;
};

class Viewer2DPanel : public wxGLCanvas {
public:
  explicit Viewer2DPanel(wxWindow *parent, bool allowOffscreenRender = false,
                         bool persistViewState = true,
                         bool enableSelection = true);
  ~Viewer2DPanel();

  static Viewer2DPanel *Instance();
  static void SetInstance(Viewer2DPanel *panel);

  void UpdateScene(bool reload = true);

  void SetRenderMode(Viewer2DRenderMode mode);
  Viewer2DRenderMode GetRenderMode() const { return m_renderMode; }

  void SetView(Viewer2DView view);
  Viewer2DView GetView() const { return m_view; }

  // Synchronize the highlighted selection from external sources (tables/3D).
  void SetSelectedUuids(const std::vector<std::string> &selection);

  // Update cached color for a specific layer so user selections are applied
  // immediately to the 2D renderer.
  void SetLayerColor(const std::string &layer, const std::string &hex);

  void LoadViewFromConfig();
  void SaveViewToConfig() const;
  void ApplyViewState(float offsetX, float offsetY, float zoom,
                      Viewer2DView view, Viewer2DRenderMode renderMode);
  bool FitViewToScene();

  // Request that the next paint pass stores every 2D drawing command in
  // m_lastCapturedFrame. The on-screen result is unchanged.
  void RequestFrameCapture();

  // Ask the panel to capture the next rendered frame and forward both the
  // recorded drawing commands and the view state to the provided callback.
  // The capture occurs on the UI thread during the following paint event;
  // the callback is invoked immediately afterwards.
  void CaptureFrameAsync(
      std::function<void(CommandBuffer, Viewer2DViewState)> callback,
      bool useSimplifiedFootprints = false,
      bool includeGridInCapture = true);
  void CaptureFrameNow(
      std::function<void(CommandBuffer, Viewer2DViewState)> callback,
      bool useSimplifiedFootprints = false,
      bool includeGridInCapture = true);

  bool RenderToRGBA(std::vector<unsigned char> &pixels, int &width,
                    int &height,
                    const std::optional<wxSize> &targetFramebufferSize =
                        std::nullopt);
  void SetPreferPerastageSvgSymbolsForLayouts(bool enabled) {
    m_preferPerastageSvgSymbolsForLayouts = enabled;
  }
  bool GetPreferPerastageSvgSymbolsForLayouts() const {
    return m_preferPerastageSvgSymbolsForLayouts;
  }

  // Accessor for the last recorded set of drawing commands. The buffer is
  // cleared and re-populated on every requested capture.
  const CommandBuffer &GetLastCapturedFrame() const { return m_lastCapturedFrame; }

  // Returns the most recent per-fixture debug report generated during frame
  // capture. The string is empty when no report was produced.
  std::string GetLastFixtureDebugReport() const { return m_lastFixtureDebugReport; }

  // Accessor for the current viewport state so exporters can match what the
  // user is seeing on screen.
  Viewer2DViewState GetViewState() const;

  std::shared_ptr<const SymbolDefinitionSnapshot>
  GetBottomSymbolCacheSnapshot() const;
  void InvalidateBottomSymbolCache();

  void SetLayoutEditOverlay(std::optional<float> aspectRatio,
                            std::optional<wxSize> viewportSize = std::nullopt);
  void SetLayoutEditOverlayScale(float scale);
  float GetLayoutEditOverlayScale() const { return m_layoutEditScale; }
  std::optional<wxSize> GetLayoutEditOverlaySize() const;

  using CursorWorldPositionCallback =
      std::function<void(const std::optional<std::array<float, 3>> &)>;
  void SetCursorWorldPositionCallback(CursorWorldPositionCallback callback);
  void SetRenderOverrides(
      const std::optional<Viewer2DRenderOverrides> &overrides);
  std::optional<Viewer2DRenderOverrides> GetRenderOverrides() const {
    return m_renderOverrides;
  }

private:
  enum class DragMode { None, View, Selection, RectSelection };
  enum class DragAxis { None, Horizontal, Vertical };
  enum class DragTarget { None, Fixtures, Trusses, Supports, SceneObjects };
  enum class PickQueryKind {
    None,
    FixtureLabel,
    TrussLabel,
    HoistLabel,
    SceneObjectLabel,
    PickUuid
  };

  void InitGL();
  void Render();
  void RenderInternal(bool swapBuffers);
  void OnPaint(wxPaintEvent &event);

  void OnMouseDown(wxMouseEvent &event);
  void OnMouseDClick(wxMouseEvent &event);
  void OnMouseUp(wxMouseEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnMouseWheel(wxMouseEvent &event);
  void OnRightUp(wxMouseEvent &event);
  void OnKeyDown(wxKeyEvent &event);
  void OnMouseEnter(wxMouseEvent &event);
  void OnMouseLeave(wxMouseEvent &event);
  void OnResize(wxSizeEvent &event);
  void OnCaptureLost(wxMouseCaptureLostEvent &event);
  void RequestRepaint();
  void RequestRepaint(const wxRect &dirtyRect);
  void ResetRepaintCoalescing();
  void TrackRefreshTelemetry();

  std::array<float, 3> MapDragDelta(float dxMeters, float dyMeters) const;
  std::optional<std::array<float, 3>>
  ComputeWorldPositionFromScreen(const wxPoint &screenPos) const;
  void NotifyCursorWorldPosition(const wxPoint &screenPos);
  void ClearCursorWorldPosition();
  void ApplySelectionDelta(const std::array<float, 3> &deltaMeters);
  void FinalizeSelectionDrag();
  void ApplyRectangleSelection(const wxPoint &start, const wxPoint &end,
                               bool selectAcrossAllTables);
  void DrawSelectionRectangle(int width, int height, bool darkMode);
  void ScheduleDragTableUpdate();
  void StopDragTableUpdates();
  void StartDragTableUpdateWorker();
  void StopDragTableUpdateWorker();
  bool ShouldPauseHeavyTasks();
  void MarkInteractionActivity();
  void OnInteractionPauseTimer(wxTimerEvent &event);
  void OnHoverHitTestTimer(wxTimerEvent &event);
  void ScheduleHoverHitTest(const wxPoint &screenPos, bool forceNow = false);
  int GetHoverHitTestIntervalMs() const;
  int GetHoverMoveThresholdPx() const;
  void TrackHoverHitTestTelemetry(std::chrono::microseconds duration);
  void ScheduleHoverLabelRefresh(const wxPoint &screenPos);
  bool TryUpdateHoverHighlightFast(const wxPoint &screenPos);
  void RunHoverHitTest(const wxPoint &screenPos);
  bool TryResolvePickUuidWithCache(const wxPoint &framebufferPos, int viewportWidth,
                                   int viewportHeight, size_t hiddenLayersHash,
                                   std::string &uuidOut);
  bool TryResolvePickLabelWithCache(PickQueryKind queryKind,
                                    const wxPoint &framebufferPos,
                                    int viewportWidth, int viewportHeight,
                                    size_t hiddenLayersHash, bool clickSelection,
                                    std::string &uuidOut);
  void StorePickCache(PickQueryKind queryKind, const wxPoint &framebufferPos,
                      int viewportWidth, int viewportHeight,
                      size_t hiddenLayersHash, bool clickSelection, bool found,
                      const std::string &uuid);
  void InvalidatePickCache();
  size_t BuildHiddenLayersHash();
  bool IsPickCacheReusable(PickQueryKind queryKind, const wxPoint &framebufferPos,
                           int viewportWidth, int viewportHeight,
                           size_t hiddenLayersHash, bool clickSelection) const;
  bool ApplyHoverUuid(const std::string &newUuid, bool requestRepaint);
  void ClearHoverState(bool requestRepaint);
  bool IsExpensiveVisualInteractionActive() const;

  struct DragTablePositionSnapshot {
    std::string uuid;
    float xMm = 0.0f;
    float yMm = 0.0f;
    float zMm = 0.0f;
  };

  std::vector<DragTablePositionSnapshot>
  BuildDragTablePositionSnapshots(DragTarget target,
                                  const std::vector<std::string> &uuids);
  void QueueDragTableUpdate(DragTarget target,
                            std::vector<std::string> uuids);

  static constexpr long kSelectionDragDelayMs = 150;
  static constexpr int kDragTableUpdateIntervalMs = 50;
  static constexpr int kHoverHitTestIdleIntervalMs = 10;
  static constexpr int kHoverHitTestInteractingIntervalMs = 35;
  static constexpr int kHoverMoveThresholdPx = 3;
  static constexpr int kHoverIdleMoveThresholdPx = 0;
  static constexpr std::chrono::milliseconds kPauseDelay{200};
  static constexpr int kPickCacheReuseDistancePx = 3;

  struct PickCacheEntry {
    bool valid = false;
    PickQueryKind queryKind = PickQueryKind::None;
    wxPoint framebufferPos;
    int viewportWidth = 0;
    int viewportHeight = 0;
    Viewer2DView view = Viewer2DView::Top;
    size_t hiddenLayersHash = 0;
    bool clickSelection = false;
    uint64_t sceneGeneration = 0;
    std::chrono::steady_clock::time_point timestamp{};
    bool found = false;
    std::string uuid;
  };

  DragMode m_dragMode = DragMode::None;
  DragAxis m_dragAxis = DragAxis::None;
  DragTarget m_dragTarget = DragTarget::None;
  std::vector<std::string> m_dragSelectionUuids;
  std::vector<std::string> m_dragFixtureUuids;
  std::vector<std::string> m_dragTrussUuids;
  std::vector<std::string> m_dragSupportUuids;
  std::vector<std::string> m_dragSceneObjectUuids;
  bool m_dragSelectionMoved = false;
  bool m_dragSelectionPushedUndo = false;
  bool m_draggedSincePress = false;
  wxLongLong m_dragPressTime = 0;
  bool m_rectSelecting = false;
  bool m_rectSelectionAcrossAllTables = false;
  wxPoint m_rectSelectStart;
  wxPoint m_rectSelectEnd;
  wxPoint m_lastMousePos;
  float m_offsetX = 0.0f;
  float m_offsetY = 0.0f;
  float m_zoom = 1.0f;
  bool m_mouseInside = false;
  bool m_hasHover = false;
  std::chrono::steady_clock::time_point m_lastInteractionTime{};
  bool m_isInteracting = false;
  bool m_interactiveLabelMode = false;
  wxTimer m_interactionResumeTimer;
  wxTimer m_hoverHitTestTimer;
  wxPoint m_pendingHoverScreenPos;
  wxPoint m_lastHoverQueryScreenPos;
  wxPoint m_lastFastHoverScreenPos;
  bool m_fastHoverHasPos = false;
  bool m_hoverQueryHasPos = false;
  bool m_hoverHitTestPending = false;
  std::chrono::steady_clock::time_point m_lastHoverHitTestTime{};
  bool m_viewMotionSinceLastHoverHitTest = false;
  PickCacheEntry m_pickCache;
  uint64_t m_pickCacheSceneGeneration = 0;
  bool m_enableSelection = true;
  std::vector<std::string> m_lastAppliedSelectionUuids;
  std::string m_hoverUuid;
  CursorWorldPositionCallback m_cursorWorldPositionCallback;

  bool m_captureNextFrame = false;
  bool m_useSimplifiedFootprints = false;
  bool m_captureIncludeGrid = true;
  CommandBuffer m_lastCapturedFrame;
  std::function<void(CommandBuffer, Viewer2DViewState)> m_captureCallback;
  std::string m_lastFixtureDebugReport;
  bool m_forceOffscreenRender = false;
  bool m_allowOffscreenRender = false;
  bool m_persistViewState = true;
  std::thread m_dragTableUpdateWorker;
  std::mutex m_dragTableUpdateMutex;
  std::condition_variable m_dragTableUpdateCv;
  bool m_dragTableWorkerStop = false;
  bool m_dragTableUpdateQueued = false;
  DragTarget m_dragTableUpdateWorkerTarget = DragTarget::None;
  std::vector<std::string> m_dragTableUpdateUuids;
  std::mutex m_dragTableUpdateSceneMutex;

  wxGLContext *m_glContext = nullptr;
  bool m_glInitialized = false;
  Viewer3DController m_controller;
  Viewer2DRenderMode m_renderMode = Viewer2DRenderMode::White;
  Viewer2DView m_view = Viewer2DView::Top;
  std::optional<float> m_layoutEditAspect;
  std::optional<wxSize> m_layoutEditBaseSize;
  std::optional<wxSize> m_layoutEditViewportSize;
  std::optional<wxSize> m_captureFramebufferSizeOverride;
  float m_layoutEditScale = 1.0f;
  bool m_preferPerastageSvgSymbolsForLayouts = false;
  std::optional<Viewer2DRenderOverrides> m_renderOverrides;
  bool m_repaintQueued = false;
  bool m_fullRepaintQueued = false;
#ifndef NDEBUG
  std::chrono::steady_clock::time_point m_refreshTelemetryWindowStart{};
  int m_refreshesInCurrentWindow = 0;
  std::chrono::steady_clock::time_point m_hoverTelemetryWindowStart{};
  int m_hoverQueriesInCurrentWindow = 0;
  std::chrono::microseconds m_hoverTotalResolveTimeInCurrentWindow{0};
#endif

  wxDECLARE_EVENT_TABLE();
};
