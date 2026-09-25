#include "inspection/gdtf_inspection.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <utility>

namespace perastage::inspection {
namespace {

// Describes the neutral representation of one existing reader diagnostic.
struct DiagnosticMapping {
  DiagnosticSeverity severity;
  DiagnosticDomain domain;
  DiagnosticClassification classification;
  const char *code;
};

// Maps archive-reader findings without changing their established semantics.
DiagnosticMapping MapArchiveDiagnostic(gdtf::ArchiveDiagnosticCode code) {
  using Code = gdtf::ArchiveDiagnosticCode;
  switch (code) {
  case Code::None:
    return {DiagnosticSeverity::Information, DiagnosticDomain::Package,
            DiagnosticClassification::General, "gdtf.archive.none"};
  case Code::NonCanonicalDescriptionXml:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Package,
            DiagnosticClassification::Compatibility,
            "gdtf.archive.non_canonical_description_xml"};
  case Code::Utf8FlagMissing:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Package,
            DiagnosticClassification::Compatibility,
            "gdtf.archive.utf8_flag_missing"};
  case Code::Utf8FallbackUsed:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Package,
            DiagnosticClassification::Compatibility,
            "gdtf.archive.utf8_fallback_used"};
  case Code::LegacyFilenameEncodingUsed:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Package,
            DiagnosticClassification::Compatibility,
            "gdtf.archive.legacy_filename_encoding_used"};
  case Code::EmptySourcePath:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
            DiagnosticClassification::General,
            "gdtf.archive.empty_source_path"};
  case Code::OpenFailed:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
            DiagnosticClassification::General, "gdtf.archive.open_failed"};
  case Code::NoReadableEntries:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.no_readable_entries"};
  case Code::MissingDescriptionXml:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::Standards,
            "gdtf.archive.missing_description_xml"};
  case Code::DuplicateDescriptionXml:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::Standards,
            "gdtf.archive.duplicate_description_xml"};
  case Code::AmbiguousDescriptionXml:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.ambiguous_description_xml"};
  case Code::EmptyDescriptionXml:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Xml,
            DiagnosticClassification::General,
            "gdtf.archive.empty_description_xml"};
  case Code::UnsafeEntryPath:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.unsafe_entry_path"};
  case Code::EntryReadFailed:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.entry_read_failed"};
  case Code::EntryTooLarge:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.entry_too_large"};
  case Code::FilesystemError:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
            DiagnosticClassification::General,
            "gdtf.archive.filesystem_error"};
  case Code::UnexpectedException:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.unexpected_exception"};
  case Code::FilenameDecodeFailed:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.filename_decode_failed"};
  case Code::FilenameEncodingAmbiguous:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
            DiagnosticClassification::General,
            "gdtf.archive.filename_encoding_ambiguous"};
  case Code::ResourceNotFound:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.archive.resource_not_found"};
  case Code::ResourcePathAmbiguous:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.archive.resource_path_ambiguous"};
  case Code::ResourceEntryTooLarge:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.archive.resource_entry_too_large"};
  case Code::ResourceReadFailed:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.archive.resource_read_failed"};
  case Code::UnsafeResourcePath:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.archive.unsafe_resource_path"};
  case Code::ResourceFilenameDecodeFailed:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.archive.resource_filename_decode_failed"};
  }
  return {DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
          DiagnosticClassification::General, "gdtf.archive.unknown"};
}

