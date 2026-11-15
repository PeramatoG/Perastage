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
#include <GL/glew.h>
#include "layoutviewerpanel.h"
#include "LayoutManager.h"
#include <wx/log.h>

std::vector<gui::layoutselection::SelectionFrame> LayoutViewerPanel::SelectionFrames() {
  EnsureSelectionIndexCache();
  std::vector<gui::layoutselection::SelectionFrame> frames;
  for (const auto &entry : selectionIndexCache_.zOrderedElements) {
    layouts::Layout2DViewFrame frame;
    wxRect rect;
    if (GetElementFrame(entry.element, frame) &&
        GetFrameRect(GetDisplayedFrame(entry.element, frame), rect))
      frames.push_back({entry.element, double(rect.x), double(rect.y),
                        double(rect.width), double(rect.height)});
  }
  return frames;
}
std::optional<layouts::LayoutElementRef> LayoutViewerPanel::HitElement(const wxPoint &pos) {
  const auto hit = gui::layoutselection::HitFrame(SelectionFrames(), pos.x, pos.y);
  return hit.kind == LayoutElementKind::None ? std::nullopt : std::optional(hit);
}

bool LayoutViewerPanel::HandleSelectionDown(wxMouseEvent &event, const wxPoint &pos) {
  const auto hit = HitElement(pos);
  if (event.ControlDown() || event.ShiftDown()) {
    if (hit) {
      if (event.ControlDown()) selectionState_.Toggle(*hit);
      else selectionState_.Add(*hit);
    } else if (event.ShiftDown()) {
      marqueeStart_ = marqueeEnd_ = pos;
      if (!mouseCapture_.TryAcquire("selection-marquee")) marqueeStart_.reset();
    }
    if (selectionState_.Current().kind == LayoutElementKind::View2D)
      EmitViewSelectionChanged(selectionState_.Current().id);
    Refresh();
    return true;
  }
  if (!hit) {
    // Resize handles extend beyond the frame interior. Keep their existing
    // single-selection hit area before treating the point as empty page.
    if (SelectedElementCount() == 1) {
      layouts::Layout2DViewFrame frame;
      wxRect rect;
      if (GetSelectedFrame(frame) && GetFrameRect(GetDisplayedFrame(selectionState_.Current(),frame),rect)) {
        const auto mode = gui::layoutinteraction::HitTestFrame(
            {pos.x,pos.y}, {rect.x,rect.y,rect.width,rect.height});
        if (mode != FrameDragMode::None && mode != FrameDragMode::Move) return false;
      }
    }
    selectionState_.Clear();
    Refresh();
    return false; // Ordinary empty-page drag pans.
  }
  if (!selectionState_.Matches(*hit)) selectionState_.Select(hit->kind, hit->id);
  if (hit->kind == LayoutElementKind::View2D) EmitViewSelectionChanged(hit->id);
  Refresh();
  if (SelectedElementCount() == 1) {
    layouts::Layout2DViewFrame frame;
    wxRect rect;
    if (GetElementFrame(*hit, frame) && GetFrameRect(GetDisplayedFrame(*hit, frame), rect)) {
      const auto mode = gui::layoutinteraction::HitTestFrame(
          {pos.x,pos.y}, {rect.x,rect.y,rect.width,rect.height});
      if (mode != FrameDragMode::Move && mode != FrameDragMode::None) return false;
    }
  }
  try {
    groupGesture_.Begin(currentLayout, selectionState_.Ordered(), pos.x, pos.y, viewportState_.Zoom());
    groupClick_ = hit;
    if (!mouseCapture_.TryAcquire("frame-drag")) groupGesture_.Cancel();
  } catch (const std::exception &e) {
    wxLogWarning("%s", wxString::FromUTF8(e.what()));
  }
  return true;
}
bool LayoutViewerPanel::HandleSelectionMotion(wxMouseEvent &event, const wxPoint &pos) {
  if (marqueeStart_) {
    marqueeEnd_ = pos;
    Refresh();
    return true;
  }
  if (!groupGesture_.Active()) return false;
  if (event.Dragging()) {
    try {
      currentLayout = groupGesture_.Preview(pos.x, pos.y);
      InvalidateSelectionIndexCache();
      SetCursor(wxCursor(wxCURSOR_SIZING));
      Refresh(); // Placement only: retain scene capture and element textures.
    } catch (const std::exception &e) {
      wxLogWarning("%s", wxString::FromUTF8(e.what()));
      CancelFrameGesture();
      mouseCapture_.Release("frame-drag");
    }
  }
  return true;
}
bool LayoutViewerPanel::HandleSelectionUp(const wxPoint &pos) {
  if (marqueeStart_) marqueeEnd_ = pos;
  if (groupGesture_.Active()) {
    try { currentLayout = groupGesture_.Preview(pos.x,pos.y); }
    catch (const std::exception &e) {
      wxLogWarning("%s", wxString::FromUTF8(e.what()));
      CancelFrameGesture();
      mouseCapture_.Release("frame-drag");
      return true;
    }
  }
  if (marqueeStart_) {
    const wxRect rect(*marqueeStart_, marqueeEnd_);
    const auto refs = gui::layoutselection::IntersectingFrames(
        SelectionFrames(), rect.x, rect.y, rect.width, rect.height);
    for (auto ref : refs) selectionState_.Add(ref);
    marqueeStart_.reset();
    mouseCapture_.Release("selection-marquee");
    Refresh();
    return true;
  }
  if (!groupGesture_.Active()) return false;
  const auto refs = groupGesture_.Refs();
  const double dx = groupGesture_.DeltaX(), dy = groupGesture_.DeltaY();
  currentLayout = groupGesture_.Original();
  groupGesture_.Cancel();
  InvalidateSelectionIndexCache();
  mouseCapture_.Release("frame-drag");
  std::string error;
  if (!layouts::LayoutManager::Get().TranslateElements(currentLayout.name, refs, dx, dy, &error))
    wxLogWarning("%s", wxString::FromUTF8(error));
  if (dx == 0 && dy == 0 && groupClick_)
    selectionState_.Select(groupClick_->kind, groupClick_->id);
  groupClick_.reset();
  ReloadDocumentPlacement();
  return true;
}

