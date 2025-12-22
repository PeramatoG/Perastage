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

#include <wx/wx.h>
#include "layouts/LayoutCollection.h"

class LayoutViewerPanel : public wxPanel {
public:
  explicit LayoutViewerPanel(wxWindow *parent);

  void SetLayoutDefinition(const layouts::LayoutDefinition &layout);

private:
  void OnPaint(wxPaintEvent &event);
  void OnSize(wxSizeEvent &event);
  void OnLeftDown(wxMouseEvent &event);
  void OnLeftUp(wxMouseEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnMouseWheel(wxMouseEvent &event);
  void OnCaptureLost(wxMouseCaptureLostEvent &event);

  void ResetViewToFit();

  layouts::LayoutDefinition currentLayout;
  double zoom = 1.0;
  wxPoint panOffset{0, 0};
  bool isPanning = false;
  wxPoint lastMousePos{0, 0};

  wxDECLARE_EVENT_TABLE();
};