// Maps description-reader findings without independently interpreting XML.
DiagnosticMapping
MapDescriptionDiagnostic(gdtf::DescriptionDiagnosticCode code) {
  using Code = gdtf::DescriptionDiagnosticCode;
  switch (code) {
  case Code::None:
    return {DiagnosticSeverity::Information, DiagnosticDomain::Xml,
            DiagnosticClassification::General, "gdtf.description.none"};
  case Code::MalformedXml:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Xml,
            DiagnosticClassification::General,
            "gdtf.description.malformed_xml"};
  case Code::MissingRoot:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Xml,
            DiagnosticClassification::Standards,
            "gdtf.description.missing_root"};
  case Code::MissingFixtureType:
    return {DiagnosticSeverity::Fatal, DiagnosticDomain::Content,
            DiagnosticClassification::Standards,
            "gdtf.description.missing_fixture_type"};
  case Code::MissingDmxModes:
    return {DiagnosticSeverity::Error, DiagnosticDomain::Content,
            DiagnosticClassification::Standards,
            "gdtf.description.missing_dmx_modes"};
  case Code::MissingUsableDmxMode:
    return {DiagnosticSeverity::Error, DiagnosticDomain::Content,
            DiagnosticClassification::Standards,
            "gdtf.description.missing_usable_dmx_mode"};
  case Code::UnknownElement:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.description.unknown_element"};
  case Code::MissingLocalResource:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.description.missing_local_resource"};
  case Code::MissingWheelMediaResource:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.description.missing_wheel_media_resource"};
  case Code::AmbiguousWheelMediaResource:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Content,
            DiagnosticClassification::General,
            "gdtf.description.ambiguous_wheel_media_resource"};
  case Code::NonCanonicalWheelMediaCaseMatch:
    return {DiagnosticSeverity::Warning, DiagnosticDomain::Content,
            DiagnosticClassification::Compatibility,
            "gdtf.description.non_canonical_wheel_media_case_match"};
  }
  return {DiagnosticSeverity::Error, DiagnosticDomain::Xml,
          DiagnosticClassification::General, "gdtf.description.unknown"};
}

// Appends an adapted archive diagnostic while retaining the original snapshot.
void AppendArchiveDiagnostic(GdtfInspectionResult &result,
                             const gdtf::ArchiveDiagnostic &source) {
  const DiagnosticMapping mapping = MapArchiveDiagnostic(source.code);
  Diagnostic diagnostic{mapping.severity, mapping.domain, mapping.classification,
                        mapping.code, source.message, std::nullopt};
  DiagnosticLocation location;
  location.sourcePath = result.inspection.request.sourcePath;
  if (!source.entryPath.empty())
    location.packageEntry = source.entryPath;
  diagnostic.location = std::move(location);
  result.inspection.diagnostics.push_back(std::move(diagnostic));
}

// Appends an adapted description diagnostic with XML and package context.
void AppendDescriptionDiagnostic(GdtfInspectionResult &result,
                                 const gdtf::DescriptionDiagnostic &source,
                                 const std::string &entryPath) {
  const DiagnosticMapping mapping = MapDescriptionDiagnostic(source.code);
  Diagnostic diagnostic{mapping.severity, mapping.domain, mapping.classification,
                        mapping.code, source.message, std::nullopt};
  DiagnosticLocation location;
  location.sourcePath = result.inspection.request.sourcePath;
  if (!entryPath.empty())
    location.packageEntry = entryPath;
  if (!source.path.empty())
    location.xmlPath = source.path;
  diagnostic.location = std::move(location);
  result.inspection.diagnostics.push_back(std::move(diagnostic));
}

// Reports whether any adapted finding records compatibility acceptance.
bool HasCompatibilityDiagnostic(const Result &inspection) {
  for (const Diagnostic &diagnostic : inspection.diagnostics) {
    if (diagnostic.classification == DiagnosticClassification::Compatibility)
      return true;
  }
  return false;
}

// Summarizes existing reader findings as an independent semantic layer.
ValidationResult SemanticValidation(const Result &inspection) {
  ValidationResult validation;
  validation.layer = ValidationLayer::SemanticInteroperability;
  validation.status = ValidationStatus::Valid;
  for (const Diagnostic &diagnostic : inspection.diagnostics) {
    if (diagnostic.domain != DiagnosticDomain::Package &&
        diagnostic.classification == DiagnosticClassification::Standards) {
      validation.diagnostics.push_back(diagnostic);
      if (diagnostic.severity >= DiagnosticSeverity::Error)
        validation.status = ValidationStatus::Invalid;
    }
  }
  return validation;
}

// Removes the scoped byte-inspection workspace when inspection completes.
class ScopedInspectionFile {
public:
  // Writes owned bytes to one unique, short-lived local inspection file.
  explicit ScopedInspectionFile(const std::vector<std::uint8_t> &bytes) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("perastage-gdtf-inspection-" + std::to_string(stamp) + "-" +
                  std::to_string(sequence.fetch_add(1)));
    std::error_code error;
    std::filesystem::create_directory(directory_, error);
    if (error)
      return;
    path_ = directory_ / "nested.gdtf";
    std::ofstream output(path_, std::ios::binary);
    if (!bytes.empty())
      output.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    valid_ = output.good();
  }

  // Prevents a scoped workspace from acquiring multiple cleanup owners.
  ScopedInspectionFile(const ScopedInspectionFile &) = delete;
  // Prevents reassignment of the scoped workspace cleanup owner.
  ScopedInspectionFile &operator=(const ScopedInspectionFile &) = delete;

  // Removes all scoped inspection bytes without exposing their path.
  ~ScopedInspectionFile() {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }

  // Reports whether the complete owned buffer was published temporarily.
  bool Valid() const { return valid_; }
  // Returns the internal path consumed only by the established reader.
  const std::filesystem::path &Path() const { return path_; }

