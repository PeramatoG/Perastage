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

#include <wx/glcanvas.h>
#include "viewer3dcamera.h"
#include "viewer3dcontroller.h"
#include "ui_render_size.h"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <wx/thread.h>
#include <wx/timer.h>
#include <vector>

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

    enum class HoverTargetTable { None, Fixtures, Trusses, SceneObjects };

private:
    wxGLContext* m_glContext;
    Viewer3DCamera m_camera;

    // Mouse interaction state
    bool m_dragging = false;
    bool m_draggedSincePress = false;
    bool m_mouseInside = false;
    wxPoint m_lastMousePos;
    bool m_rectSelecting = false;
    bool m_rectSelectionAcrossAllTables = false;
    wxPoint m_rectSelectStart;
    wxPoint m_rectSelectEnd;

    // Type of interaction currently active (Orbit or Pan)
    enum class InteractionMode { None, Orbit, Pan };
    InteractionMode m_mode = InteractionMode::None;
    std::chrono::steady_clock::time_point m_lastInteractionTime{};
    bool m_isInteracting = false;
    bool m_cameraMoving = false;
    std::chrono::steady_clock::time_point m_lastResourceSyncCheck{};
    std::vector<std::string> m_lastAppliedSelectionUuids;

    // Initializes OpenGL settings
    void InitGL();

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
    void OnCaptureLost(wxMouseCaptureLostEvent& event);
    void ApplyRectangleSelection(const wxPoint& start, const wxPoint& end);
    void DrawSelectionRectangle(int width, int height);

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
    bool m_mouseMoved = false;

    // True once OpenGL/GLEW initialization has been performed
    bool m_glInitialized = false;

    // Multisample anti-aliasing availability negotiated at context creation.
    bool m_hasSampleBuffers = false;

    Viewer3DController m_controller;
    std::unique_ptr<class BasePassFramebufferCache> m_basePassCache;

    std::atomic<bool> m_threadRunning{false};
    std::atomic<bool> m_shuttingDown{false};
    std::atomic<bool> m_modalDialogActive{false};
    std::thread m_refreshThread;
    void RefreshLoop();
    void OnThreadRefresh(wxThreadEvent& event);
    void OnZoomInteractionTimeout(wxTimerEvent& event);
    void ArmZoomInteractionTimeout();

    wxDECLARE_EVENT_TABLE();
    wxTimer m_zoomInteractionTimer;
};
