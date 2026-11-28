#include "layout_viewer_selection_state.h"

#include <algorithm>

namespace gui::layoutselection {
std::vector<LayoutElementRef> IntersectingFrames(
    const std::vector<SelectionFrame> &frames, double x, double y, double width, double height) {
  if (width < 0) { x += width; width = -width; }
  if (height < 0) { y += height; height = -height; }
  std::vector<LayoutElementRef> result;
  for (const auto &f : frames)
    if (f.x <= x + width && f.x + f.width >= x &&
        f.y <= y + height && f.y + f.height >= y)
      result.push_back(f.element);
  return result;
}
LayoutElementRef HitFrame(const std::vector<SelectionFrame> &frames, double x, double y) {
  for (auto it = frames.rbegin(); it != frames.rend(); ++it)
    if (x >= it->x && x < it->x + it->width && y >= it->y && y < it->y + it->height)
      return it->element;
  return {};
}
namespace {
constexpr std::array<LayoutElementKind, 5> kKinds = {
    LayoutElementKind::View2D, LayoutElementKind::Legend,
    LayoutElementKind::EventTable, LayoutElementKind::Text,
    LayoutElementKind::Image};
}

const std::vector<LayoutElementRef> &LayoutViewerSelectionState::Ordered() const {
  return ordered_;
}
bool LayoutViewerSelectionState::Publish(bool changed) {
  if (changed && onChange_) onChange_();
  return changed;
}
LayoutElementRef LayoutViewerSelectionState::Current() const {
  return ordered_.empty() ? LayoutElementRef{} : ordered_.back();
}
bool LayoutViewerSelectionState::Select(LayoutElementKind kind, int id) {
  if (kind == LayoutElementKind::None || id < 0) return Clear();
  const LayoutElementRef next{kind,id};
  if (ordered_.size() == 1 && ordered_.front() == next) return false;
  ordered_ = {next};
  return Publish(true);
}
bool LayoutViewerSelectionState::Replace(const std::vector<LayoutElementRef> &elements) {
  std::vector<LayoutElementRef> next;
  for (auto element : elements)
    if (element.kind != LayoutElementKind::None && element.id >= 0 &&
        std::find(next.begin(),next.end(),element)==next.end()) next.push_back(element);
  if (ordered_==next) return false;
  ordered_=std::move(next);
  return Publish(true);
}
bool LayoutViewerSelectionState::Add(LayoutElementRef element) {
  if (element.kind == LayoutElementKind::None || element.id < 0 || Matches(element)) return false;
  ordered_.push_back(element);
  return Publish(true);
}
bool LayoutViewerSelectionState::Remove(LayoutElementRef element) {
  return Publish(std::erase(ordered_, element) != 0);
}
bool LayoutViewerSelectionState::Toggle(LayoutElementRef element) {
  return Matches(element) ? Remove(element) : Add(element);
}
bool LayoutViewerSelectionState::Clear() {
  const bool changed = !ordered_.empty();
  ordered_.clear();
  return Publish(changed);
}
bool LayoutViewerSelectionState::IsSelected(LayoutElementKind kind, int id) const {
  return Matches({kind,id});
}
bool LayoutViewerSelectionState::Matches(LayoutElementRef element) const {
  return std::find(ordered_.begin(), ordered_.end(), element) != ordered_.end();
}
bool LayoutViewerSelectionState::Retain(const ElementRefsByKind &idsByKind) {
  const auto count = ordered_.size();
  std::erase_if(ordered_, [&](auto ref) {
    for (size_t i = 0; i < kKinds.size(); ++i)
      if (ref.kind == kKinds[i])
        return std::find(idsByKind[i].begin(), idsByKind[i].end(), ref.id) == idsByKind[i].end();
    return true;
  });
  return Publish(count != ordered_.size());
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
