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

Viewer2DOffscreenRenderer::Viewer2DOffscreenRenderer(wxWindow *parent) {
  parent_ = parent;
  CreatePanel();
}

Viewer2DOffscreenRenderer::~Viewer2DOffscreenRenderer() { DestroyPanel(); }

void Viewer2DOffscreenRenderer::SetSharedContext(wxGLContext *sharedContext) {
  if (sharedContext_ == sharedContext)
    return;
  sharedContext_ = sharedContext;
  DestroyPanel();
  CreatePanel();
}

void Viewer2DOffscreenRenderer::SetViewportSize(const wxSize &size) {
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  panel_->SetSize(size);
  panel_->SetClientSize(size);
}

void Viewer2DOffscreenRenderer::PrepareForCapture() {
  if (!panel_)
    return;
  panel_->LoadViewFromConfig();
  panel_->UpdateScene(true);
}

void Viewer2DOffscreenRenderer::CreatePanel() {
  if (!parent_)
    return;
  host_ = new wxPanel(parent_, wxID_ANY, wxDefaultPosition, wxSize(1, 1));
  host_->Hide();

  panel_ = new Viewer2DPanel(host_, true, false, false, sharedContext_);
  panel_->SetSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->SetClientSize(wxSize(kDefaultViewportWidth, kDefaultViewportHeight));
  panel_->LoadViewFromConfig();
  panel_->UpdateScene(true);
}

void Viewer2DOffscreenRenderer::DestroyPanel() {
  if (panel_) {
    panel_->Destroy();
    panel_ = nullptr;
  }
  if (host_) {
    host_->Destroy();
    host_ = nullptr;
  }
}
