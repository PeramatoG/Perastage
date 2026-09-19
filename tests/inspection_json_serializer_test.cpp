#include "inspection/inspection_json_serializer.h"

#include "json.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

using namespace perastage::inspection;
using namespace perastage::inspection::serialization;

namespace {

// Copies explicit UTF-8 code units into a byte string for JSON assertions.
std::string Utf8String(std::u8string_view value) {
  return std::string(reinterpret_cast<const char *>(value.data()),
                     value.size());
}

} // namespace

// Verifies the complete schema shape for an empty successful result.
void TestEmptySuccessfulResult() {
  const Result result{{std::filesystem::path("fixtures/example.gdtf")}, {}};
  const std::string serialized = SerializeResultToJson(result);
  const nlohmann::json parsed = nlohmann::json::parse(serialized);

  assert(parsed.size() == 5);
  assert(parsed.at("schema_version") == kInspectionJsonSchemaVersion);
  assert(parsed.at("schema_version").is_number_unsigned());
  assert(parsed.at("request").is_object());
  assert(parsed.at("request").size() == 1);
  assert(parsed.at("request").at("source_path") == "fixtures/example.gdtf");
  assert(parsed.at("success") == true);
  assert(parsed.at("worst_severity").is_null());
  assert(parsed.at("diagnostics").is_array());
  assert(parsed.at("diagnostics").empty());
  assert(
      serialized ==
      R"({"diagnostics":[],"request":{"source_path":"fixtures/example.gdtf"},"schema_version":1,"success":true,"worst_severity":null})");
}

// Verifies stable warning tokens and absent-location behavior.
void TestCompatibilityWarning() {
  Diagnostic warning;
  warning.severity = DiagnosticSeverity::Warning;
  warning.domain = DiagnosticDomain::Package;
  warning.classification = DiagnosticClassification::Compatibility;
  warning.code = "package.legacy_filename_encoding";
  warning.message = "Legacy filename encoding.";
  const Result result{{"fixture.gdtf"}, {warning}};
  const nlohmann::json parsed =
      nlohmann::json::parse(SerializeResultToJson(result));
  const auto &diagnostic = parsed.at("diagnostics").at(0);

  assert(parsed.at("success") == true);
  assert(parsed.at("worst_severity") == "warning");
  assert(diagnostic.at("severity") == "warning");
  assert(diagnostic.at("domain") == "package");
  assert(diagnostic.at("classification") == "compatibility");
  assert(diagnostic.at("code") == warning.code);
  assert(diagnostic.at("message") == warning.message);
  assert(!diagnostic.contains("location"));
}

// Verifies full locations, numeric coordinates, and standards classification.
void TestStandardsDiagnosticWithFullLocation() {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Error;
  diagnostic.domain = DiagnosticDomain::Xml;
  diagnostic.classification = DiagnosticClassification::Standards;
  diagnostic.code = "xml.required_element_missing";
  diagnostic.message = "A required XML element is missing.";
  diagnostic.location = DiagnosticLocation{
      std::filesystem::path("fixtures/example.gdtf"), "description.xml",
      "/GDTF/FixtureType", std::uint32_t{12}, std::uint32_t{4}};
  const Result result{{"fixtures/example.gdtf"}, {diagnostic}};
  const nlohmann::json parsed =
      nlohmann::json::parse(SerializeResultToJson(result));
  const auto &serializedDiagnostic = parsed.at("diagnostics").at(0);
  const auto &location = serializedDiagnostic.at("location");

  assert(serializedDiagnostic.at("classification") == "standards");
  assert(serializedDiagnostic.at("classification") != "compatibility");
  assert(location.at("source_path") == "fixtures/example.gdtf");
  assert(location.at("package_entry") == "description.xml");
  assert(location.at("xml_path") == "/GDTF/FixtureType");
  assert(location.at("line").is_number_unsigned());
  assert(location.at("column").is_number_unsigned());
  assert(location.at("line") == 12);
  assert(location.at("column") == 4);
}

// Verifies fatal failure semantics and the stable fatal token.
void TestFatalDiagnostic() {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Fatal;
  diagnostic.domain = DiagnosticDomain::Input;
  diagnostic.classification = DiagnosticClassification::General;
  diagnostic.code = "input.open_failed";
  diagnostic.message = "The input file could not be opened.";
  const Result result{{"missing.gdtf"}, {diagnostic}};
  const nlohmann::json parsed =
      nlohmann::json::parse(SerializeResultToJson(result));

  assert(parsed.at("success") == false);
  assert(parsed.at("worst_severity") == "fatal");
  assert(parsed.at("diagnostics").at(0).at("severity") == "fatal");
  assert(parsed.at("diagnostics").at(0).at("domain") == "input");
  assert(parsed.at("diagnostics").at(0).at("classification") == "general");
}

// Verifies insertion order and all remaining stable enum tokens.
void TestDiagnosticOrderingAndTokens() {
  Diagnostic information;
  information.severity = DiagnosticSeverity::Information;
  information.domain = DiagnosticDomain::Content;
  information.code = "content.first";
  information.message = "First.";
  Diagnostic error;
  error.severity = DiagnosticSeverity::Error;
  error.domain = DiagnosticDomain::Xml;
  error.code = "xml.second";
  error.message = "Second.";
  const Result result{{"ordered.gdtf"}, {information, error}};
  const nlohmann::json parsed =
      nlohmann::json::parse(SerializeResultToJson(result));

  assert(parsed.at("diagnostics").at(0).at("code") == "content.first");
  assert(parsed.at("diagnostics").at(0).at("severity") == "information");
  assert(parsed.at("diagnostics").at(0).at("domain") == "content");
  assert(parsed.at("diagnostics").at(1).at("code") == "xml.second");
  assert(parsed.at("diagnostics").at(1).at("severity") == "error");
  assert(parsed.at("diagnostics").at(1).at("domain") == "xml");
}

// Verifies UTF-8 paths, JSON escaping, omitted optionals, and determinism.
void TestUnicodeEscapingAndDeterminism() {
  const std::string expectedSourcePath = Utf8String(u8"資料/灯具.gdtf");
  const std::string expectedMessage =
      Utf8String(u8"Quoted \"message\" with newline\n雪");
  const std::string expectedPackageEntry =
      Utf8String(u8"géométrie/灯具\\name.json");
  Diagnostic diagnostic;
  diagnostic.code = "content.escaped";
  diagnostic.message = expectedMessage;
  diagnostic.location = DiagnosticLocation{};
  diagnostic.location->packageEntry = expectedPackageEntry;
  const Result result{{std::filesystem::path(u8"資料/灯具.gdtf")},
                      {diagnostic}};

  const std::string first = SerializeResultToJson(result);
  const std::string second = SerializeResultToJson(result);
  const nlohmann::json parsed = nlohmann::json::parse(first);
  const auto &location = parsed.at("diagnostics").at(0).at("location");

  assert(first == second);
  assert(first.find(R"(\"message\")") != std::string::npos);
  assert(first.find(R"(newline\n)") != std::string::npos);
  assert(parsed.at("request").at("source_path") == expectedSourcePath);
  assert(parsed.at("diagnostics").at(0).at("message") == expectedMessage);
  assert(location.at("package_entry") == expectedPackageEntry);
  assert(location.size() == 1);
}

// Runs the inspection JSON serialization contract checks.
int main() {
  TestEmptySuccessfulResult();
  TestCompatibilityWarning();
  TestStandardsDiagnosticWithFullLocation();
  TestFatalDiagnostic();
  TestDiagnosticOrderingAndTokens();
  TestUnicodeEscapingAndDeterminism();
}
