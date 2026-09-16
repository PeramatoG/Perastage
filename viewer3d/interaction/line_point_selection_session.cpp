#include "line_point_selection_session.h"

namespace viewer3d::interaction {

// Starts point selection on the supplied world-space line.
void LinePointSelectionSession::Begin(const std::array<float, 3> &newStart,
                                      const std::array<float, 3> &newEnd) {
  Reset();
  active = true;
  start = newStart;
  end = newEnd;
}

// Stores the first selected point and initializes its preview.
void LinePointSelectionSession::CommitFirst(const std::array<float, 3> &point) {
  first = point;
  preview = point;
}

// Updates the transient point shown before the next click.
void LinePointSelectionSession::SetPreview(
    const std::optional<std::array<float, 3>> &point) {
  preview = point;
}

// Clears selection and mouse-up suppression state.
void LinePointSelectionSession::Reset() {
  active = false;
  consumeMouseUp = false;
  start = {};
  end = {};
  first.reset();
  preview.reset();
}

} // namespace viewer3d::interaction
