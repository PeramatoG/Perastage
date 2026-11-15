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
#include "fixture_label_composition.h"
#include "../core/layouts/FixtureLabelPresentation.h"
namespace layouts { struct Layout2DViewRenderOptions; }
namespace viewer2d {
// Published as shared_ptr<const ...>; no authoritative project state is replaced.
struct FixtureLabelPresentationContext {
  FixtureLabelOverrideMap styles;
};
std::shared_ptr<const FixtureLabelPresentationContext> BuildLabelPresentationContext(
    const ConfigManager &, const layouts::FixtureLabelPresentation &,
    const layouts::FixtureLabelProfiles &);
FixtureLabelOverrideMap SnapshotLayoutLabelEditorInheritance(
    const ConfigManager &, const layouts::Layout2DViewRenderOptions &);
FixtureLabelOverrideMap OverlayLabelPresentation(const FixtureLabelOverrideMap &,
    const layouts::FixtureLabelPresentation &, const layouts::FixtureLabelProfiles &);
} // namespace viewer2d
