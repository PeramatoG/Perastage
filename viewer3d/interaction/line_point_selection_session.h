#pragma once

#include <array>
#include <optional>

namespace viewer3d::interaction {

// Stores neutral state while two points are selected along a line.
class LinePointSelectionSession {
public:
  // Starts point selection on the supplied world-space line.
  void Begin(const std::array<float, 3> &start,
             const std::array<float, 3> &end);
  // Stores the first selected point and initializes its preview.
  void CommitFirst(const std::array<float, 3> &point);
  // Marks the next matching mouse-up as consumed by point selection.
  void MarkMouseUpToConsume();
  // Consumes and clears a pending point-selection mouse-up.
  bool ConsumeMouseUp();
  // Updates the transient point shown before the next click.
  void SetPreview(const std::optional<std::array<float, 3>> &point);
  // Completes point selection and clears point-specific transient state.
  void Complete();
  // Cancels point selection while retaining pending mouse-up suppression.
  void Cancel();
  // Clears selection and mouse-up suppression state.
  void Reset();

  bool active = false;
  bool consumeMouseUp = false;
  std::array<float, 3> start{};
  std::array<float, 3> end{};
  std::optional<std::array<float, 3>> first;
  std::optional<std::array<float, 3>> preview;
};

} // namespace viewer3d::interaction
