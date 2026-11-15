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
#include "layoutviewerpanel.h"
#include "layoutviewerpanel_helpers.h"
#include "LayoutManager.h"
#include "editable_focus_utils.h"
#include <wx/log.h>

void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  SetFocus();
  pendingFrameCommit_ = false;
  const wxPoint pos = layoutviewerpanel::GetLogicalMousePosition(event);
  const gui::layoutinteraction::Point pointer{pos.x, pos.y};
  if (HandleSelectionDown(event, pos)) return;
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(GetDisplayedFrame(selectionState_.Current(), selectedFrame), frameRect)) {
    const FrameDragMode mode = gui::layoutinteraction::HitTestFrame(
        pointer, {frameRect.GetX(), frameRect.GetY(), frameRect.GetWidth(),
                  frameRect.GetHeight()});
    if (mode != FrameDragMode::None) {
      interactionSession_.BeginFrameDrag(mode, pointer, selectedFrame);
      if (!mouseCapture_.TryAcquire("frame-drag")) {
        interactionSession_.CancelAfterCaptureLoss();
        return;
      }
      return;
    }
  }
  interactionSession_.BeginPan(pointer);
  if (!mouseCapture_.TryAcquire("viewport-pan"))
    interactionSession_.CancelAfterCaptureLoss();
}
// Finalizes the active frame edit or viewport pan gesture.
void LayoutViewerPanel::OnLeftUp(wxMouseEvent &event) {
  if (HandleSelectionUp(layoutviewerpanel::GetLogicalMousePosition(event))) return;
  const FrameDragMode dragMode = interactionSession_.DragMode();
  if (dragMode != FrameDragMode::None) {
    layouts::Layout2DViewFrame preview;
    if (GetSelectedFrame(preview)) {
      if (dragMode != FrameDragMode::Move)
        preview = interactionSession_.DeferredResize().value_or(
            interactionSession_.DragStartFrame());
      const auto final = interactionSession_.FinalFrame(preview);
      if (final) {
        ApplyFrameUpdateToSelection(*final, dragMode == FrameDragMode::Move);
        CommitPendingFrameUpdate();
      } else if (dragMode == FrameDragMode::Move) {
        ApplyFrameUpdateToSelection(interactionSession_.DragStartFrame(), true);
        pendingFrameCommit_ = false;
      }
    }
    interactionSession_.CompleteFrameDrag();
    mouseCapture_.Release("frame-drag");
    return;
  }
  if (interactionSession_.IsPanning()) {
    interactionSession_.EndPan();
    mouseCapture_.Release("viewport-pan");
  }
}

void LayoutViewerPanel::OnLeftDClick(wxMouseEvent &event) {
  CancelFrameGesture();
  mouseCapture_.Release("frame-drag");
  const wxPoint pos = layoutviewerpanel::GetLogicalMousePosition(event);
  if (const auto hit = HitElement(pos)) {
    selectionState_.Select(hit->kind, hit->id);
    Refresh();
  }
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(GetDisplayedFrame(selectionState_.Current(), selectedFrame), frameRect) && frameRect.Contains(pos)) {
    if (selectionState_.Current().kind == LayoutElementKind::View2D) {
      EmitEditViewRequest();
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
      wxCommandEvent editEvent;
      OnEditEventTable(editEvent);
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::Legend) {
      wxCommandEvent editEvent;
      OnEditLegend(editEvent);
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::Text) {
      wxCommandEvent editEvent;
      OnEditText(editEvent);
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::Image) {
      wxCommandEvent editEvent;
      OnEditImage(editEvent);
      return;
    }
  }
  event.Skip();
}

bool LayoutViewerPanel::DeleteSelectedElement() {
  if (selectionState_.Ordered().empty()) return false;
  CancelFrameGesture();
  mouseCapture_.Release("frame-drag");
  std::string error;
  if (!layouts::LayoutManager::Get().DeleteElements(currentLayout.name, selectionState_.Ordered(), &error)) {
    wxLogWarning("%s", wxString::FromUTF8(error));
    return false;
  }
  ReloadDocumentPlacement();
  return true;
}

void LayoutViewerPanel::OnKeyDown(wxKeyEvent &event) {
  if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
    event.Skip();
    return;
  }

  const int key = event.GetKeyCode();
  if (key == WXK_ESCAPE && (groupGesture_.Active() || marqueeStart_ || interactionSession_.DragMode() != FrameDragMode::None)) {
    CancelFrameGesture();
    mouseCapture_.Release("frame-drag");
    return;
  }
  if (key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
    if (interactionSession_.DragMode() != FrameDragMode::None) {
      CancelFrameGesture();
      mouseCapture_.Release("frame-drag");
    }
    DeleteSelectedElement();
    return;
  }
  if (key == 'Z' || key == 'z') {
    ResetViewToFit();
    RequestRenderRebuild();
    Refresh();
    return;
  }
  event.Skip();
}

// Handles pane visibility changes and resumes pending automatic fits.
void LayoutViewerPanel::OnShow(wxShowEvent &event) {
  if (!event.IsShown()) {
    CancelFrameGesture();
    mouseCapture_.Release("workspace-hidden");
  }
  if (event.IsShown()) {
    InitGL();
    SchedulePendingFitToViewport();
    InvalidateRenderIfFrameChanged(true);
    if (isReadyToRender_ && NeedsRenderRebuild()) {
      RequestRenderRebuild();
    }
  }
  event.Skip();
}

