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

bool Viewer2DOffscreenRenderer::RenderToTexture(const wxSize &size) {
  if (!panel_)
    return false;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return false;

  SetViewportSize(size);
  panel_->EnsureGLReady();
  EnsureOffscreenTarget(size);
  if (renderedFbo_ == 0 || renderedTexture_ == 0)
    return false;

  panel_->RenderToFramebuffer(renderedFbo_);
  return true;
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
  DestroyOffscreenTarget();
  if (panel_) {
    panel_->Destroy();
    panel_ = nullptr;
  }
  if (host_) {
    host_->Destroy();
    host_ = nullptr;
  }
}

void Viewer2DOffscreenRenderer::EnsureOffscreenTarget(const wxSize &size) {
  if (!panel_)
    return;

  if (renderedTexture_ == 0) {
    glGenTextures(1, &renderedTexture_);
  }
  if (renderedFbo_ == 0) {
    glGenFramebuffers(1, &renderedFbo_);
  }
  if (renderedDepthRbo_ == 0) {
    glGenRenderbuffers(1, &renderedDepthRbo_);
  }

  if (renderedTextureSize_ != size) {
    renderedTextureSize_ = size;
    glBindTexture(GL_TEXTURE_2D, renderedTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.GetWidth(),
                 size.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, renderedDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          size.GetWidth(), size.GetHeight());
  }

  glBindFramebuffer(GL_FRAMEBUFFER, renderedFbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         renderedTexture_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, renderedDepthRbo_);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewer2DOffscreenRenderer::DestroyOffscreenTarget() {
  if (panel_) {
    panel_->EnsureGLReady();
  }
  if (renderedDepthRbo_ != 0) {
    glDeleteRenderbuffers(1, &renderedDepthRbo_);
    renderedDepthRbo_ = 0;
  }
  if (renderedFbo_ != 0) {
    glDeleteFramebuffers(1, &renderedFbo_);
    renderedFbo_ = 0;
  }
  if (renderedTexture_ != 0) {
    glDeleteTextures(1, &renderedTexture_);
    renderedTexture_ = 0;
  }
  renderedTextureSize_ = wxSize(0, 0);
}
