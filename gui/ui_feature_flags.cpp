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
#include "ui_feature_flags.h"

#include "Viewer2DPrintSettings.h"

namespace {

constexpr bool kIsDebugBuild =
#ifdef NDEBUG
    false;
#else
    true;
#endif

} // namespace

namespace ui {

bool IsFeatureEnabled(FeatureFlag flag) {
  switch (flag) {
  case FeatureFlag::PrintViewer2DDialog:
  case FeatureFlag::PrintViewer2DElementsDetail:
  case FeatureFlag::GenerateFixtureSymbols:
  case FeatureFlag::AssignSelectedFixtureCategory:
    return kIsDebugBuild;
  }

  return false;
}

void ApplyBuildDefaultsToViewer2DPrintSettings(
    print::Viewer2DPrintSettings &settings) {
  if (kIsDebugBuild)
    return;

  settings.pageSize = print::PageSize::A4;
  settings.includeGrid = true;
  settings.detailedFootprints = false;
}

} // namespace ui
