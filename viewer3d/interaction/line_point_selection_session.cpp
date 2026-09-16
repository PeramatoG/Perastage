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

// Marks the next matching mouse-up as consumed by point selection.
void LinePointSelectionSession::MarkMouseUpToConsume() {
  consumeMouseUp = true;
}

// Consumes and clears a pending point-selection mouse-up.
bool LinePointSelectionSession::ConsumeMouseUp() {
  if (!consumeMouseUp)
    return false;
  consumeMouseUp = false;
  return true;
}

// Updates the transient point shown before the next click.
void LinePointSelectionSession::SetPreview(
    const std::optional<std::array<float, 3>> &point) {
  preview = point;
}

// Completes point selection while retaining mouse-up suppression for its click.
void LinePointSelectionSession::Complete() {
  active = false;
  first.reset();
  preview.reset();
}

// Cancels point selection while retaining mouse-up suppression for its click.
void LinePointSelectionSession::Cancel() { Complete(); }

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