private:
  std::filesystem::path directory_;
  std::filesystem::path path_;
  bool valid_ = false;
};

} // namespace

// Reports whether a usable semantic document was produced.
bool GdtfInspectionResult::Success() const {
  return document.has_value() && document->Valid() &&
         !inspection.HasFatalDiagnostics();
}

// Inspects one GDTF through package inventory and established read services.
GdtfInspectionResult InspectGdtf(const Request &request) {
  GdtfInspectionResult result;
  PackageInspectionResult package = InspectPackage(request);
  result.inspection = std::move(package.inspection);
  result.packageInventory = std::move(package.inventory);
  if (result.inspection.HasFatalDiagnostics())
    return result;
  if (!result.packageInventory ||
      result.packageInventory->kind != PackageKind::Gdtf) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Fatal;
    diagnostic.domain = DiagnosticDomain::Input;
    diagnostic.code = "gdtf.input.unsupported_package_kind";
    diagnostic.message = "The input package is not a GDTF file.";
    diagnostic.location = DiagnosticLocation{request.sourcePath};
    result.inspection.diagnostics.push_back(std::move(diagnostic));
    return result;
  }

  gdtf::GdtfDocument document = gdtf::LoadGdtfDocument(request.sourcePath);
  for (const auto &diagnostic : document.Archive().diagnostics)
    AppendArchiveDiagnostic(result, diagnostic);
  for (const auto &diagnostic : document.Description().diagnostics) {
    AppendDescriptionDiagnostic(result, diagnostic,
                                document.Archive().descriptionEntryPath);
  }
  DiagnosticLocation validationLocation;
  validationLocation.sourcePath = request.sourcePath;
  validationLocation.packageEntry = document.Archive().descriptionEntryPath;
  XmlSchemaValidationResult validation = ValidateXmlAgainstSchema(
      document.Archive().descriptionXml, Gdtf12Schema(), validationLocation);
  result.validation.push_back(std::move(validation.xml));
  result.validation.push_back(std::move(validation.schema));
  result.validation.push_back(SemanticValidation(result.inspection));
  result.document = std::move(document);
  if (result.Success()) {
    result.status = HasCompatibilityDiagnostic(result.inspection)
                        ? GdtfReadStatus::CompatibilityAccepted
                        : GdtfReadStatus::Canonical;
  }
  return result;
}

// Wraps a filesystem path in the neutral GDTF inspection request.
GdtfInspectionResult InspectGdtf(const std::filesystem::path &sourcePath) {
  return InspectGdtf(Request{sourcePath});
}

// Inspects owned GDTF bytes through the same established filesystem reader.
GdtfInspectionResult InspectGdtf(const std::vector<std::uint8_t> &bytes,
                                 const Request &request) {
  ScopedInspectionFile source(bytes);
  if (!source.Valid()) {
    GdtfInspectionResult result;
    result.inspection.request = request;
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Fatal;
    diagnostic.domain = DiagnosticDomain::Input;
    diagnostic.code = "gdtf.input.byte_workspace_failed";
    diagnostic.message =
        "A scoped workspace for GDTF byte inspection could not be created.";
    result.inspection.diagnostics.push_back(std::move(diagnostic));
    return result;
  }
  GdtfInspectionResult result = InspectGdtf(source.Path());
  result.inspection.request = request;
  for (Diagnostic &diagnostic : result.inspection.diagnostics) {
    if (diagnostic.location)
      diagnostic.location->sourcePath = request.sourcePath;
  }
  for (ValidationResult &validation : result.validation) {
    for (Diagnostic &diagnostic : validation.diagnostics) {
      if (diagnostic.location)
        diagnostic.location->sourcePath = request.sourcePath;
    }
  }
  if (result.document) {
    gdtf::ArchiveReadResult archive = result.document->Archive();
    archive.sourcePath = request.sourcePath;
    result.document =
        gdtf::GdtfDocument(std::move(archive), result.document->Description());
  }
  return result;
}

} // namespace perastage::inspection
