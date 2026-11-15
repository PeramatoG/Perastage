/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "LayoutCollection.h"
#include "LayoutElementRef.h"
#include "alignment_geometry.h"
#include <algorithm>
#include <type_traits>

namespace layouts {
struct ElementFrame {
  LayoutElementRef element;
  ViewportFramePoints frame;
};
// Resolve typed payloads without exposing the authoritative collection to GUI.
template <class Page, class Visitor>
bool VisitElement(Page &page, LayoutElementRef ref, Visitor visitor) {
  const auto visit = [&](auto &items) {
    auto it = std::find_if(items.begin(), items.end(),
        [&](auto &item) { return item.id == ref.id; });
    if (it == items.end()) return false;
    visitor(*it);
    return true;
  };
  switch (ref.kind) {
  case LayoutElementKind::View2D: return visit(page.view2dViews);
  case LayoutElementKind::Legend: return visit(page.legendViews);
  case LayoutElementKind::EventTable: return visit(page.eventTables);
  case LayoutElementKind::Text: return visit(page.textViews);
  case LayoutElementKind::Image: return visit(page.imageViews);
  default: return false;
  }
}
inline ViewportFramePoints ElementFramePoints(const auto &item) {
  if constexpr (std::is_same_v<std::decay_t<decltype(item)>, Layout2DViewDefinition>)
    return ResolveViewportFrame(item);
  else return item.frame;
}
inline ViewportFramePoints ResolveElementFrame(const LayoutDefinition &page,
                                              LayoutElementRef ref) {
  ViewportFramePoints frame;
  if (!VisitElement(page, ref, [&](const auto &item) { frame = ElementFramePoints(item); }))
    throw std::invalid_argument("Layout element does not exist on this page.");
  ValidateViewportFrame(frame);
  return frame;
}
inline std::vector<ElementFrame> SnapshotFrames(const LayoutDefinition &page,
    const std::vector<LayoutElementRef> &refs) {
  if (refs.empty()) throw std::invalid_argument("Select Layout elements first.");
  std::vector<ElementFrame> result;
  for (auto ref : refs) {
    if (std::any_of(result.begin(), result.end(), [&](auto &f) { return f.element == ref; }))
      throw std::invalid_argument("Duplicate Layout element identity.");
    result.push_back({ref, ResolveElementFrame(page, ref)});
  }
  return result;
}
// Integer payloads round absolute target origins to nearest point, ties toward
// positive infinity (<= 0.5 pt error per axis). Viewports keep fractional precision.
inline void SetElementPosition(LayoutDefinition &page, LayoutElementRef ref,
                               double x, double y) {
  auto frame = ResolveElementFrame(page, ref);
  frame.x = x; frame.y = y;
  ValidateViewportFrame(frame);
  VisitElement(page, ref, [&](auto &item) {
    if constexpr (std::is_same_v<std::decay_t<decltype(item)>, Layout2DViewDefinition>) {
      SetViewportFrame(item, frame);
    } else {
      frame.x = std::floor(frame.x + 0.5); frame.y = std::floor(frame.y + 0.5);
      ValidateViewportFrame(frame);
      item.frame.x = static_cast<int>(frame.x);
      item.frame.y = static_cast<int>(frame.y);
    }
  });
}
// Preview and commit share exactly the same quantization and validation.
inline LayoutDefinition TranslateElements(const LayoutDefinition &original,
    const std::vector<LayoutElementRef> &refs, double dx, double dy) {
  const auto frames = SnapshotFrames(original, refs);
  if (!std::isfinite(dx) || !std::isfinite(dy))
    throw std::invalid_argument("Layout translation must be finite.");
  // A common integer delta preserves mixed-selection relative positions.
  const bool hasInteger = std::any_of(refs.begin(), refs.end(), [](auto ref) {
    return ref.kind != LayoutElementKind::View2D;
  });
  if (hasInteger) { dx = std::floor(dx + 0.5); dy = std::floor(dy + 0.5); }
  auto candidate = original;
  for (const auto &f : frames)
    SetElementPosition(candidate, f.element, f.frame.x + dx, f.frame.y + dy);
  return candidate;
}
// The only document/projected-coordinate adapter: paper Y grows downward.
inline alignment::Rectangle AlignmentRectangle(ViewportFramePoints f, bool origins) {
  if (origins) return {f.x, -f.y, f.x, -f.y};
  return {f.x, -(f.y + f.height), f.x + f.width, -f.y};
}
LayoutDefinition AlignElements(const LayoutDefinition &original,
    const std::vector<LayoutElementRef> &refs, alignment::Reference reference,
    alignment::Operation operation, bool origins = false);
inline LayoutDefinition DeleteElements(const LayoutDefinition &original,
    const std::vector<LayoutElementRef> &refs) {
  SnapshotFrames(original, refs); // Validate the entire batch before deletion.
  auto candidate = original;
  const auto erase = [&](auto &items, LayoutElementKind kind) {
    std::erase_if(items, [&](const auto &item) {
      return std::find(refs.begin(), refs.end(), LayoutElementRef{kind,item.id}) != refs.end();
    });
  };
  erase(candidate.view2dViews, LayoutElementKind::View2D);
  erase(candidate.legendViews, LayoutElementKind::Legend);
  erase(candidate.eventTables, LayoutElementKind::EventTable);
  erase(candidate.textViews, LayoutElementKind::Text);
  erase(candidate.imageViews, LayoutElementKind::Image);
  return candidate;
}
} // namespace layouts
