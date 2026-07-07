#pragma once

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gdtf {

enum class FixtureInsertionDiagnosticSeverity {
  Info,
  Warning,
  Error
};

struct FixtureInsertionDiagnostic {
  FixtureInsertionDiagnosticSeverity severity =
      FixtureInsertionDiagnosticSeverity::Info;
  std::string stage;
  std::string code;
  std::string message;
  std::string detail;
};

struct GdtfFixtureInsertionPreparation {
  bool success = false;
  std::filesystem::path sourcePath;
  std::string fixtureDisplayName;
  std::vector<std::string> dmxModeNames;
  std::optional<float> weightKg;
  std::optional<float> powerConsumptionW;
  std::string modelColorHex;
  std::vector<FixtureInsertionDiagnostic> diagnostics;
  bool tolerantNonStandardInputAccepted = false;
  bool standardsCompliantForCheckedRead = false;
};

GdtfFixtureInsertionPreparation PrepareGdtfFixtureInsertion(
    const std::filesystem::path &sourcePath);
const FixtureInsertionDiagnostic *FirstError(
    const GdtfFixtureInsertionPreparation &preparation);
std::string SummarizeFixtureInsertionDiagnostics(
    const GdtfFixtureInsertionPreparation &preparation);

} // namespace gdtf
