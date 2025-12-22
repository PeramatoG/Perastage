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

#include <algorithm>
#include <cmath>
#include <wx/dcbuffer.h>

namespace {
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 10.0;
constexpr double kZoomStep = 1.1;
constexpr int kFitMarginPx = 40;
}

wxBEGIN_EVENT_TABLE(LayoutViewerPanel, wxPanel)
    EVT_PAINT(LayoutViewerPanel::OnPaint)
    EVT_SIZE(LayoutViewerPanel::OnSize)
    EVT_LEFT_DOWN(LayoutViewerPanel::OnLeftDown)
    EVT_LEFT_UP(LayoutViewerPanel::OnLeftUp)
    EVT_MOTION(LayoutViewerPanel::OnMouseMove)
    EVT_MOUSEWHEEL(LayoutViewerPanel::OnMouseWheel)
    EVT_MOUSE_CAPTURE_LOST(LayoutViewerPanel::OnCaptureLost)
wxEND_EVENT_TABLE()

LayoutViewerPanel::LayoutViewerPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  currentLayout.pageSetup.pageSize = print::PageSize::A4;
  currentLayout.pageSetup.landscape = false;
  ResetViewToFit();
}

void LayoutViewerPanel::SetLayoutDefinition(
    const layouts::LayoutDefinition &layout) {
  currentLayout = layout;
  ResetViewToFit();
  Refresh();
}

void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  wxAutoBufferedPaintDC dc(this);
  dc.Clear();

  wxSize size = GetClientSize();
  dc.SetBrush(wxBrush(wxColour(90, 90, 90)));
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();

  const double scaledWidth = pageWidth * zoom;
  const double scaledHeight = pageHeight * zoom;

  const wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  const wxPoint topLeft(center.x - static_cast<int>(scaledWidth / 2.0) +
                            panOffset.x,
                        center.y - static_cast<int>(scaledHeight / 2.0) +
                            panOffset.y);

  dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
  dc.SetPen(wxPen(wxColour(200, 200, 200)));
  dc.DrawRectangle(topLeft.x, topLeft.y, static_cast<int>(scaledWidth),
                   static_cast<int>(scaledHeight));
}

void LayoutViewerPanel::OnSize(wxSizeEvent &) {
  Refresh();
}

void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  isPanning = true;
  lastMousePos = event.GetPosition();
  CaptureMouse();
}

void LayoutViewerPanel::OnLeftUp(wxMouseEvent &) {
  if (isPanning) {
    isPanning = false;
    if (HasCapture())
      ReleaseMouse();
  }
}

void LayoutViewerPanel::OnMouseMove(wxMouseEvent &event) {
  if (!isPanning || !event.Dragging())
    return;

  wxPoint currentPos = event.GetPosition();
  wxPoint delta = currentPos - lastMousePos;
  panOffset += delta;
  lastMousePos = currentPos;
  Refresh();
}

void LayoutViewerPanel::OnMouseWheel(wxMouseEvent &event) {
  const int rotation = event.GetWheelRotation();
  const int delta = event.GetWheelDelta();
  if (delta == 0 || rotation == 0)
    return;

  const double steps = static_cast<double>(rotation) /
                       static_cast<double>(delta);
  const double factor = std::pow(kZoomStep, steps);
  const double newZoom = std::clamp(zoom * factor, kMinZoom, kMaxZoom);
  if (std::abs(newZoom - zoom) < 1e-6)
    return;

  wxSize size = GetClientSize();
  wxPoint center(size.GetWidth() / 2, size.GetHeight() / 2);
  wxPoint mousePos = event.GetPosition();

  wxPoint relative = mousePos - center - panOffset;
  const double scale = newZoom / zoom;
  wxPoint newRelative(static_cast<int>(relative.x * scale),
                      static_cast<int>(relative.y * scale));

  panOffset += relative - newRelative;
  zoom = newZoom;
  Refresh();
}

void LayoutViewerPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  isPanning = false;
}

void LayoutViewerPanel::ResetViewToFit() {
  wxSize size = GetClientSize();
  const double pageWidth = currentLayout.pageSetup.PageWidthPt();
  const double pageHeight = currentLayout.pageSetup.PageHeightPt();

  if (pageWidth <= 0.0 || pageHeight <= 0.0 || size.GetWidth() <= 0 ||
      size.GetHeight() <= 0) {
    zoom = 1.0;
    panOffset = wxPoint(0, 0);
    return;
  }

  const double fitWidth =
      static_cast<double>(size.GetWidth() - kFitMarginPx) / pageWidth;
  const double fitHeight =
      static_cast<double>(size.GetHeight() - kFitMarginPx) / pageHeight;
  zoom = std::clamp(std::min(fitWidth, fitHeight), kMinZoom, kMaxZoom);
  panOffset = wxPoint(0, 0);
}
