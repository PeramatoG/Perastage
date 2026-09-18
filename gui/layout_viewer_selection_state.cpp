#include "layout_viewer_selection_state.h"

#include <algorithm>

namespace gui::layoutselection {
namespace {
constexpr std::array<LayoutElementKind, 5> kKinds = {
    LayoutElementKind::View2D, LayoutElementKind::Legend,
    LayoutElementKind::EventTable, LayoutElementKind::Text,
    LayoutElementKind::Image};
}

// Returns the immutable identity of the current selection.
LayoutElementRef LayoutViewerSelectionState::Current() const {
  return current_;
}

// Explicitly selects a valid element identity and reports whether it changed.
bool LayoutViewerSelectionState::Select(LayoutElementKind kind, int id) {
  if (kind == LayoutElementKind::None || id < 0)
    return Clear();
  const LayoutElementRef next{kind, id};
  const bool changed = !(current_ == next);
  current_ = next;
  return changed;
}

// Explicitly clears the current selection and reports whether it changed.
bool LayoutViewerSelectionState::Clear() {
  const bool changed = !(current_ == LayoutElementRef{});
  current_ = {};
  return changed;
}

// Reports whether the supplied identity is currently selected.
bool LayoutViewerSelectionState::IsSelected(LayoutElementKind kind,
                                            int id) const {
  return Matches({kind, id});
}

// Reports whether the supplied reference is currently selected.
bool LayoutViewerSelectionState::Matches(LayoutElementRef element) const {
  return current_ == element;
}

// Builds ascending stable Z order using the established category order.
std::vector<ZOrderedElement>
BuildStableZOrder(const ElementsByKind &elementsByKind) {
  std::vector<ZOrderedElement> result;
  std::size_t count = 0;
  for (const auto &elements : elementsByKind)
    count += elements.size();
  result.reserve(count);
  std::size_t stableOrder = 0;
  for (std::size_t kindIndex = 0; kindIndex < kKinds.size(); ++kindIndex) {
    for (const auto &element : elementsByKind[kindIndex]) {
      result.push_back(
          {{kKinds[kindIndex], element.id}, element.zIndex, stableOrder++});
    }
  }
  std::stable_sort(
      result.begin(), result.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.zIndex < rhs.zIndex; });
  return result;
}

// Computes the minimum and maximum Z index, returning zeroes for an empty list.
std::pair<int, int>
GetZIndexRange(const std::vector<ZOrderedElement> &elements) {
  if (elements.empty())
    return {0, 0};
  int minimum = elements.front().zIndex;
  int maximum = minimum;
  for (const auto &element : elements) {
    minimum = std::min(minimum, element.zIndex);
    maximum = std::max(maximum, element.zIndex);
  }
  return {minimum, maximum};
}

// Computes the next Z index above all current elements.
int BringToFrontTarget(const std::vector<ZOrderedElement> &elements) {
  return GetZIndexRange(elements).second + 1;
}

// Computes the next Z index below all current elements.
int SendToBackTarget(const std::vector<ZOrderedElement> &elements) {
  return GetZIndexRange(elements).first - 1;
}

// Retains a valid selection or chooses the first element in priority order.
LayoutElementRef RetainOrChooseDefault(LayoutElementRef current,
                                       const ElementRefsByKind &idsByKind) {
  for (std::size_t kindIndex = 0; kindIndex < kKinds.size(); ++kindIndex) {
    const auto &ids = idsByKind[kindIndex];
    if (current.kind == kKinds[kindIndex] &&
        std::find(ids.begin(), ids.end(), current.id) != ids.end()) {
      return current;
    }
  }
  for (std::size_t kindIndex = 0; kindIndex < kKinds.size(); ++kindIndex) {
    if (!idsByKind[kindIndex].empty())
      return {kKinds[kindIndex], idsByKind[kindIndex].front()};
  }
  return {};
}

} // namespace gui::layoutselection
