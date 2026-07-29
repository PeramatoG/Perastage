#pragma once

#include "mvrscene.h"

#include <string>
#include <vector>

namespace scene_transform_integrity {

enum class Severity { Repair, Fatal };

struct Diagnostic {
  Severity severity = Severity::Fatal;
  MvrNodeType type = MvrNodeType::SceneObject;
  std::string uuid;
  std::string reason;
  std::string action;
};

struct Result {
  bool success = true;
  bool repaired = false;
  std::vector<Diagnostic> diagnostics;
};

// Validates hierarchy transforms and repairs canonical local matrices safely.
Result ValidateAndRepair(MvrScene &scene, float tolerance = 0.001f);

// Formats one transform-integrity diagnostic for logs and user warnings.
std::string FormatDiagnostic(const Diagnostic &diagnostic);

} // namespace scene_transform_integrity
