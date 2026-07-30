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
 * File: viewer3dpanel.h
 * Author: Luisma Peramato
 * License: GNU General Public License v3.0
 * Description: OpenGL-based 3D viewer panel using wxGLCanvas.
 */

#pragma once

#include "continuous_placement_type.h"
#include "continuous_placement_state.h"
#include <wx/glcanvas.h>
#include "../viewer2d/viewer2d_measure_tool.h"
#include "interaction/selection_drag_math.h"
#include "viewer3dcamera.h"
#include "viewer3dcontroller.h"
#include "ui_render_size.h"
#include "magnet_snap.h"
#include "transform_space.h"
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <wx/thread.h>
#include <wx/timer.h>
#include <vector>
#include <optional>

wxDECLARE_EVENT(wxEVT_VIEWER_REFRESH, wxThreadEvent);

class Viewer3DPanel : public wxGLCanvas
{
public:
    Viewer3DPanel(wxWindow* parent);
    ~Viewer3DPanel();

    // Stop the refresh thread. Safe to call multiple times.
    void StopRefreshThread();

    // Loads camera parameters from ConfigManager (delayed initialization)
    void LoadCameraFromConfig();

    // Toggles for rendering options
    bool showAxes = true;
    bool showGrid = true;

    void UpdateScene();
    void PrepareForSceneReplacement();
    void CompleteSceneReplacement();
    void SetSelectedFixtures(const std::vector<std::string>& uuids);
    void SetLayerColor(const std::string& layer, const std::string& hex);
    std::shared_ptr<const SymbolDefinitionSnapshot>
    GetBottomSymbolCacheSnapshot() const;

    static Viewer3DPanel* Instance();
    static void SetInstance(Viewer3DPanel* panel);

    Viewer3DCamera& GetCamera() { return m_camera; }
    const Viewer3DCamera& GetCamera() const { return m_camera; }
    bool ShouldPauseHeavyTasks();
    bool IsCameraMoving() const { return m_cameraMoving; }
    void SetStandardView(Viewer2DView view);
    bool FrameSceneToFit();
    bool ResetCameraToIsometric();
    void SetModalDialogActive(bool active);

    // Toggles the 3D measure tool mode and resets its transient state.
    void SetMeasureToolEnabled(bool enabled);
    // Toggles the 3D measure tool with the requested measuring mode.
    void SetMeasureToolEnabled(bool enabled, Viewer2DMeasureMode mode);
    // Returns whether the 3D measure tool is currently enabled.
    bool IsMeasureToolEnabled() const { return m_measureToolEnabled; }
    // Returns the active 3D measure mode.
    Viewer2DMeasureMode GetMeasureToolMode() const { return m_measureMode; }
    // Enables or disables Magnet snapping for 3D selection dragging.
    void SetMagnetEnabled(bool enabled, bool persist = true);
    // Returns whether Magnet snapping is currently enabled for 3D selection dragging.
    bool IsMagnetEnabled() const { return m_magnetEnabled; }
    void SetLeftDragSelectionMovementEnabled(bool enabled);
    // Returns whether left-click selection dragging is enabled in the 3D viewport.
    bool IsLeftDragSelectionMovementEnabled() const {
        return m_leftDragSelectionMovementEnabled;
    }
    void SetAxisConstrainedMovementEnabled(bool enabled);
    void SetTransformSpace(transform_space::TransformSpace space);
    // Returns whether 3D selection movement is constrained to axes.
    bool IsAxisConstrainedMovementEnabled() const {
        return m_axisConstrainedMovementEnabled;
    }
    void BeginContinuousPlacement(ContinuousPlacementType type,
                                  const std::string& elementUuid);
    bool UndoContinuousPlacement();
    bool IsContinuousPlacementActive() const {
        return m_continuousPlacementActive;
    }

    enum class HoverTargetTable { None, Fixtures, Trusses, SceneObjects };

private:
    wxGLContext* m_glContext;
    Viewer3DCamera m_camera;

