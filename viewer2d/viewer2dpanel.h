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
#include <functional>
#include <memory>
#include <optional>
#include <string>
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

  // Update cached color for a specific layer so user selections are applied
  // immediately to the 2D renderer.
  void SetLayerColor(const std::string &layer, const std::string &hex);

  void LoadViewFromConfig();
  void SaveViewToConfig() const;

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
                    int &height);

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

  void SetLayoutEditOverlay(std::optional<float> aspectRatio,
                            std::optional<wxSize> viewportSize = std::nullopt);
  void SetLayoutEditOverlayScale(float scale);
  float GetLayoutEditOverlayScale() const { return m_layoutEditScale; }
  std::optional<wxSize> GetLayoutEditOverlaySize() const;

private:
  void InitGL();
  void Render();
  void RenderInternal(bool swapBuffers);
  void OnPaint(wxPaintEvent &event);

  void OnMouseDown(wxMouseEvent &event);
  void OnMouseUp(wxMouseEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnMouseWheel(wxMouseEvent &event);
  void OnKeyDown(wxKeyEvent &event);
  void OnMouseEnter(wxMouseEvent &event);
  void OnMouseLeave(wxMouseEvent &event);
  void OnResize(wxSizeEvent &event);
  void OnCaptureLost(wxMouseCaptureLostEvent &event);

  bool m_dragging = false;
  bool m_draggedSincePress = false;
  wxPoint m_lastMousePos;
  float m_offsetX = 0.0f;
  float m_offsetY = 0.0f;
  float m_zoom = 1.0f;
  bool m_mouseInside = false;
  bool m_mouseMoved = false;
  bool m_hasHover = false;
  bool m_enableSelection = true;
  std::string m_hoverUuid;

  bool m_captureNextFrame = false;
  bool m_useSimplifiedFootprints = false;
  bool m_captureIncludeGrid = true;
  CommandBuffer m_lastCapturedFrame;
  std::function<void(CommandBuffer, Viewer2DViewState)> m_captureCallback;
  std::string m_lastFixtureDebugReport;
  bool m_forceOffscreenRender = false;
  bool m_allowOffscreenRender = false;
  bool m_persistViewState = true;

  wxGLContext *m_glContext = nullptr;
  bool m_glInitialized = false;
  Viewer3DController m_controller;
  Viewer2DRenderMode m_renderMode = Viewer2DRenderMode::White;
  Viewer2DView m_view = Viewer2DView::Top;
  std::optional<float> m_layoutEditAspect;
  std::optional<wxSize> m_layoutEditBaseSize;
  std::optional<wxSize> m_layoutEditViewportSize;
  float m_layoutEditScale = 1.0f;

  wxDECLARE_EVENT_TABLE();
};
