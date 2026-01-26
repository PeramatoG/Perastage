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

#include "viewer2dpanel.h"
#include <wx/panel.h>
#include <wx/window.h>

class Viewer2DOffscreenRenderer {
public:
  explicit Viewer2DOffscreenRenderer(wxWindow *parent);
  ~Viewer2DOffscreenRenderer();

  Viewer2DPanel *GetPanel() const { return panel_; }
  void SetSharedContext(wxGLContext *sharedContext);
  void SetViewportSize(const wxSize &size);
  void PrepareForCapture(const wxSize &size);
  bool RenderToTexture(const wxSize &size);
  unsigned int GetRenderedTexture() const { return colorTex_; }
  wxSize GetRenderedTextureSize() const { return renderSize_; }

private:
  void CreatePanel();
  void DestroyPanel();
  void EnsureRenderTarget(const wxSize &size);
  void DestroyRenderTarget();
  wxWindow *parent_ = nullptr;
  wxPanel *host_ = nullptr;
  Viewer2DPanel *panel_ = nullptr;
  wxGLContext *sharedContext_ = nullptr;
  unsigned int fbo_ = 0;
  unsigned int colorTex_ = 0;
  unsigned int depthRb_ = 0;
  wxSize renderSize_{0, 0};
};
