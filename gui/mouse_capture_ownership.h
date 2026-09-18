#pragma once

#include <string>

class wxWindow;

namespace ui {

// Tracks whether Perastage initiated a balanced wx mouse-capture lifecycle.
class MouseCaptureOwnershipState {
public:
  // Records a successful application-initiated capture.
  bool MarkAcquired() {
    if (m_owned)
      return false;
    m_owned = true;
    return true;
  }

  // Consumes ownership when a release can safely be issued.
  bool ConsumeForRelease(bool nativeCaptureMatches) {
    if (!m_owned)
      return false;
    m_owned = false;
    return nativeCaptureMatches;
  }

  // Invalidates ownership after wx reports capture loss.
  bool AbandonOnLoss() {
    const bool wasOwned = m_owned;
    m_owned = false;
    return wasOwned;
  }

  // Reports whether Perastage currently owns the matching capture lifecycle.
  bool IsOwned() const { return m_owned; }

private:
  bool m_owned = false;
};

// Balances wx mouse capture using explicit application-side ownership.
class MouseCaptureOwner {
public:
  MouseCaptureOwner(wxWindow &window, std::string component);

  // Acquires capture only when neither wx nor this owner is already captured.
  bool TryAcquire(const std::string &gesture);
  // Releases only a capture initiated and still natively held by this owner.
  bool Release(const std::string &gesture);
  // Invalidates tracked ownership before capture-loss cancellation runs.
  void AbandonOnLoss(const std::string &gesture);
  // Reports application-side ownership for interaction invariants and tests.
  bool IsOwned() const { return m_state.IsOwned(); }

private:
  void LogAnomaly(const std::string &operation, const std::string &gesture,
                  const std::string &detail) const;

  wxWindow &m_window;
  std::string m_component;
  MouseCaptureOwnershipState m_state;
};

} // namespace ui
