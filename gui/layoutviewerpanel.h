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

#include <memory>
#include <optional>
#include <wx/glcanvas.h>
#include <wx/wx.h>
#include "layouts/LayoutCollection.h"
#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"
#include "viewer2dstate.h"

wxDECLARE_EVENT(EVT_LAYOUT_VIEW_EDIT, wxCommandEvent);

class LayoutViewerPanel : public wxGLCanvas {
public:
  explicit LayoutViewerPanel(wxWindow *parent);
  ~LayoutViewerPanel();

  void SetLayoutDefinition(const layouts::LayoutDefinition &layout);
  layouts::Layout2DViewDefinition *GetEditableView();
  const layouts::Layout2DViewDefinition *GetEditableView() const;

private:
  void OnPaint(wxPaintEvent &event);
  void OnSize(wxSizeEvent &event);
  void OnLeftDown(wxMouseEvent &event);
  void OnLeftUp(wxMouseEvent &event);
  void OnLeftDClick(wxMouseEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnMouseWheel(wxMouseEvent &event);
  void OnCaptureLost(wxMouseCaptureLostEvent &event);
  void OnRightUp(wxMouseEvent &event);
  void OnEditView(wxCommandEvent &event);
  void OnDeleteView(wxCommandEvent &event);

  void ResetViewToFit();
  wxRect GetPageRect() const;
  bool GetFrameRect(const layouts::Layout2DViewFrame &frame,
                    wxRect &rect) const;
  void UpdateFrame(const layouts::Layout2DViewFrame &frame,
                   bool updatePosition);
  void InitGL();
  void RebuildCachedTexture();
  void ClearCachedTexture();
  void RequestRenderRebuild();
  void InvalidateRenderIfFrameChanged();
  void EmitEditViewRequest();

  enum class FrameDragMode {
    None,
    Move,
    ResizeRight,
    ResizeBottom,
    ResizeCorner
  };

  FrameDragMode HitTestFrame(const wxPoint &pos, const wxRect &frameRect) const;
  wxCursor CursorForMode(FrameDragMode mode) const;

  layouts::LayoutDefinition currentLayout;
  double zoom = 1.0;
  wxPoint panOffset{0, 0};
  bool isPanning = false;
  wxPoint lastMousePos{0, 0};
  FrameDragMode dragMode = FrameDragMode::None;
  FrameDragMode hoverMode = FrameDragMode::None;
  wxPoint dragStartPos{0, 0};
  layouts::Layout2DViewFrame dragStartFrame;
  int layoutVersion = 0;
  int captureVersion = -1;
  bool captureInProgress = false;
  bool hasCapture = false;
  CommandBuffer cachedBuffer;
  Viewer2DViewState cachedViewState;
  viewer2d::Viewer2DState cachedRenderState;
  bool hasRenderState = false;
  std::shared_ptr<const SymbolDefinitionSnapshot> cachedSymbols;
  wxGLContext *glContext_ = nullptr;
  bool glInitialized_ = false;
  unsigned int cachedTexture_ = 0;
  wxSize cachedTextureSize{0, 0};
  bool renderDirty = true;
  bool renderPending = false;

  wxDECLARE_EVENT_TABLE();
};
