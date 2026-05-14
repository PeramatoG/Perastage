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
#include "logger.h"

namespace {
constexpr int kDefaultViewportWidth = 1600;
constexpr int kDefaultViewportHeight = 900;
}

// Creates an offscreen-capable Viewer2D panel that stays hidden from the visible UI hierarchy.
Viewer2DOffscreenRenderer::Viewer2DOffscreenRenderer(wxWindow *parent) {
  host_ = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(1, 1));
  host_->Hide();

  panel_ = new Viewer2DPanel(host_, true, false, false);
  panel_->SetSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->SetClientSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->SetRenderOverrides(std::nullopt);
  panel_->UpdateScene(true);
}

// Releases the offscreen host hierarchy owned by this renderer.
Viewer2DOffscreenRenderer::~Viewer2DOffscreenRenderer() {
  if (host_) {
    host_->Destroy();
    host_ = nullptr;
    panel_ = nullptr;
  }
}

// Applies the capture viewport size to the underlying offscreen Viewer2D panel.
void Viewer2DOffscreenRenderer::SetViewportSize(const wxSize &size) {
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  const wxSize panelBefore = panel_->GetClientSize();
  panel_->SetMinSize(size);
  panel_->SetSize(size);
  panel_->SetClientSize(size);
  Logger::Instance().Log("Viewer2DOffscreenRenderer::SetViewportSize panelBefore=" +
                         std::to_string(panelBefore.GetWidth()) + "x" +
                         std::to_string(panelBefore.GetHeight()) + " panelAfter=" +
                         std::to_string(panel_->GetClientSize().GetWidth()) + "x" +
                         std::to_string(panel_->GetClientSize().GetHeight()));
}

// Prepares the offscreen Viewer2D panel state before a capture pass.
void Viewer2DOffscreenRenderer::PrepareForCapture() {
  if (!panel_)
    return;
  Logger::Instance().Log("Viewer2DOffscreenRenderer::PrepareForCapture panelShown=" +
                         std::to_string(panel_->IsShown() ? 1 : 0));
  panel_->SetRenderOverrides(std::nullopt);
  panel_->UpdateScene(true);
}

// Applies capture-friendly render defaults used by symbol snapshot workflows.
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
  panel_->UpdateScene(true);
}
