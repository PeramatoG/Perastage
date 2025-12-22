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

#include "viewer3dcontroller.h"
#include "layouts/LayoutCollection.h"
#include <functional>
#include <string>
#include <unordered_set>
#include <wx/glcanvas.h>

class Layout2DViewPanel : public wxGLCanvas {
public:
  explicit Layout2DViewPanel(wxWindow *parent);
  ~Layout2DViewPanel();

  static Layout2DViewPanel *Instance();
  static void SetInstance(Layout2DViewPanel *panel);

  void SetActiveLayout(const std::string &layoutName,
                       const layouts::Layout2DViewDefinition &view);
  const std::string &GetActiveLayoutName() const { return m_layoutName; }

  void UpdateScene(bool reload = true);

  Viewer2DView GetView() const { return m_view; }
  void SetView(Viewer2DView view);

  Viewer2DRenderMode GetRenderMode() const { return m_renderMode; }

  const layouts::Layout2DViewRenderOptions &GetRenderOptions() const;
  void UpdateRenderOptions(
      const std::function<void(layouts::Layout2DViewRenderOptions &)> &updater);

  std::unordered_set<std::string> GetHiddenLayers() const;
  void SetHiddenLayers(const std::unordered_set<std::string> &layers);

  layouts::Layout2DViewDefinition GetViewDefinition() const;
  void ApplyViewDefinition(const layouts::Layout2DViewDefinition &view);

private:
  void InitGL();
  void Render();
  void UpdateFrameFromLayout();
  void SyncCameraState();
  void DrawFrameOverlay(int width, int height);

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

  std::string m_layoutName;
  layouts::Layout2DViewDefinition m_viewDefinition;
  std::unordered_set<std::string> m_hiddenLayers;

  bool m_dragging = false;
  wxPoint m_lastMousePos;
  float m_offsetX = 0.0f;
  float m_offsetY = 0.0f;
  float m_zoom = 1.0f;
  bool m_mouseInside = false;

  wxGLContext *m_glContext = nullptr;
  bool m_glInitialized = false;
  Viewer3DController m_controller;
  Viewer2DRenderMode m_renderMode = Viewer2DRenderMode::White;
  Viewer2DView m_view = Viewer2DView::Top;

  static Layout2DViewPanel *s_instance;

  wxDECLARE_EVENT_TABLE();
};
