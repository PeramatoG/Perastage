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

// Selects how truss geometry references are serialized during MVR export.
enum class MvrTrussGeometryExportMode {
  Standard = 0,
  DirectGeometry3DForTrussSymbols = 1,
};

// Carries caller-selected MVR export behavior without depending on GUI classes.
struct MvrExportOptions {
  MvrTrussGeometryExportMode trussGeometryExportMode = MvrTrussGeometryExportMode::Standard;
};

// Returns the single canonical policy used by project and interchange serialization.
inline MvrExportOptions CanonicalMvrExportOptions() { return {}; }
