#include "line_point_selection_session.h"

namespace viewer3d::interaction {

// Starts point selection on the supplied world-space line.
void LinePointSelectionSession::Begin(const std::array<float, 3> &newStart,
                                      const std::array<float, 3> &newEnd) {
  Reset();
  active_ = true;
  start_ = newStart;
  end_ = newEnd;
}

// Stores the first selected point and initializes its preview.
void LinePointSelectionSession::CommitFirst(const std::array<float, 3> &point) {
  first_ = point;
  preview_ = point;
}

// Marks the next matching mouse-up as consumed by point selection.
void LinePointSelectionSession::MarkMouseUpToConsume() {
  consumeMouseUp_ = true;
}

// Consumes and clears a pending point-selection mouse-up.
bool LinePointSelectionSession::ConsumeMouseUp() {
  if (!consumeMouseUp_)
    return false;
  consumeMouseUp_ = false;
  return true;
}

// Updates the transient point shown before the next click.
void LinePointSelectionSession::SetPreview(
    const std::optional<std::array<float, 3>> &point) {
  preview_ = point;
}

// Completes point selection while retaining mouse-up suppression for its click.
void LinePointSelectionSession::Complete() {
  active_ = false;
  first_.reset();
  preview_.reset();
}

// Cancels point selection while retaining mouse-up suppression for its click.
void LinePointSelectionSession::Cancel() { Complete(); }

// Clears selection and mouse-up suppression state.
void LinePointSelectionSession::Reset() {
  active_ = false;
  consumeMouseUp_ = false;
  start_ = {};
  end_ = {};
  first_.reset();
  preview_.reset();
}

} // namespace viewer3d::interaction
