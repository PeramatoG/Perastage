#include "viewer2dpanel.h"

// Invalidates capture ownership before cancelling transient interaction state.
void Viewer2DPanel::OnCaptureLost(wxMouseCaptureLostEvent &WXUNUSED(event)) {
  m_mouseCapture.AbandonOnLoss("active-gesture");
  FinishInteractiveTransformPresentation();
  m_interaction.Cancel(m_placementSession.IsActive());
  m_pendingMagnetSnap.reset();
  ClearCursorWorldPosition();
}
