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
  std::vector<std::string> informationalLogs;
};

// Prepares an immutable source scene for export without package or XML I/O.
Result Prepare(const MvrScene &sourceScene, const MvrExportOptions &options);

} // namespace mvr_export_preparation
