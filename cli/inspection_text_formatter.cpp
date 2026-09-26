#include "inspection_text_formatter.h"

#include <array>
#include <sstream>

namespace perastage::cli {
namespace {

// Converts a source path to the shared UTF-8 generic representation.
std::string PathText(const std::filesystem::path &path) {
  const std::u8string text = path.generic_u8string();
  return std::string(text.begin(), text.end());
}

// Returns a stable severity token for human technical output.
const char *Severity(inspection::DiagnosticSeverity value) {
  using S = inspection::DiagnosticSeverity;
  switch (value) {
  case S::Information:
    return "information";
  case S::Warning:
    return "warning";
  case S::Error:
    return "error";
  case S::Fatal:
    return "fatal";
  }
  return "unknown";
}

// Returns a stable diagnostic classification token.
const char *Classification(inspection::DiagnosticClassification value) {
  using C = inspection::DiagnosticClassification;
  switch (value) {
  case C::General:
    return "general";
  case C::Standards:
    return "standards";
  case C::Compatibility:
    return "compatibility";
  }
  return "unknown";
}

// Returns a stable diagnostic domain token.
const char *Domain(inspection::DiagnosticDomain value) {
  using D = inspection::DiagnosticDomain;
  switch (value) {
  case D::Input:
    return "input";
  case D::Package:
    return "package";
  case D::Xml:
    return "xml";
  case D::Content:
    return "content";
  }
  return "unknown";
}

// Returns a stable validation status token.
const char *ValidationStatus(inspection::ValidationStatus value) {
  using S = inspection::ValidationStatus;
  switch (value) {
  case S::NotRun:
    return "not-run";
  case S::Unavailable:
    return "unavailable";
  case S::Valid:
    return "valid";
  case S::Invalid:
    return "invalid";
  }
  return "unknown";
}

// Returns a stable validation layer token.
const char *ValidationLayer(inspection::ValidationLayer value) {
  using L = inspection::ValidationLayer;
  switch (value) {
  case L::XmlWellFormedness:
    return "xml-well-formedness";
  case L::Schema:
    return "schema";
  case L::SemanticInteroperability:
    return "semantic-interoperability";
  }
  return "unknown";
}

// Returns a stable resource-kind token.
const char *ResourceKind(inspection::ResourceKind value) {
  using K = inspection::ResourceKind;
  switch (value) {
  case K::XmlText:
    return "xml-text";
  case K::Text:
    return "text";
  case K::Image:
    return "image";
  case K::Model:
    return "model";
  case K::NestedGdtf:
    return "nested-gdtf";
  case K::Binary:
    return "binary";
  }
  return "unknown";
}

// Counts all public diagnostics by severity.
std::array<std::size_t, 4>
DiagnosticCounts(const inspection::Result &result,
                 const std::vector<inspection::ValidationResult> &validation) {
  std::array<std::size_t, 4> counts{};
  for (const auto &item : result.diagnostics)
    ++counts[static_cast<std::size_t>(item.severity)];
  for (const auto &layer : validation)
    for (const auto &item : layer.diagnostics)
      ++counts[static_cast<std::size_t>(item.severity)];
  return counts;
}

// Appends common deterministic summary fields.
void AppendCommon(std::ostringstream &out, const inspection::Result &result,
                  const std::vector<inspection::ValidationResult> &validation,
                  std::size_t entries, std::size_t resources) {
  out << "Input: " << PathText(result.request.sourcePath) << '\n';
  out << "Package entries: " << entries << '\n';
  out << "Resources: " << resources << '\n';
  out << "Validation:";
  for (const auto &layer : validation)
    out << ' ' << ValidationLayer(layer.layer) << '='
        << ValidationStatus(layer.status);
  out << '\n';
  const auto counts = DiagnosticCounts(result, validation);
  out << "Diagnostics: information=" << counts[0] << " warning=" << counts[1]
      << " error=" << counts[2] << " fatal=" << counts[3] << '\n';
}

// Appends one diagnostic record with all available stable facts.
void AppendDiagnostic(std::ostringstream &out,
                      const inspection::Diagnostic &item) {
  out << Severity(item.severity) << " | " << Classification(item.classification)
      << " | " << Domain(item.domain) << " | " << item.code << " | "
      << item.message;
  if (item.location) {
    out << " | location=";
    if (item.location->sourcePath)
      out << PathText(*item.location->sourcePath);
    if (item.location->packageEntry)
      out << '#' << *item.location->packageEntry;
    if (item.location->xmlPath)
      out << ':' << *item.location->xmlPath;
    if (item.location->line)
      out << ':' << *item.location->line;
    if (item.location->column)
      out << ':' << *item.location->column;
  }
  out << '\n';
}

} // namespace

// Formats a deterministic GDTF summary from the retained document.
std::string FormatGdtfSummary(
    const inspection::GdtfInspectionResult &result,
    const std::vector<inspection::ResourceDescriptor> &resources) {
  std::ostringstream out;
  out << "Format: GDTF\n";
  out << "Status: "
      << (result.status == inspection::GdtfReadStatus::Canonical ? "canonical"
          : result.status == inspection::GdtfReadStatus::CompatibilityAccepted
              ? "compatibility-accepted"
              : "unusable")
      << '\n';
  AppendCommon(out, result.inspection, result.validation,
               result.packageInventory ? result.packageInventory->entries.size()
                                       : 0,
               resources.size());
  if (result.document) {
    const auto &item = result.document->Description();
    out << "DataVersion: " << item.dataVersion << '\n'
        << "Manufacturer: " << item.manufacturer << '\n'
        << "Fixture type: " << item.fixtureTypeName << '\n'
        << "Short name: " << item.shortName << '\n'
        << "Long name: " << item.longName << '\n'
        << "DMX modes: " << item.dmxModeNames.size() << '\n'
        << "Wheels: " << item.wheels.size() << '\n';
  }
  return out.str();
}

// Formats a deterministic MVR summary from the retained snapshot.
std::string
FormatMvrSummary(const inspection::MvrInspectionResult &result,
                 const std::vector<inspection::ResourceDescriptor> &resources) {
  std::ostringstream out;
  out << "Format: MVR\n"
      << "Status: " << (result.Success() ? "inspected" : "unusable") << '\n';
  AppendCommon(out, result.inspection, result.validation,
               result.packageInventory ? result.packageInventory->entries.size()
                                       : 0,
               resources.size());
  if (result.snapshot) {
    const auto &item = *result.snapshot;
    out << "MVR version: " << item.versionMajor << '.' << item.versionMinor
        << '\n'
        << "Provider: " << item.provider << '\n'
        << "Provider version: " << item.providerVersion << '\n';
    for (const auto &count : item.nodeCounts)
      out << count.type << ": " << count.count << '\n';
    out << "embedded_gdtfs: " << item.embeddedGdtfEntries.size() << '\n'
        << "referenced_resources: " << item.referencedResources.size() << '\n';
  }
  return out.str();
}

// Formats package entries in authoritative inventory order.
std::string FormatInventory(const inspection::PackageInventory &inventory) {
  std::ostringstream out;
  for (const auto &item : inventory.entries) {
    out << (item.type == inspection::PackageEntryType::File ? "file"
                                                            : "directory")
        << " | "
        << (item.normalizedPath ? *item.normalizedPath : item.displayPath)
        << " | size=";
    if (item.sizeKnown)
      out << item.uncompressedSize;
    else
      out << "unknown";
    out << " | path=" << (item.pathSafe ? "safe" : "unsafe") << '\n';
  }
  return out.str();
}

// Formats ordered resource descriptors without reading their full payloads.
std::string
FormatResources(const std::vector<inspection::ResourceDescriptor> &resources) {
  std::ostringstream out;
  for (const auto &item : resources) {
    out << (item.normalizedPath ? *item.normalizedPath : item.displayPath)
        << " | kind=" << ResourceKind(item.kind) << " | size=";
    if (item.sizeKnown)
      out << item.size;
    else
      out << "unknown";
    out << " | raw-read=" << (item.rawReadSupported ? "yes" : "no")
        << " | text-preview=" << (item.textPreviewSupported ? "yes" : "no")
        << " | path=" << (item.pathSafe ? "safe" : "unsafe") << '\n';
  }
  return out.str();
}

// Formats top-level and validation diagnostics without deduplication.
std::string
FormatDiagnostics(const inspection::Result &result,
                  const std::vector<inspection::ValidationResult> &validation) {
  std::ostringstream out;
  for (const auto &item : result.diagnostics)
    AppendDiagnostic(out, item);
  for (const auto &layer : validation)
    for (const auto &item : layer.diagnostics)
      AppendDiagnostic(out, item);
  return out.str();
}

} // namespace perastage::cli
