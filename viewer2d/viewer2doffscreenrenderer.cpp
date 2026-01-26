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

#include "configmanager.h"

#include <wx/app.h>

namespace {
constexpr int kDefaultViewportWidth = 1600;
constexpr int kDefaultViewportHeight = 900;
}

Viewer2DOffscreenRenderer::Viewer2DOffscreenRenderer(wxWindow *parent) {
  parent_ = parent;
  CreatePanel();
  StartWorker();
}

Viewer2DOffscreenRenderer::~Viewer2DOffscreenRenderer() {
  StopWorker();
  DestroyPanel();
}

Viewer2DOffscreenRenderer::PanelLock Viewer2DOffscreenRenderer::AcquirePanel() {
  PanelLock lock;
  lock.lock = std::unique_lock<std::mutex>(panelMutex_);
  lock.panel = panel_;
  return lock;
}

void Viewer2DOffscreenRenderer::SetSharedContext(wxGLContext *sharedContext) {
  if (sharedContext_ == sharedContext)
    return;
  sharedContext_ = sharedContext;
  {
    std::lock_guard<std::mutex> lock(jobMutex_);
    workerSharedContext_ = nullptr;
  }
  jobCv_.notify_all();
  DestroyPanel();
  CreatePanel();
}

void Viewer2DOffscreenRenderer::SetViewportSize(const wxSize &size) {
  std::lock_guard<std::mutex> lock(panelMutex_);
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  panel_->SetExternalContext(nullptr);
  panel_->SetSize(size);
  panel_->SetClientSize(size);
}

void Viewer2DOffscreenRenderer::PrepareForCapture(const wxSize &size) {
  std::lock_guard<std::mutex> lock(panelMutex_);
  PrepareForCaptureLocked(size);
}

bool Viewer2DOffscreenRenderer::RenderToTexture(const wxSize &size) {
  std::lock_guard<std::mutex> lock(panelMutex_);
  if (!panel_)
    return false;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return false;

  panel_->SetExternalContext(nullptr);
  PrepareForCaptureLocked(size);
  return fbo_ != 0 && colorTex_ != 0;
}

void Viewer2DOffscreenRenderer::EnqueueViewRender(
    int viewId, const viewer2d::Viewer2DState &renderState, const wxSize &size,
    size_t renderToken, double renderZoom,
    std::function<void(const RenderResult &)> callback) {
  if (viewId < 0)
    return;
  RenderJob job;
  job.viewId = viewId;
  job.renderState = renderState;
  job.size = size;
  job.renderToken = renderToken;
  job.renderZoom = renderZoom;
  job.callback = std::move(callback);
  {
    std::lock_guard<std::mutex> lock(jobMutex_);
    const auto [it, inserted] = jobQueue_.emplace(viewId, job);
    if (!inserted) {
      it->second = std::move(job);
      return;
    }
    jobOrder_.push_back(viewId);
  }
  jobCv_.notify_one();
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
  std::lock_guard<std::mutex> lock(panelMutex_);
  DestroyRenderTarget();
  workerContext_.reset();
  workerSharedContext_ = nullptr;
  if (panel_) {
    panel_->Destroy();
    panel_ = nullptr;
  }
  if (host_) {
    host_->Destroy();
    host_ = nullptr;
  }
}

void Viewer2DOffscreenRenderer::StartWorker() {
  workerStop_ = false;
  workerThread_ = std::thread([this]() { WorkerLoop(); });
}

void Viewer2DOffscreenRenderer::StopWorker() {
  {
    std::lock_guard<std::mutex> lock(jobMutex_);
    workerStop_ = true;
  }
  jobCv_.notify_all();
  if (workerThread_.joinable())
    workerThread_.join();
}

void Viewer2DOffscreenRenderer::WorkerLoop() {
  while (true) {
    RenderJob job;
    {
      std::unique_lock<std::mutex> lock(jobMutex_);
      jobCv_.wait(lock, [this]() { return workerStop_ || !jobOrder_.empty(); });
      if (workerStop_)
        return;
      const int viewId = jobOrder_.front();
      jobOrder_.pop_front();
      auto it = jobQueue_.find(viewId);
      if (it == jobQueue_.end())
        continue;
      job = std::move(it->second);
      jobQueue_.erase(it);
    }

    RenderResult result = RenderJobToTexture(job);
    if (job.callback) {
      wxTheApp->CallAfter(
          [callback = job.callback, result]() { callback(result); });
    }
  }
}

void Viewer2DOffscreenRenderer::EnsureWorkerContext() {
  if (!panel_)
    return;
  if (sharedContext_ == nullptr)
    return;
  if (!workerContext_ || workerSharedContext_ != sharedContext_) {
    workerContext_.reset();
    workerSharedContext_ = sharedContext_;
    workerContext_ = std::make_unique<wxGLContext>(panel_, sharedContext_);
  }
}

Viewer2DOffscreenRenderer::RenderResult
Viewer2DOffscreenRenderer::RenderJobToTexture(const RenderJob &job) {
  RenderResult result;
  result.viewId = job.viewId;
  result.renderToken = job.renderToken;
  result.renderZoom = job.renderZoom;
  result.size = job.size;

  std::lock_guard<std::mutex> lock(panelMutex_);
  if (!panel_ || job.size.GetWidth() <= 0 || job.size.GetHeight() <= 0)
    return result;
  EnsureWorkerContext();
  if (!workerContext_)
    return result;

  panel_->SetExternalContext(workerContext_.get());
  panel_->SetRenderViewportOverride(job.size);

  ConfigManager &cfg = ConfigManager::Get();
  viewer2d::Viewer2DState renderState = job.renderState;
  auto stateGuard = std::make_shared<viewer2d::ScopedViewer2DState>(
      panel_, nullptr, cfg, renderState, nullptr, nullptr, false);
  static_cast<void>(stateGuard);

  panel_->EnsureGLReady();
  EnsureRenderTarget(job.size);
  panel_->RenderToTexture(fbo_, job.size);
  result.texture = colorTex_;
  result.success = fbo_ != 0 && colorTex_ != 0;

  panel_->SetRenderViewportOverride(std::nullopt);
  panel_->SetExternalContext(nullptr);
  return result;
}

void Viewer2DOffscreenRenderer::PrepareForCaptureLocked(const wxSize &size) {
  if (!panel_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  panel_->SetExternalContext(nullptr);
  panel_->SetSize(size);
  panel_->SetClientSize(size);
  panel_->EnsureGLReady();
  EnsureRenderTarget(size);
  panel_->RenderToTexture(fbo_, size);
}

void Viewer2DOffscreenRenderer::EnsureRenderTarget(const wxSize &size) {
  if (!panel_)
    return;

  if (colorTex_ == 0) {
    glGenTextures(1, &colorTex_);
  }
  if (fbo_ == 0) {
    glGenFramebuffers(1, &fbo_);
  }
  if (depthRb_ == 0) {
    glGenRenderbuffers(1, &depthRb_);
  }

  if (renderSize_ != size) {
    renderSize_ = size;
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.GetWidth(),
                 size.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          size.GetWidth(), size.GetHeight());
  }

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTex_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depthRb_);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewer2DOffscreenRenderer::DestroyRenderTarget() {
  if (panel_) {
    panel_->EnsureGLReady();
  }
  if (depthRb_ != 0) {
    glDeleteRenderbuffers(1, &depthRb_);
    depthRb_ = 0;
  }
  if (fbo_ != 0) {
    glDeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
  if (colorTex_ != 0) {
    glDeleteTextures(1, &colorTex_);
    colorTex_ = 0;
  }
  renderSize_ = wxSize(0, 0);
}
