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

#include "configmanager.h"
#include "mvr_export_options.h"

#include <optional>
#include <string>

namespace mvr::preferences {

constexpr const char *kTrussGeometryExportModeKey = "mvr_truss_geometry_export_mode";
constexpr const char *kTrussGeometryExportModeStandardValue = "standard";
constexpr const char *kTrussGeometryExportModeDirectGeometry3DValue = "direct_geometry3d_for_truss_symbols";

// Converts a persisted config value into a truss geometry export mode.
inline MvrTrussGeometryExportMode ParseTrussGeometryExportMode(const std::optional<std::string> &value) {
  if (value && *value == kTrussGeometryExportModeDirectGeometry3DValue)
    return MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols;
  return MvrTrussGeometryExportMode::Standard;
}

// Converts a truss geometry export mode into a stable persisted config value.
inline const char *SerializeTrussGeometryExportMode(MvrTrussGeometryExportMode mode) {
  switch (mode) {
  case MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols:
    return kTrussGeometryExportModeDirectGeometry3DValue;
  case MvrTrussGeometryExportMode::Standard:
  default:
    return kTrussGeometryExportModeStandardValue;
  }
}

// Loads MVR export options from the existing user preference store.
inline MvrExportOptions LoadExportOptions(const ConfigManager &config) {
  MvrExportOptions options;
  options.trussGeometryExportMode = ParseTrussGeometryExportMode(config.GetValue(kTrussGeometryExportModeKey));
  return options;
}

// Saves MVR export options to the existing user preference store.
inline void SaveExportOptions(ConfigManager &config, const MvrExportOptions &options) {
  config.SetValue(kTrussGeometryExportModeKey, SerializeTrussGeometryExportMode(options.trussGeometryExportMode));
}

} // namespace mvr::preferences