// Apply persisted placements without invalidating expensive scene captures.
void LayoutViewerPanel::ReloadDocumentPlacement() {
  for (const auto &page : layouts::LayoutManager::Get().GetLayouts().Items()) {
    if (page.name != currentLayout.name) continue;
    currentLayout = page;
    InvalidateSelectionIndexCache();
    selectionState_.Retain(BuildElementIdsByKind());
    Refresh();
    return;
  }
  selectionState_.Clear();
}
bool LayoutViewerPanel::AlignSelectedElements(alignment::Reference reference,
    alignment::Operation operation, bool origins) {
  if (groupGesture_.Active() || marqueeStart_ ||
      interactionSession_.DragMode() != FrameDragMode::None) {
    CancelFrameGesture();
    mouseCapture_.Release("frame-drag");
  }
  std::string error;
  if (!layouts::LayoutManager::Get().AlignElements(currentLayout.name,
      selectionState_.Ordered(), reference, operation, origins, &error)) {
    wxLogWarning("%s", wxString::FromUTF8(error));
    return false;
  }
  ReloadDocumentPlacement();
  return true;
}

void LayoutViewerPanel::DrawDocumentSelection() const {
  const auto outline = [](const wxRect &r) {
    glBegin(GL_LINE_LOOP);
    glVertex2d(r.GetLeft(), r.GetTop()); glVertex2d(r.GetRight()+1, r.GetTop());
    glVertex2d(r.GetRight()+1, r.GetBottom()+1); glVertex2d(r.GetLeft(), r.GetBottom()+1);
    glEnd();
  };
  wxRect combined;
  bool first = true;
  for (auto ref : selectionState_.Ordered()) {
    layouts::Layout2DViewFrame frame;
    wxRect rect;
    if (!GetElementFrame(ref, frame) || !GetFrameRect(GetDisplayedFrame(ref, frame), rect)) continue;
    if (first) { combined = rect; first = false; } else combined.Union(rect);
    const bool primary = ref == selectionState_.Current();
    glColor4ub(primary ? 30 : 60, primary ? 110 : 160, 240, 255);
    glLineWidth(primary ? 2.0f : 1.0f);
    outline(rect);
  }
  glLineWidth(1.0f);
  glColor4ub(60,160,240,255);
  if (SelectedElementCount() > 1 && !first) { combined.Inflate(4); outline(combined); }
  if (marqueeStart_) outline(wxRect(*marqueeStart_, marqueeEnd_));
}
