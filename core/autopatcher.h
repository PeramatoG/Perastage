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

#include "mvrscene.h"
#include <string>
#include <vector>

namespace AutoPatcher {
// Automatically assign DMX addresses by Position, physical component, and type.
// Transverse Positions run front-to-back before longitudinal Positions, while
// disconnected parallel components use a deterministic serpentine traversal.
// Positions and components that fit one universe are kept intact when possible.
void AutoPatch(MvrScene &scene, int startUniverse = 1, int startChannel = 1);

// Re-patch only the selected fixtures in the exact order provided by
// selectionOrder. Existing patch values for the selected fixtures are cleared
// before re-patching. The new patch starts at the next free channel after the
// highest already-patched fixture that remains in the scene.
void AutoPatchSelection(MvrScene &scene,
                        const std::vector<std::string> &selectionOrder);
} // namespace AutoPatcher
