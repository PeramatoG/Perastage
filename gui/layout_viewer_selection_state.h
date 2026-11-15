#pragma once

#include "layouts/LayoutElementRef.h"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace gui::layoutselection {

using layouts::LayoutElementKind;
using layouts::LayoutElementRef;

struct SelectionFrame {
  LayoutElementRef element;
  double x, y, width, height;
};
// Input follows ascending stable Z order; hits choose the last (topmost).
std::vector<LayoutElementRef> IntersectingFrames(
    const std::vector<SelectionFrame> &frames, double x, double y, double width, double height);
LayoutElementRef HitFrame(const std::vector<SelectionFrame> &frames, double x, double y);

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
  const std::vector<LayoutElementRef> &Ordered() const;
  bool Replace(const std::vector<LayoutElementRef> &elements);
  bool Add(LayoutElementRef element);
  bool Toggle(LayoutElementRef element);
  bool Remove(LayoutElementRef element);
  bool Retain(const ElementRefsByKind &idsByKind);
  // Returns the newest remaining selected identity (the stable primary).
  LayoutElementRef Current() const;
  // Replaces membership with one identity and reports whether it changed.
  bool Select(LayoutElementKind kind, int id);
  // Explicitly clears the current selection and reports whether it changed.
  bool Clear();
  // Reports whether the supplied identity is currently selected.
  bool IsSelected(LayoutElementKind kind, int id) const;
  // Reports whether the supplied reference is currently selected.
  bool Matches(LayoutElementRef element) const;

private:
  // Primary is the newest remaining selection; additive no-ops keep it stable.
  std::vector<LayoutElementRef> ordered_;

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