    // Mouse interaction state
    bool m_dragging = false;
    bool m_draggedSincePress = false;
    bool m_mouseInside = false;
    wxPoint m_lastMousePos;
    bool m_hasLastMousePos = false;
    bool m_rectSelecting = false;
    bool m_rectSelectionAcrossAllTables = false;
    wxPoint m_rectSelectStart;
    wxPoint m_rectSelectEnd;
    bool m_selectionDragArmed = false;
    bool m_selectionDragMoved = false;
    bool m_selectionDragUndoPushed = false;
    bool m_magnetEnabled = false;
    bool m_leftDragSelectionMovementEnabled = false;
    bool m_axisConstrainedMovementEnabled = true;
    transform_space::TransformSpace m_transformSpace =
        transform_space::TransformSpace::World;
    std::optional<magnet_snap::SnapResult> m_pendingMagnetSnap;
    wxLongLong m_selectionDragPressTime = 0;
    HoverTargetTable m_selectionDragTarget = HoverTargetTable::None;
    std::vector<std::string> m_dragSelectionUuids;
    std::vector<std::string> m_dragFixtureUuids;
    std::vector<std::string> m_dragTrussUuids;
    std::vector<std::string> m_dragSceneObjectUuids;
    std::array<float, 3> m_selectionDragAnchorMeters{0.0f, 0.0f, 0.0f};
    viewer3d::SelectionDragAxis m_selectionDragAxis =
        viewer3d::SelectionDragAxis::None;
    bool m_continuousPlacementActive = false;
    ContinuousPlacementType m_continuousPlacementType =
        ContinuousPlacementType::None;
    continuous_placement::ViewRevisionState m_placementViewRevision;
    std::string m_continuousPlacementUuid;
    std::vector<std::string> m_continuousPlacedUuids;

    // Type of interaction currently active (Orbit or Pan)
    enum class InteractionMode { None, Orbit, Pan };
    InteractionMode m_mode = InteractionMode::None;
    std::chrono::steady_clock::time_point m_lastInteractionTime{};
    bool m_isInteracting = false;
    bool m_cameraMoving = false;
    std::chrono::steady_clock::time_point m_lastResourceSyncCheck{};
    std::vector<std::string> m_lastAppliedSelectionUuids;
    std::vector<std::string> m_lastAppliedPrimarySelectionUuids;

    // Initializes OpenGL settings
    bool InitGL();

    // Handles paint events
    void OnPaint(wxPaintEvent& event);

    // Handles resize events
    void OnResize(wxSizeEvent& event);

