#pragma once

#include "inspection/inspection_contract.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace perastage::inspection {

// Identifies the independent standards-checking stage that produced a result.
enum class ValidationLayer : std::uint8_t {
  XmlWellFormedness,
  Schema,
  SemanticInteroperability,
};

// Records the outcome of one validation stage without implying other stages.
enum class ValidationStatus : std::uint8_t {
  NotRun,
  Unavailable,
  Valid,
  Invalid,
};

// Identifies a repository-controlled schema and its specification basis.
struct SchemaIdentity {
  std::string format;
  std::string formatVersion;
  std::string schemaVersion;
  std::string provenance;
  std::string sourceRevision;
};

// Owns the neutral outcome and findings for one validation layer.
struct ValidationResult {
  ValidationLayer layer = ValidationLayer::XmlWellFormedness;
  ValidationStatus status = ValidationStatus::NotRun;
  std::optional<SchemaIdentity> schema;
  std::vector<Diagnostic> diagnostics;
};

// Describes one local, repository-controlled XSD input.
struct SchemaDescriptor {
  SchemaIdentity identity;
  std::filesystem::path path;
  std::string xsd;
};

// Owns distinct XML and XSD results for one immutable source buffer.
struct XmlSchemaValidationResult {
  ValidationResult xml;
  ValidationResult schema;
};

XmlSchemaValidationResult
ValidateXmlAgainstSchema(std::string_view xml, const SchemaDescriptor &schema,
                         const DiagnosticLocation &sourceLocation = {});
SchemaDescriptor Gdtf12Schema();
SchemaDescriptor Mvr16Schema();

} // namespace perastage::inspection
