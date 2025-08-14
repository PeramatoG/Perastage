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

namespace AutoPatcher {
// Automatically assign DMX addresses to fixtures in the scene.
// Fixtures are patched sequentially starting at the given universe and channel.
// The order is front-to-back (Y axis), prioritizing fixtures by type within each
// hang position so identical types are consecutive, and finally left-to-right (X axis).
void AutoPatch(MvrScene &scene, int startUniverse = 1, int startChannel = 1);
}
