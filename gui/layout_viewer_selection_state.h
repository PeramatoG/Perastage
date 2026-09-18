#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace gui::layoutselection {

enum class LayoutElementKind { None, View2D, Legend, EventTable, Text, Image };

struct LayoutElementRef {
  LayoutElementKind kind = LayoutElementKind::None;
  int id = -1;

  bool operator==(const LayoutElementRef &other) const = default;
};

struct ElementZIndex {
  int id = -1;
  int zIndex = 0;
};

struct ZOrderedElement {
  LayoutElementRef element;
  int zIndex = 0;
  std::size_t stableOrder = 0;
};

using ElementsByKind = std::array<std::vector<ElementZIndex>, 5>;
using ElementRefsByKind = std::array<std::vector<int>, 5>;

class LayoutViewerSelectionState {
public:
  // Returns the immutable identity of the current selection.
  LayoutElementRef Current() const;
  // Explicitly selects an element identity and reports whether it changed.
  bool Select(LayoutElementKind kind, int id);
  // Explicitly clears the current selection and reports whether it changed.
  bool Clear();
  // Reports whether the supplied identity is currently selected.
  bool IsSelected(LayoutElementKind kind, int id) const;
  // Reports whether the supplied reference is currently selected.
  bool Matches(LayoutElementRef element) const;

private:
  LayoutElementRef current_;
};

// Builds ascending stable Z order using View2D, Legend, EventTable, Text, Image
// category order.
std::vector<ZOrderedElement>
BuildStableZOrder(const ElementsByKind &elementsByKind);
// Computes the minimum and maximum Z index, returning zeroes for an empty list.
std::pair<int, int>
GetZIndexRange(const std::vector<ZOrderedElement> &elements);
// Computes the next Z index above all current elements.
int BringToFrontTarget(const std::vector<ZOrderedElement> &elements);
// Computes the next Z index below all current elements.
int SendToBackTarget(const std::vector<ZOrderedElement> &elements);
// Retains a valid selection or chooses the first element in category priority
// order.
LayoutElementRef RetainOrChooseDefault(LayoutElementRef current,
                                       const ElementRefsByKind &idsByKind);

} // namespace gui::layoutselection
