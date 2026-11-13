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

#include "ui_print_build_defaults.h"

namespace {

constexpr bool kIsDebugBuild =
#ifdef NDEBUG
    false;
#else
    true;
#endif

} // namespace

namespace ui {

// Returns whether a UI feature flag is enabled for the current build profile.
bool IsFeatureEnabled(FeatureFlag flag) {
  switch (flag) {
  case FeatureFlag::GenerateFixtureSymbols:
    return true;
  case FeatureFlag::LayoutViewerVboQuadBatch:
    return false;
  case FeatureFlag::PrintViewer2DDialog:
  case FeatureFlag::PrintViewer2DElementsDetail:
  case FeatureFlag::AssignSelectedFixtureCategory:
  case FeatureFlag::GdtfVerboseCatalogLogs:
    return kIsDebugBuild;
  }

  return false;
}

// Applies build-profile defaults to 2D print settings before showing dialogs/exporting.
void ApplyBuildDefaultsToViewer2DPrintSettings(
    print::Viewer2DPrintSettings &settings) {
  detail::ApplyPrintBuildDefaults(settings, kIsDebugBuild);
}

// Keeps Release defaults from silently fitting a fixed-scale A3 Layout to A4.
void ApplyBuildDefaultsToViewer2DPrintSettings(
    print::Viewer2DPrintSettings &settings, const print::PageSetup &physicalDocument) {
  detail::ApplyPrintBuildDefaults(settings, kIsDebugBuild, &physicalDocument);
}

} // namespace ui
