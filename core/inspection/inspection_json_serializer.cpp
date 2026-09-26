#include "inspection_json_serializer.h"

#include "inspection_json_serialization_detail.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace perastage::inspection::serialization {
namespace detail {
namespace {

// Converts a filesystem path to UTF-8 with generic path separators.
std::string SerializePath(const std::filesystem::path &path) {
  const std::u8string utf8Path = path.generic_u8string();
  return std::string(utf8Path.begin(), utf8Path.end());
}

// Returns the stable machine token for a diagnostic severity.
const char *SeverityToken(DiagnosticSeverity severity) {
  switch (severity) {
  case DiagnosticSeverity::Information:
    return "information";
  case DiagnosticSeverity::Warning:
    return "warning";
  case DiagnosticSeverity::Error:
    return "error";
  case DiagnosticSeverity::Fatal:
    return "fatal";
  }
  throw std::invalid_argument("Unknown inspection diagnostic severity");
}

// Returns the stable machine token for a diagnostic domain.
const char *DomainToken(DiagnosticDomain domain) {
  switch (domain) {
  case DiagnosticDomain::Input:
    return "input";
  case DiagnosticDomain::Package:
    return "package";
  case DiagnosticDomain::Xml:
    return "xml";
  case DiagnosticDomain::Content:
    return "content";
  }
  throw std::invalid_argument("Unknown inspection diagnostic domain");
}

// Returns the stable machine token for a diagnostic classification.
const char *ClassificationToken(DiagnosticClassification classification) {
  switch (classification) {
  case DiagnosticClassification::General:
    return "general";
  case DiagnosticClassification::Standards:
    return "standards";
  case DiagnosticClassification::Compatibility:
    return "compatibility";
  }
  throw std::invalid_argument("Unknown inspection diagnostic classification");
}

// Serializes only the location fields present in the semantic result.
nlohmann::json SerializeLocation(const DiagnosticLocation &location) {
  nlohmann::json serialized = nlohmann::json::object();
  if (location.sourcePath)
    serialized["source_path"] = SerializePath(*location.sourcePath);
  if (location.packageEntry)
    serialized["package_entry"] = *location.packageEntry;
  if (location.xmlPath)
    serialized["xml_path"] = *location.xmlPath;
  if (location.line)
    serialized["line"] = *location.line;
  if (location.column)
    serialized["column"] = *location.column;
  return serialized;
}

} // namespace

// Serializes one diagnostic without introducing presentation-only fields.
nlohmann::json SerializeDiagnostic(const Diagnostic &diagnostic) {
  nlohmann::json serialized{
      {"severity", SeverityToken(diagnostic.severity)},
      {"domain", DomainToken(diagnostic.domain)},
      {"classification", ClassificationToken(diagnostic.classification)},
      {"code", diagnostic.code},
      {"message", diagnostic.message}};
  if (diagnostic.location)
    serialized["location"] = SerializeLocation(*diagnostic.location);
  return serialized;
}

// Builds the backward-compatible base object shared by all report serializers.
nlohmann::json SerializeBase(const Result &result) {
  nlohmann::json diagnostics = nlohmann::json::array();
  for (const Diagnostic &diagnostic : result.diagnostics)
    diagnostics.push_back(SerializeDiagnostic(diagnostic));
  nlohmann::json root{
      {"schema_version", kInspectionJsonSchemaVersion},
      {"request", {{"source_path", SerializePath(result.request.sourcePath)}}},
      {"success", result.Success()},
      {"worst_severity", nullptr},
      {"diagnostics", std::move(diagnostics)}};
  if (const auto severity = result.WorstSeverity())
    root["worst_severity"] = SeverityToken(*severity);
  return root;
}

} // namespace detail

// Serializes a result to deterministic compact versioned JSON.
std::string SerializeResultToJson(const Result &result) {
  return detail::SerializeBase(result).dump();
}

} // namespace perastage::inspection::serialization
