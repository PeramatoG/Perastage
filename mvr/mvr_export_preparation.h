/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
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

#include "mvr_export_diagnostic.h"
#include "mvr_export_options.h"

#include "mvrscene.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mvr_export_preparation {

// Contains the normalized private scene and values derived before
// serialization.
struct Result {
  bool success = false;
  MvrScene scene;
  std::unordered_map<std::string, std::pair<std::string, int>> objectIds;
  std::unordered_map<std::string, int> fixtureUnitNumbers;
  std::unordered_map<std::string, std::string> layerUuids;
  std::unordered_map<std::string, std::string> positions;
  std::unordered_map<std::string, std::string> positionReferences;
  std::vector<MvrExportDiagnostic> diagnostics;
  std::vector<MvrExportDiagnostic> objectIdDiagnostics;
  std::unordered_map<std::string, MvrExportDiagnostic>
      positionReferenceDiagnostics;
  std::vector<std::string> informationalLogs;
};

// Prepares an immutable source scene for export without package or XML I/O.
Result Prepare(const MvrScene &sourceScene, const MvrExportOptions &options);

} // namespace mvr_export_preparation
