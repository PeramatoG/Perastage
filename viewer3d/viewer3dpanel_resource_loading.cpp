#include "viewer3dpanel.h"
#include "logger.h"
#include "performance_metrics.h"

// The refresh signal pumps work independently of paint/vsync. A slow paint
// must not reduce throughput to one tiny resource batch per rendered frame.
bool Viewer3DPanel::ProcessProgressiveResourceRefresh() {
  auto progress = m_controller.GetResourceLoadProgress();
  if (progress.Pending()) m_loadCompletionReported = false;
  if (!m_controller.IsResourceSyncPending() && !progress.Pending()) return false;
  if (!IsShownOnScreen() || !IsEnabled()) return true;
  const bool paused = ShouldPauseHeavyTasks() || m_runtimeState.IsCameraMoving();
  m_loadStatus.Update(*this, progress, paused);
  if (paused || !PrepareGlResourceSync("progressive resource loading")) return true;

  m_controller.UpdateFrameStateLightweight();
  if (m_controller.ConsumeResourceSyncPending())
    m_controller.UpdateResourcesIfDirty(true, true);
  else
    m_controller.PreparePendingRenderMeshes();

  progress = m_controller.GetResourceLoadProgress();
  // Completion is announced by OnPaint after the final geometry is presented.
  if (progress.Pending()) m_loadStatus.Update(*this, progress, false);
  if (m_loadPresentationCadence.PresentationDue(
          std::chrono::steady_clock::now(), !progress.Pending())) {
    m_runtimeState.MarkSceneChanged();
    Refresh(false);
  }
  return true;
}

void Viewer3DPanel::ResourceLoadingPresented(bool paused) {
  const auto now = std::chrono::steady_clock::now();
  m_loadPresentationCadence.Presented(now);
  const auto progress = m_controller.GetResourceLoadProgress();
  m_loadStatus.Update(*this, progress, paused);
  if (progress.Pending()) m_loadCompletionReported = false;
  else if (progress.total > 0 && !m_loadCompletionReported) {
    m_loadCompletionReported = true;
    Logger::Instance().Log("Viewer scene presentation complete: resources=" +
        std::to_string(progress.completed) + " unavailable=" + std::to_string(progress.failed) +
        (performance::Enabled() ? " " + performance::Summary() : ""));
  }
}
