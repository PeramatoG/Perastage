#include "viewer2d_line_point_selection_session.h"

namespace viewer2d::interaction {

// Starts a fresh selection constrained to the supplied world-space line.
void Viewer2DLinePointSelectionSession::Begin(WorldPoint lineStart,
                                              WorldPoint lineEnd) {
  Reset();
  m_active = true;
  m_lineStart = lineStart;
  m_lineEnd = lineEnd;
}

// Updates the projected pointer preview.
void Viewer2DLinePointSelectionSession::UpdatePreview(WorldPoint point) {
  if (m_active)
    m_previewPoint = point;
}

// Accepts a projected point and returns a pair when selection completes.
std::optional<LinePointSelectionResult>
Viewer2DLinePointSelectionSession::AcceptPoint(WorldPoint point) {
  if (!m_active)
    return std::nullopt;
  if (!m_firstPoint) {
    m_firstPoint = point;
    m_previewPoint = point;
    return std::nullopt;
  }
  const LinePointSelectionResult result{*m_firstPoint, point};
  m_active = false;
  m_firstPoint.reset();
  m_previewPoint.reset();
  return result;
}

// Arms suppression of the matching mouse-up event.
void Viewer2DLinePointSelectionSession::MarkConsumeNextMouseUp() {
  m_consumeNextMouseUp = true;
}

// Consumes and clears the pending mouse-up suppression flag.
bool Viewer2DLinePointSelectionSession::ConsumeNextMouseUp() {
  if (!m_consumeNextMouseUp)
    return false;
  m_consumeNextMouseUp = false;
  return true;
}

// Cancels selection and clears all session-owned state.
void Viewer2DLinePointSelectionSession::Reset() {
  m_active = false;
  m_consumeNextMouseUp = false;
  m_lineStart = {};
  m_lineEnd = {};
  m_firstPoint.reset();
  m_previewPoint.reset();
}

} // namespace viewer2d::interaction
