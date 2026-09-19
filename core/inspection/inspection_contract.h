#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace perastage::inspection {

// Orders diagnostic impact from informational context through read failure.
enum class DiagnosticSeverity : std::uint8_t {
  Information,
  Warning,
  Error,
  Fatal,
};

// Identifies the technical area that produced a diagnostic.
enum class DiagnosticDomain : std::uint8_t {
  Input,
  Package,
  Xml,
  Content,
};

// Separates general findings from standards and compatibility findings.
enum class DiagnosticClassification : std::uint8_t {
  General,
  Standards,
  Compatibility,
};

// Locates a finding in its source file, package entry, or XML document.
struct DiagnosticLocation {
  std::optional<std::filesystem::path> sourcePath;
  std::optional<std::string> packageEntry;
  std::optional<std::string> xmlPath;
  std::optional<std::uint32_t> line;
  std::optional<std::uint32_t> column;
};

// Carries one presentation-neutral finding in deterministic result order.
struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Information;
  DiagnosticDomain domain = DiagnosticDomain::Content;
  DiagnosticClassification classification =
      DiagnosticClassification::General;
  // Stable codes use technical English dotted identifiers such as
  // "package.open_failed" and are never localized presentation strings.
  std::string code;
  std::string message;
  std::optional<DiagnosticLocation> location;
};

// Identifies the filesystem input for one read-only inspection.
struct Request {
  std::filesystem::path sourcePath;
};

// Owns ordered structured findings returned by a read-only inspection.
struct Result {
  Request request;
  std::vector<Diagnostic> diagnostics;

  bool HasFatalDiagnostics() const;
  bool Success() const;
  std::optional<DiagnosticSeverity> WorstSeverity() const;
};

} // namespace perastage::inspection
