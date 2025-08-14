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

#include "viewer3dcontroller.h"
#include <wx/glcanvas.h>
#include <wx/wx.h>

class Viewer2DPanel : public wxGLCanvas {
public:
  explicit Viewer2DPanel(wxWindow *parent);
  ~Viewer2DPanel();

  static Viewer2DPanel *Instance();
  static void SetInstance(Viewer2DPanel *panel);

  void UpdateScene(bool reload = true);

  void SetRenderMode(Viewer2DRenderMode mode);
  Viewer2DRenderMode GetRenderMode() const { return m_renderMode; }

  void SetView(Viewer2DView view);
  Viewer2DView GetView() const { return m_view; }

  void LoadViewFromConfig();
  void SaveViewToConfig() const;

private:
  void InitGL();
  void Render();
  void OnPaint(wxPaintEvent &event);

  void OnMouseDown(wxMouseEvent &event);
  void OnMouseUp(wxMouseEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnMouseWheel(wxMouseEvent &event);
  void OnKeyDown(wxKeyEvent &event);
  void OnMouseEnter(wxMouseEvent &event);
  void OnMouseLeave(wxMouseEvent &event);
  void OnResize(wxSizeEvent &event);

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

  wxDECLARE_EVENT_TABLE();
};
