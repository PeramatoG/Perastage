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
#include "layouts/LayoutDocumentEdit.h"
#include <optional>

namespace gui::layoutinteraction {
// Owns an immutable document snapshot. Previews never publish history.
class LayoutViewerGroupGesture {
public:
  void Begin(const layouts::LayoutDefinition &page,
             const std::vector<layouts::LayoutElementRef> &refs,
             double x, double y, double zoom) {
    layouts::SnapshotFrames(page, refs);
    if (!std::isfinite(zoom) || zoom <= 0) throw std::invalid_argument("Invalid page zoom.");
    original_ = page; refs_ = refs;
    startX_ = x; startY_ = y; zoom_ = zoom; dx_ = dy_ = 0;
  }
  layouts::LayoutDefinition Preview(double x, double y) {
    const double dx = (x - startX_) / zoom_, dy = (y - startY_) / zoom_;
    auto page = layouts::TranslateElements(*original_, refs_, dx, dy);
    dx_ = dx; dy_ = dy;
    return page;
  }
  bool Active() const { return original_.has_value(); }
  const layouts::LayoutDefinition &Original() const { return *original_; }
  const std::vector<layouts::LayoutElementRef> &Refs() const { return refs_; }
  double DeltaX() const { return dx_; }
  double DeltaY() const { return dy_; }
  void Cancel() { original_.reset(); refs_.clear(); dx_ = dy_ = 0; }
private:
  std::optional<layouts::LayoutDefinition> original_;
  std::vector<layouts::LayoutElementRef> refs_;
  double startX_ = 0, startY_ = 0, zoom_ = 1, dx_ = 0, dy_ = 0;
};
} // namespace gui::layoutinteraction
