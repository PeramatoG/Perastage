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

// Presents a moved scene transform before further pointer events run.
void Viewer2DPanel::PresentInteractiveTransformFrame() {
  wxASSERT_MSG(wxIsMainThread(),
               "Interactive presentation must run on the UI thread.");
  RequestRepaint();
  Update();
}

// Clears repaint coalescing after a paint event begins.
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
