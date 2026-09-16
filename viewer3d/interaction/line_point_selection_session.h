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

  // Reports whether point selection is active.
  bool IsActive() const { return active_; }
  // Reports whether the next matching mouse-up must be consumed.
  bool HasMouseUpToConsume() const { return consumeMouseUp_; }
  // Returns the line start in world coordinates.
  const std::array<float, 3> &Start() const { return start_; }
  // Returns the line end in world coordinates.
  const std::array<float, 3> &End() const { return end_; }
  // Returns the first committed point when present.
  const std::optional<std::array<float, 3>> &First() const { return first_; }
  // Returns the current preview point when present.
  const std::optional<std::array<float, 3>> &Preview() const {
    return preview_;
  }

private:
  bool active_ = false;
  bool consumeMouseUp_ = false;
  std::array<float, 3> start_{};
  std::array<float, 3> end_{};
  std::optional<std::array<float, 3>> first_;
  std::optional<std::array<float, 3>> preview_;
};

} // namespace viewer3d::interaction