// Routes pointer motion through frame-edit and viewport-pan policies.
void LayoutViewerPanel::OnMouseMove(wxMouseEvent &event) {
  const wxPoint currentPos = layoutviewerpanel::GetLogicalMousePosition(event);
  const gui::layoutinteraction::Point pointer{currentPos.x, currentPos.y};
  if (HandleSelectionMotion(event, currentPos)) return;
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(GetDisplayedFrame(selectionState_.Current(), selectedFrame), frameRect)) {
    interactionSession_.UpdateHoverMode(gui::layoutinteraction::HitTestFrame(
        pointer, {frameRect.GetX(), frameRect.GetY(), frameRect.GetWidth(),
                  frameRect.GetHeight()}));
    if (SelectedElementCount() > 1) {
      const auto hit = HitElement(currentPos);
      interactionSession_.UpdateHoverMode(hit && selectionState_.Matches(*hit)
          ? FrameDragMode::Move : FrameDragMode::None);
    }
    SetCursor(CursorForMode(interactionSession_.HoverMode()));
  } else {
    interactionSession_.UpdateHoverMode(FrameDragMode::None);
    SetCursor(wxCursor(wxCURSOR_ARROW));
  }

  const FrameDragMode dragMode = interactionSession_.DragMode();
  if (dragMode != FrameDragMode::None && event.Dragging()) {
    SetCursor(CursorForMode(dragMode));
    std::optional<double> imageAspectRatio;
    if (selectionState_.Current().kind == LayoutElementKind::Image) {
      const auto *image = GetSelectedImage();
      imageAspectRatio = image ? image->aspectRatio : 0.0;
    }
    const layouts::Layout2DViewFrame frame =
        gui::layoutinteraction::ComputeDraggedFrame(
            dragMode, interactionSession_.DragStartFrame(),
            interactionSession_.DragStartPointer(), pointer,
            viewportState_.Zoom(), imageAspectRatio);
    const bool updatePosition = dragMode == FrameDragMode::Move;
    if (updatePosition) {
      ApplyFrameUpdateToSelection(frame, true);
    } else {
      interactionSession_.SetDeferredResize(frame);
      Refresh();
    }
    return;
  }

  if (!interactionSession_.IsPanning() || !event.Dragging())
    return;

  const gui::layoutinteraction::Point delta =
      interactionSession_.UpdatePan(pointer);
  viewportState_.ApplyPan({delta.x, delta.y});
  Refresh();
}

// Updates visual zoom around the cursor and defers high-quality cache
// rendering.
void LayoutViewerPanel::OnMouseWheel(wxMouseEvent &event) {
  if (groupGesture_.Active() || marqueeStart_ || interactionSession_.DragMode() != FrameDragMode::None)
    return;
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  const wxPoint mousePos = layoutviewerpanel::GetLogicalMousePosition(event);
  if (!viewportState_.ApplyWheelZoom(
          event.GetWheelRotation(), event.GetWheelDelta(),
          {mousePos.x, mousePos.y}, {size.GetWidth(), size.GetHeight()},
          layoutviewerpanel::GetLayoutSafeMaxZoom(currentLayout)))
    return;
  InvalidateRenderIfFrameChanged(false);
  RequestRenderRebuild();
  Refresh();
}

// Discards the local preview when a gesture is cancelled.
void LayoutViewerPanel::CancelFrameGesture() {
  if (groupGesture_.Active()) {
    currentLayout = groupGesture_.Original();
    groupGesture_.Cancel();
    InvalidateSelectionIndexCache();
  }
  marqueeStart_.reset();
  groupClick_.reset();
  if (interactionSession_.DragMode() == FrameDragMode::Move)
    ApplyFrameUpdateToSelection(interactionSession_.DragStartFrame(), true);
  pendingFrameCommit_ = false;
  interactionSession_.CancelAfterCaptureLoss();
  Refresh();
}

void LayoutViewerPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  mouseCapture_.AbandonOnLoss("active-gesture");
  CancelFrameGesture();
}


bool LayoutViewerPanel::SelectElementAtPosition(const wxPoint &pos) {
  const auto hit = HitElement(pos);
  if (!hit) return false;
  if (!selectionState_.Matches(*hit)) selectionState_.Select(hit->kind, hit->id);
  if (hit->kind == LayoutElementKind::View2D) EmitViewSelectionChanged(hit->id);
  RefreshAfterSelectionOnlyUpdate();
  return true;
}

// Maps a frame interaction mode to its wx cursor presentation.
wxCursor LayoutViewerPanel::CursorForMode(FrameDragMode mode) const {
  switch (mode) {
  case FrameDragMode::ResizeRight:
    return wxCursor(wxCURSOR_SIZEWE);
  case FrameDragMode::ResizeBottom:
    return wxCursor(wxCURSOR_SIZENS);
  case FrameDragMode::ResizeCorner:
    return wxCursor(wxCURSOR_SIZENWSE);
  case FrameDragMode::Move:
    return wxCursor(wxCURSOR_SIZING);
  case FrameDragMode::None:
  default:
    return wxCursor(wxCURSOR_ARROW);
  }
}
