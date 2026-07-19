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
#include "viewer2doffscreenrenderer.h"

namespace {
constexpr int kDefaultViewportWidth = 1600;
constexpr int kDefaultViewportHeight = 900;
}

// Creates an offscreen 2D renderer host that is mapped but never visible in the main UI.
Viewer2DOffscreenRenderer::Viewer2DOffscreenRenderer(wxWindow *parent) {
#if defined(__WXGTK__) || defined(__WXOSX__)
  host_ = new wxFrame(parent, wxID_ANY, wxString(), wxPoint(-32000, -32000),
                      wxSize(1, 1),
                      wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxBORDER_NONE);
  host_->Show();
#else
  host_ = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(1, 1));
  host_->Hide();
#endif

  panel_ = new Viewer2DPanel(host_, true, false, false);
  panel_->SetSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->SetClientSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->SetRenderOverrides(std::nullopt);
}

// Destroys the offscreen host window and releases the capture panel.
Viewer2DOffscreenRenderer::~Viewer2DOffscreenRenderer() {
  if (host_) {
    host_->Destroy();
    host_ = nullptr;
    panel_ = nullptr;
  }
}

// Updates the offscreen capture viewport size used by the embedded 2D panel.
void Viewer2DOffscreenRenderer::SetViewportSize(const wxSize &size) {
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  panel_->SetSize(size);
  panel_->SetClientSize(size);
}

// Resets capture overrides and refreshes scene data before a new capture.
void Viewer2DOffscreenRenderer::PrepareForCapture() {
  if (!panel_)
    return;
  panel_->SetRenderOverrides(std::nullopt);
}

// Applies symbol-capture defaults for legend and symbol texture rendering.
void Viewer2DOffscreenRenderer::ApplySymbolCaptureDefaults() {
  if (!panel_)
    return;

  Viewer2DRenderOverrides overrides;
  overrides.darkMode = false;
  overrides.showGrid = false;
  overrides.showRuler = false;
  overrides.drawFixtureLabels = false;
  overrides.forceBottomViewForTopFixtures = false;
  overrides.symbolCaptureRenderProfile = true;
  overrides.symbolCaptureIncludeCoplanarEdges = true;

  panel_->SetRenderOverrides(overrides);
  panel_->SetPreferPerastageSvgSymbolsForLayouts(false);
  panel_->ApplyViewState(0.0f, 0.0f, 1.0f, Viewer2DView::Top,
                         Viewer2DRenderMode::ByFixtureType);
}
