#include "viewer2dpanel.h"

#include <chrono>

// Queues a coalesced full repaint.
void Viewer2DPanel::RequestRepaint() {
  if (m_fullRepaintQueued)
    return;
  m_repaintQueued = true;
  m_fullRepaintQueued = true;
  TrackRefreshTelemetry();
  Refresh(false);
}

// Queues a dirty-region repaint when no broader repaint is pending.
void Viewer2DPanel::RequestRepaint(const wxRect &dirtyRect) {
  if (!dirtyRect.IsEmpty() && !m_fullRepaintQueued && !m_repaintQueued) {
    m_repaintQueued = true;
    TrackRefreshTelemetry();
    RefreshRect(dirtyRect, false);
    return;
  }
  RequestRepaint();
}

// Clears repaint coalescing after paint begins.
void Viewer2DPanel::ResetRepaintCoalescing() {
  m_repaintQueued = false;
  m_fullRepaintQueued = false;
}

// Records debug-only repaint request telemetry.
void Viewer2DPanel::TrackRefreshTelemetry() {
#ifndef NDEBUG
  const auto now = std::chrono::steady_clock::now();
  if (m_refreshTelemetryWindowStart.time_since_epoch().count() == 0)
    m_refreshTelemetryWindowStart = now;
  ++m_refreshesInCurrentWindow;
  const auto elapsed = now - m_refreshTelemetryWindowStart;
  if (elapsed >= std::chrono::seconds(1)) {
    wxLogDebug("Viewer2DPanel refreshes/s: %d", m_refreshesInCurrentWindow);
    m_refreshTelemetryWindowStart = now;
    m_refreshesInCurrentWindow = 0;
  }
#endif
}


// Presents the newest transform at a bounded interactive frame cadence.
void Viewer2DPanel::PresentInteractiveTransformFrame() {
  RequestRepaint();
  if (!m_paintInProgress && m_interactivePresentationCadence.IsPresentationDue(
                                std::chrono::steady_clock::now()))
    Update();
}

// Flushes the final transform and reconciles transform-dependent caches once.
void Viewer2DPanel::FinishInteractiveTransformPresentation() {
  if (m_activeTransformTargets.empty())
    return;
  m_activeTransformTargets.clear();
  m_controller.MarkSceneTransformsDirty();
  m_interactivePresentationCadence.Reset();
  RequestRepaint();
  if (!m_paintInProgress)
    Update();
}
