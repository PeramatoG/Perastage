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
#include "LayoutDocumentEdit.h"

namespace layouts {
LayoutDefinition AlignElements(const LayoutDefinition &original,
    const std::vector<LayoutElementRef> &refs, alignment::Reference reference,
    alignment::Operation operation, bool origins) {
  const auto frames = SnapshotFrames(original, refs);
  std::vector<alignment::Rectangle> rectangles;
  for (const auto &f : frames)
    rectangles.push_back(AlignmentRectangle(f.frame, origins));
  const auto plan = alignment::Calculate(rectangles, reference, operation, 1e-9);
  if (!plan.valid) throw std::invalid_argument(plan.diagnostic);
  const bool horizontal = operation == alignment::Operation::Left ||
      operation == alignment::Operation::Right ||
      operation == alignment::Operation::CenterHorizontal;
  // Integer rounding can move the combined extents by up to half a point.
  // Treat an integer-containing selection inside that resolution as settled,
  // otherwise repeated extents alignment can translate precise viewports again.
  if (reference == alignment::Reference::Extents &&
      std::any_of(refs.begin(), refs.end(), [](auto ref) {
        return ref.kind != LayoutElementKind::View2D;
      }) && std::all_of(plan.offsets.begin(), plan.offsets.end(), [](double offset) {
        return std::abs(offset) <= 0.5 + 1e-9;
      })) return original;
  auto candidate = original;
  for (size_t i = 0; i < frames.size(); ++i) {
    if (std::abs(plan.offsets[i]) <= 1e-9) continue;
    const auto &f = frames[i];
    SetElementPosition(candidate, f.element,
        f.frame.x + (horizontal ? plan.offsets[i] : 0),
        f.frame.y - (horizontal ? 0 : plan.offsets[i]));
  }
  return candidate;
}

} // namespace layouts
