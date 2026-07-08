#include "gdtf_fixture_insertion_preparation.h"

#include <exception>
#include <sstream>
#include <system_error>

namespace gdtf {
namespace {

// Adds a structured insertion diagnostic to the preparation result.
void AddDiagnostic(GdtfFixtureInsertionPreparation &result,
                   FixtureInsertionDiagnosticSeverity severity,
                   std::string stage, std::string code, std::string message,
                   std::string detail = {}) {
  result.diagnostics.push_back({severity, std::move(stage), std::move(code),
                                std::move(message), std::move(detail)});
}

// Converts an archive diagnostic code to a stable insertion diagnostic code.
std::string ToCode(ArchiveDiagnosticCode code) {
  switch (code) {
  case ArchiveDiagnosticCode::None:
    return "archive-none";
  case ArchiveDiagnosticCode::EmptySourcePath:
    return "archive-empty-source-path";
  case ArchiveDiagnosticCode::OpenFailed:
    return "archive-open-failed";
  case ArchiveDiagnosticCode::NoReadableEntries:
    return "archive-no-readable-entries";
  case ArchiveDiagnosticCode::MissingDescriptionXml:
    return "archive-missing-description";
  case ArchiveDiagnosticCode::DuplicateDescriptionXml:
    return "archive-duplicate-description";
  case ArchiveDiagnosticCode::AmbiguousDescriptionXml:
    return "archive-ambiguous-description";
  case ArchiveDiagnosticCode::NonCanonicalDescriptionXml:
    return "archive-non-canonical-description";
  case ArchiveDiagnosticCode::EmptyDescriptionXml:
    return "archive-empty-description";
  case ArchiveDiagnosticCode::UnsafeEntryPath:
    return "archive-unsafe-entry-path";
  case ArchiveDiagnosticCode::EntryReadFailed:
    return "archive-entry-read-failed";
  case ArchiveDiagnosticCode::EntryTooLarge:
    return "archive-entry-too-large";
  case ArchiveDiagnosticCode::FilesystemError:
    return "archive-filesystem-error";
  case ArchiveDiagnosticCode::UnexpectedException:
    return "archive-unexpected-exception";
  }
  return "archive-unknown";
}

// Converts a description diagnostic code to a stable insertion diagnostic code.
std::string ToCode(DescriptionDiagnosticCode code) {
  switch (code) {
  case DescriptionDiagnosticCode::None:
    return "description-none";
  case DescriptionDiagnosticCode::MalformedXml:
    return "description-malformed-xml";
  case DescriptionDiagnosticCode::MissingRoot:
    return "description-missing-root";
  case DescriptionDiagnosticCode::MissingFixtureType:
    return "description-missing-fixture-type";
  case DescriptionDiagnosticCode::MissingDmxModes:
    return "description-missing-dmx-modes";
  case DescriptionDiagnosticCode::MissingUsableDmxMode:
    return "description-missing-usable-dmx-mode";
  case DescriptionDiagnosticCode::UnknownElement:
    return "description-unknown-element";
  case DescriptionDiagnosticCode::MissingLocalResource:
    return "description-missing-local-resource";
  case DescriptionDiagnosticCode::MissingWheelMediaResource:
    return "description-missing-wheel-media";
  case DescriptionDiagnosticCode::AmbiguousWheelMediaResource:
    return "description-ambiguous-wheel-media";
  case DescriptionDiagnosticCode::NonCanonicalWheelMediaCaseMatch:
    return "description-non-canonical-wheel-media-case";
  }
  return "description-unknown";
}

// Reports whether a description diagnostic prevents fixture insertion.
bool IsFatalDescriptionDiagnostic(DescriptionDiagnosticCode code) {
  switch (code) {
  case DescriptionDiagnosticCode::MalformedXml:
  case DescriptionDiagnosticCode::MissingRoot:
  case DescriptionDiagnosticCode::MissingFixtureType:
  case DescriptionDiagnosticCode::MissingDmxModes:
  case DescriptionDiagnosticCode::MissingUsableDmxMode:
    return true;
  case DescriptionDiagnosticCode::None:
  case DescriptionDiagnosticCode::UnknownElement:
  case DescriptionDiagnosticCode::MissingLocalResource:
  case DescriptionDiagnosticCode::MissingWheelMediaResource:
  case DescriptionDiagnosticCode::AmbiguousWheelMediaResource:
  case DescriptionDiagnosticCode::NonCanonicalWheelMediaCaseMatch:
    return false;
  }
  return true;
}

// Returns the preferred display name from a parsed FixtureType snapshot.
std::string DisplayNameFromDescription(const GdtfDescriptionSnapshot &description,
                                       const std::filesystem::path &sourcePath) {
  if (!description.fixtureTypeName.empty())
    return description.fixtureTypeName;
  if (!description.shortName.empty())
    return description.shortName;
  if (!description.longName.empty())
    return description.longName;
  return sourcePath.stem().generic_string();
}

} // namespace

// Prepares a GDTF fixture for Add Fixture without mutating project or archive state.
GdtfFixtureInsertionPreparation PrepareGdtfFixtureInsertion(
    const std::filesystem::path &sourcePath) {
  GdtfFixtureInsertionPreparation result;
  result.sourcePath = sourcePath;
  try {
    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Info,
                  "validating", "preparation-started",
                  "Preparing GDTF fixture for insertion.",
                  sourcePath.filename().generic_string());
    if (sourcePath.empty()) {
      AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                    "validating", "empty-source-path",
                    "No GDTF file was selected.");
      return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(sourcePath, ec) || ec) {
      AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                    "validating", "source-missing",
                    "The selected GDTF file does not exist.", ec.message());
      return result;
    }
    if (!std::filesystem::is_regular_file(sourcePath, ec) || ec) {
      AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                    "validating", "source-not-regular-file",
                    "The selected GDTF path is not a regular file.",
                    ec.message());
      return result;
    }
    const auto fileSize = std::filesystem::file_size(sourcePath, ec);
    if (ec || fileSize == 0) {
      AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                    "validating", "source-empty-or-unreadable",
                    "The selected GDTF file is empty or cannot be read.",
                    ec.message());
      return result;
    }

    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Info,
                  "archive-read", "archive-read-started",
                  "Reading GDTF archive.",
                  sourcePath.filename().generic_string());
    const ArchiveReadResult archive = ReadGdtfArchive(sourcePath);
    for (const ArchiveDiagnostic &diagnostic : archive.diagnostics) {
      const bool warning = diagnostic.code ==
                           ArchiveDiagnosticCode::NonCanonicalDescriptionXml;
      AddDiagnostic(result,
                    warning ? FixtureInsertionDiagnosticSeverity::Warning
                            : FixtureInsertionDiagnosticSeverity::Error,
                    "archive-read", ToCode(diagnostic.code), diagnostic.message,
                    diagnostic.entryPath);
    }
    result.tolerantNonStandardInputAccepted =
        archive.usedCompatibilityDescriptionFallback;
    result.standardsCompliantForCheckedRead =
        archive.standardsCompliantDescriptionLocation;
    if (!archive.Success())
      return result;

    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Info,
                  "description-read", "description-read-started",
                  "Reading GDTF description.", archive.descriptionEntryPath);
    const GdtfDescriptionSnapshot description =
        ReadGdtfDescription(archive.descriptionXml, [&archive] {
          std::vector<std::string> paths;
          paths.reserve(archive.entries.size());
          for (const ArchiveEntry &entry : archive.entries)
            paths.push_back(entry.path);
          return paths;
        }());
    bool fatalDescription = false;
    for (const DescriptionDiagnostic &diagnostic : description.diagnostics) {
      const bool fatal = IsFatalDescriptionDiagnostic(diagnostic.code);
      fatalDescription |= fatal;
      AddDiagnostic(result,
                    fatal ? FixtureInsertionDiagnosticSeverity::Error
                          : FixtureInsertionDiagnosticSeverity::Warning,
                    "description-read", ToCode(diagnostic.code),
                    diagnostic.message, diagnostic.path);
    }
    if (fatalDescription || !description.Success())
      return result;

    result.fixtureDisplayName = DisplayNameFromDescription(description, sourcePath);
    result.dmxModeNames = description.dmxModeNames;
    if (description.weightKgPresent)
      result.weightKg = description.weightKg;
    if (description.powerConsumptionWPresent)
      result.powerConsumptionW = description.powerConsumptionW;
    result.modelColorHex = description.modelColorHex;
    result.success = true;
    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Info,
                  "mode-read", "modes-ready", "GDTF fixture modes are ready.",
                  std::to_string(result.dmxModeNames.size()));
  } catch (const std::filesystem::filesystem_error &error) {
    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                  "failed", "filesystem-exception", error.what());
  } catch (const std::system_error &error) {
    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                  "failed", "system-exception", error.what());
  } catch (const std::exception &error) {
    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                  "failed", "std-exception", error.what());
  } catch (...) {
    AddDiagnostic(result, FixtureInsertionDiagnosticSeverity::Error,
                  "failed", "unknown-exception",
                  "Unknown error while preparing GDTF fixture insertion.");
  }
  return result;
}

// Returns the first error diagnostic from a fixture insertion preparation result.
const FixtureInsertionDiagnostic *FirstError(
    const GdtfFixtureInsertionPreparation &preparation) {
  for (const FixtureInsertionDiagnostic &diagnostic : preparation.diagnostics) {
    if (diagnostic.severity == FixtureInsertionDiagnosticSeverity::Error)
      return &diagnostic;
  }
  return nullptr;
}

// Builds a concise diagnostic string for user-facing and log summaries.
std::string SummarizeFixtureInsertionDiagnostics(
    const GdtfFixtureInsertionPreparation &preparation) {
  std::ostringstream output;
  bool first = true;
  for (const FixtureInsertionDiagnostic &diagnostic : preparation.diagnostics) {
    if (diagnostic.severity == FixtureInsertionDiagnosticSeverity::Info)
      continue;
    if (!first)
      output << "\n";
    first = false;
    output << diagnostic.stage << ": " << diagnostic.message;
    if (!diagnostic.detail.empty())
      output << " (" << diagnostic.detail << ")";
  }
  return output.str();
}

} // namespace gdtf
