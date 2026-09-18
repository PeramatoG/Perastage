#include "mouse_capture_ownership.h"

#include "diagnostics/DiagnosticLogger.h"

#include <wx/window.h>

#include <utility>

namespace ui {

// Creates an ownership guard for one wx window.
MouseCaptureOwner::MouseCaptureOwner(wxWindow &window, std::string component)
    : m_window(window), m_component(std::move(component)) {}

// Acquires capture only after validating tracked and native ownership.
bool MouseCaptureOwner::TryAcquire(const std::string &gesture) {
  wxWindow *const nativeOwner = wxWindow::GetCapture();
  if (m_state.IsOwned()) {
    if (nativeOwner != &m_window)
      LogAnomaly("acquire", gesture, "tracked capture no longer matches wx");
    else
      LogAnomaly("acquire", gesture, "duplicate acquire skipped");
    return false;
  }
  if (nativeOwner != nullptr) {
    LogAnomaly("acquire", gesture,
               nativeOwner == &m_window
                   ? "native capture is untracked; acquire skipped"
                   : "another window owns native capture; acquire skipped");
    return false;
  }

  m_window.CaptureMouse();
  m_state.MarkAcquired();
  return true;
}

// Releases only when both Perastage and wx still identify this owner.
bool MouseCaptureOwner::Release(const std::string &gesture) {
  wxWindow *const nativeOwner = wxWindow::GetCapture();
  const bool tracked = m_state.IsOwned();
  const bool nativeMatches = nativeOwner == &m_window;
  if (!tracked) {
    LogAnomaly("release", gesture,
               nativeMatches ? "native capture is untracked; release skipped"
                             : "duplicate or unowned release skipped");
    return false;
  }
  if (!m_state.ConsumeForRelease(nativeMatches)) {
    LogAnomaly("release", gesture,
               "tracked capture no longer matches wx; release skipped");
    return false;
  }

  m_window.ReleaseMouse();
  return true;
}

// Invalidates ownership immediately when wx reports capture loss.
void MouseCaptureOwner::AbandonOnLoss(const std::string &gesture) {
  const bool wasOwned = m_state.AbandonOnLoss();
  LogAnomaly("capture-lost", gesture,
             wasOwned ? "active owned capture was lost"
                      : "capture loss had no tracked ownership");
}

// Writes a concise capture-lifecycle anomaly to the application log.
void MouseCaptureOwner::LogAnomaly(const std::string &operation,
                                   const std::string &gesture,
                                   const std::string &detail) const {
  const wxWindow *const nativeOwner = wxWindow::GetCapture();
  diagnostics::DiagnosticLogger::Warning(
      m_component + " mouse capture: operation=" + operation +
      " gesture=" + gesture + " tracked=" +
      (m_state.IsOwned() ? "owned" : "idle") + " native=" +
      (nativeOwner == &m_window ? "self"
                                : (nativeOwner ? "other" : "none")) +
      " detail=" + detail);
}

} // namespace ui
