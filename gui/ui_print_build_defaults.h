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
#include "Viewer2DPrintSettings.h"

namespace ui::detail {
// Resolves print defaults from an explicit profile while keeping assertion
// configuration independent of the policy under test.
inline void ApplyPrintBuildDefaults(print::Viewer2DPrintSettings &settings,
    bool isDebugBuild, const print::PageSetup *physicalDocument = nullptr) {
  if (isDebugBuild) return;
  settings.pageSize = physicalDocument ? physicalDocument->pageSize : print::PageSize::A4;
  settings.includeGrid = true;
  settings.detailedFootprints = false;
}
} // namespace ui::detail
