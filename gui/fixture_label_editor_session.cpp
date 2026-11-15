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
#include "fixture_label_editor.h"
viewer2d::FixtureLabelOverrideMap FixtureLabelEditor::EffectiveDraft() const {
  if (!session_) return draft_;
  auto effective = session_->inherited;
  const auto overlay = [](auto &to, const auto &from) {
    for (int v=0; v<3; ++v) for (int f=0; f<3; ++f) if (from[v][f]) to[v][f] = from[v][f];
  };
  if (const auto it = draft_.find(viewer2d::kLabelDefaultsKey); it != draft_.end()) {
    for (auto &[id, entry] : effective) overlay(entry.fieldStyles, it->second.fieldStyles);
    overlay(effective[viewer2d::kLabelDefaultsKey].fieldStyles, it->second.fieldStyles);
  }
  for (const auto &[id, entry] : draft_) if (id != viewer2d::kLabelDefaultsKey)
    overlay(effective[id].fieldStyles, entry.fieldStyles);
  return effective;
}