    // Handles mouse input for camera control
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);
    void OnMouseDClick(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void ApplyCameraDrag(const wxMouseEvent& event, const wxPoint& mousePos);

    // Clear all selected scene object types and refresh related UI state.
    void ClearAllObjectSelections(const char* undoLabel);
    // Synchronizes the current hover highlight with the 3D controller and tables.
    void SynchronizeHoverHighlight();
    void OnCaptureLost(wxMouseCaptureLostEvent& event);
    void ApplyRectangleSelection(const wxPoint& start, const wxPoint& end);
    // Safely binds the GL context for interaction and picking paths.
    bool TryBindGlContextForInteraction(const char* caller);
    // Prepares the GL context before running resource synchronization work.
    bool PrepareGlResourceSync(const char* caller);
    void DrawSelectionRectangle(int width, int height);
    void ResetSelectionDragState();
    bool PrepareSelectionDrag(const wxPoint& mousePos);
    std::array<float, 3> ComputeSelectionCenterMeters(
        const std::vector<std::string>& uuids, HoverTargetTable target) const;
    std::array<viewer3d::ProjectedAxis, 3> BuildProjectedDragAxes(
        const RenderSize& renderSize) const;
    std::optional<std::array<float, 3>> ProjectMouseToSelectionDragViewPlane(
        const wxPoint& mousePos, const RenderSize& renderSize,
        const std::array<float, 3>& planePointMeters) const;
    void ApplySelectionDragDelta(const std::array<float, 3>& deltaMeters);
    std::optional<magnet_snap::SnapSource> BuildActiveMagnetSource() const;
    magnet_snap::SnapSettings BuildActiveMagnetSettings(
        const magnet_snap::SnapSource& source) const;
    std::optional<magnet_snap::SnapResult> FindActiveMagnetSnap() const;
    std::optional<magnet_snap::SnapResult> RestorePendingMagnetSnapPreview();
    void CommitActiveMagnetSnap();
    void UpdateSelectionDragStatusPosition();
    void FinalizeSelectionDrag();
    bool AlignContinuousElementToPointer(const wxPoint& mousePos);
    void ConfirmContinuousPlacement();
    void CancelContinuousPlacement();
    void EndContinuousPlacementState();
    void RefreshContinuousPlacementViews();
    void DrawSelectionDragGizmo(const RenderSize& renderSize);
    std::array<float, 3> GetSelectionDragAxisVector(
        viewer3d::SelectionDragAxis axis) const;

    // Renders the full scene
    void Render(const RenderSize& renderSize);
    void ApplyCameraMatrices(const RenderSize& renderSize, double fovYDegrees = 45.0);
    bool ExportCurrentViewToPng();

    // Hovered fixture label state
    bool m_hasHover = false;
    wxPoint m_hoverPos;
    wxString m_hoverText;
    std::string m_hoverUuid;

    struct HoverQueryState {
        wxPoint mouseFramebufferPos;
        uint64_t cameraRevision = 0;
        uint64_t hiddenLayersRevision = 0;
        uint64_t sceneRevision = 0;
    };
    HoverQueryState m_lastHoverQueryState{};
    bool m_hasLastHoverQueryState = false;
    std::chrono::steady_clock::time_point m_lastHoverQueryTime{};
    bool m_forceHoverQuery = false;
    HoverTargetTable m_lastHoverTargetTable = HoverTargetTable::None;
    uint64_t m_cameraRevision = 0;
    uint64_t m_hiddenLayersRevision = 0;
    uint64_t m_sceneRevision = 0;
    uint64_t m_selectionRevision = 0;
    uint64_t m_highlightRevision = 0;
    size_t m_lastCameraFingerprint = 0;
    size_t m_lastHiddenLayersFingerprint = 0;
    size_t m_lastThreadCameraFingerprint = 0;
    bool m_hasLastThreadCameraFingerprint = false;
    bool m_paintInProgress = false;
    bool m_selectionRefreshPending = false;
    bool m_highlightRefreshPending = false;
    std::chrono::steady_clock::time_point m_refreshTelemetryWindowStart{};
    int m_fullRefreshesInCurrentWindow = 0;
    int m_highlightRefreshesInCurrentWindow = 0;
    double m_fullRenderMsAccumInCurrentWindow = 0.0;
    int m_fullRenderSamplesInCurrentWindow = 0;
    double m_hoverQueryMsAccumInCurrentWindow = 0.0;
    int m_hoverQuerySamplesInCurrentWindow = 0;
    double m_highlightUpdateMsAccumInCurrentWindow = 0.0;
    int m_highlightUpdateSamplesInCurrentWindow = 0;

    // True when the mouse moved since the last paint

    bool m_measureToolEnabled = false;
    Viewer2DMeasureMode m_measureMode = Viewer2DMeasureMode::CenterToCenter;
    bool m_measureHasAnchor = false;
    std::string m_measureAnchorUuid;
    std::array<float, 3> m_measureAnchorWorldMeters{0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_measureAnchorDrawWorldMeters{0.0f, 0.0f, 0.0f};
    bool m_measureHasCommittedTarget = false;
    std::array<float, 3> m_measureCommittedTargetWorldMeters{0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_measureCommittedTargetDrawWorldMeters{0.0f, 0.0f, 0.0f};
    wxPoint m_measurePreviewMousePos;
    bool m_measureHasPreviewMousePos = false;

    // Resets the active and committed 3D measurement points while preserving enablement.
    void ResetMeasureState();
    // Resolves the world-space center position of an element uuid on the active table.
    std::optional<std::array<float, 3>> ResolveMeasureWorldFromUuid(
        HoverTargetTable target, const std::string& uuid) const;
    // Resolves the world-space bounds of an element uuid on the active table.
    std::optional<ISelectionContext::BoundingBox> ResolveMeasureBoundsFromUuid(
        HoverTargetTable target, const std::string& uuid) const;
    // Computes nearest points between two world-space bounds for gap measuring.
    std::pair<std::array<float, 3>, std::array<float, 3>> ComputeNearestBoundsPoints(
        const ISelectionContext::BoundingBox& a, const ISelectionContext::BoundingBox& b) const;
    // Draws the 3D measurement line and optional distance overlay.
    void DrawMeasureOverlay(const RenderSize& renderSize);
    bool m_mouseMoved = false;

    // True once OpenGL/GLEW initialization has been performed
    bool m_glInitialized = false;

    // Multisample anti-aliasing availability negotiated at context creation.
    bool m_hasSampleBuffers = false;

    Viewer3DController m_controller;

    std::atomic<bool> m_threadRunning{false};
    std::atomic<bool> m_shuttingDown{false};
    std::atomic<bool> m_modalDialogActive{false};
    std::atomic<bool> m_refreshEventPending{false};
    std::thread m_refreshThread;
    void RefreshLoop();
    void OnThreadRefresh(wxThreadEvent& event);
    void OnZoomInteractionTimeout(wxTimerEvent& event);
    void ArmZoomInteractionTimeout();

    wxDECLARE_EVENT_TABLE();
    wxTimer m_zoomInteractionTimer;
};
