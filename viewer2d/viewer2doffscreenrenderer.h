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
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <wx/panel.h>
#include <wx/window.h>

class Viewer2DOffscreenRenderer {
public:
  explicit Viewer2DOffscreenRenderer(wxWindow *parent);
  ~Viewer2DOffscreenRenderer();

  struct PanelLock {
    std::unique_lock<std::mutex> lock;
    Viewer2DPanel *panel = nullptr;
  };

  struct RenderResult {
    int viewId = -1;
    size_t renderToken = 0;
    double renderZoom = 0.0;
    wxSize size{0, 0};
    unsigned int texture = 0;
    bool success = false;
  };

  Viewer2DPanel *GetPanel() const { return panel_; }
  PanelLock AcquirePanel();
  void SetSharedContext(wxGLContext *sharedContext);
  void SetViewportSize(const wxSize &size);
  void PrepareForCapture(const wxSize &size);
  bool RenderToTexture(const wxSize &size);
  unsigned int GetRenderedTexture() const { return colorTex_; }
  wxSize GetRenderedTextureSize() const { return renderSize_; }
  void EnqueueViewRender(
      int viewId, const viewer2d::Viewer2DState &renderState,
      const wxSize &size, size_t renderToken, double renderZoom,
      std::function<void(const RenderResult &)> callback);

private:
  struct RenderJob {
    int viewId = -1;
    viewer2d::Viewer2DState renderState;
    wxSize size{0, 0};
    size_t renderToken = 0;
    double renderZoom = 0.0;
    std::function<void(const RenderResult &)> callback;
  };

  void CreatePanel();
  void DestroyPanel();
  void StartWorker();
  void StopWorker();
  void WorkerLoop();
  void EnsureWorkerContext();
  RenderResult RenderJobToTexture(const RenderJob &job);
  void PrepareForCaptureLocked(const wxSize &size);
  void EnsureRenderTarget(const wxSize &size);
  void DestroyRenderTarget();
  wxWindow *parent_ = nullptr;
  wxPanel *host_ = nullptr;
  Viewer2DPanel *panel_ = nullptr;
  wxGLContext *sharedContext_ = nullptr;
  wxGLContext *workerSharedContext_ = nullptr;
  std::unique_ptr<wxGLContext> workerContext_;
  std::mutex panelMutex_;
  std::mutex jobMutex_;
  std::condition_variable jobCv_;
  std::unordered_map<int, RenderJob> jobQueue_;
  std::deque<int> jobOrder_;
  bool workerStop_ = false;
  std::thread workerThread_;
  unsigned int fbo_ = 0;
  unsigned int colorTex_ = 0;
  unsigned int depthRb_ = 0;
  wxSize renderSize_{0, 0};
};
