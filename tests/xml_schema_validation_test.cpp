#include "inspection/xml_schema_validation.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace perastage::inspection;

// Finds a validation diagnostic by stable code.
bool HasCode(const ValidationResult &result, const std::string &code) {
  for (const Diagnostic &diagnostic : result.diagnostics)
    if (diagnostic.code == code)
      return true;
  return false;
}

// Reports whether a validation stage contains one diagnostic severity.
bool HasSeverity(const ValidationResult &result, DiagnosticSeverity severity) {
  for (const Diagnostic &diagnostic : result.diagnostics)
    if (diagnostic.severity == severity)
      return true;
  return false;
}

// Reads one repository-controlled characterization fixture as exact bytes.
std::string ReadFixture(const std::string &relativePath) {
  std::ifstream input(std::filesystem::path(PERASTAGE_TEST_SOURCE_DIR) /
                          relativePath,
                      std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Exercises independent XML, schema, determinism, location, and offline checks.
int main() {
  const SchemaDescriptor schema = Mvr16Schema();
  assert(schema.identity.schemaVersion == "mvr-1.6");
  assert(schema.identity.provenance == "mvrdevelopment/tools:mvr.xsd");
  assert(schema.identity.sourceRevision ==
         "e199c6ed635de23cb5ebf9654ee54a358775a065");
  const std::string valid =
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"1.7\"><Scene><Layers/>"
      "</Scene></GeneralSceneDescription>";
  const auto accepted = ValidateXmlAgainstSchema(valid, schema);
  assert(accepted.xml.status == ValidationStatus::Valid);
  assert(accepted.schema.status == ValidationStatus::Valid);

  const auto missingAttribute = ValidateXmlAgainstSchema(
      "<GeneralSceneDescription verMajor=\"1\"/>", schema);
  assert(missingAttribute.xml.status == ValidationStatus::Valid);
  assert(missingAttribute.schema.status == ValidationStatus::Invalid);
  assert(HasCode(missingAttribute.schema, "schema.document.invalid"));

  const auto malformed = ValidateXmlAgainstSchema(
      "<GeneralSceneDescription\n verMajor=\"1\">", schema);
  assert(malformed.xml.status == ValidationStatus::Invalid);
  assert(malformed.schema.status == ValidationStatus::NotRun);
  assert(HasCode(malformed.xml, "xml.well_formedness.invalid"));
  assert(!malformed.xml.diagnostics.empty());
  assert(malformed.xml.diagnostics.front().location);
  assert(malformed.xml.diagnostics.front().location->line);

  const auto wrongOrder = ValidateXmlAgainstSchema(
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
      "provider=\"Perastage\" providerVersion=\"1.7\">"
      "<Scene><Layers/></Scene><UserData/></GeneralSceneDescription>",
      schema);
  assert(wrongOrder.xml.status == ValidationStatus::Valid);
  assert(wrongOrder.schema.status == ValidationStatus::Invalid);

  const auto invalidType = ValidateXmlAgainstSchema(
      "<GeneralSceneDescription verMajor=\"one\" verMinor=\"6\"/>", schema);
  assert(invalidType.schema.status == ValidationStatus::Invalid);
  const auto repeated = ValidateXmlAgainstSchema(
      "<GeneralSceneDescription verMajor=\"one\" verMinor=\"6\"/>", schema);
  assert(repeated.schema.status == invalidType.schema.status);
  assert(repeated.schema.diagnostics.front().message ==
         invalidType.schema.diagnostics.front().message);

  const auto external = ValidateXmlAgainstSchema(
      "<!DOCTYPE GeneralSceneDescription SYSTEM \"http://127.0.0.1:9/no.dtd\">"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene><Layers/>"
      "</Scene></GeneralSceneDescription>",
      schema);
  assert(external.xml.status == ValidationStatus::Valid);
  assert(!HasSeverity(external.xml, DiagnosticSeverity::Error));
  assert(external.schema.status == ValidationStatus::Valid);

  const std::filesystem::path localDtd =
      std::filesystem::temp_directory_path() / "perastage-validation.dtd";
  {
    std::ofstream output(localDtd);
    output << "<!ATTLIST GeneralSceneDescription provider CDATA #FIXED "
              "\"Loaded\" providerVersion CDATA #FIXED \"Loaded\">";
  }
  const std::string localXml =
      "<!DOCTYPE GeneralSceneDescription SYSTEM \"" + localDtd.string() +
      "\"><GeneralSceneDescription verMajor=\"1\" verMinor=\"6\">"
      "<Scene><Layers/></Scene></GeneralSceneDescription>";
  const auto localExternal = ValidateXmlAgainstSchema(localXml, schema);
  assert(localExternal.xml.status == ValidationStatus::Valid);
  assert(!HasSeverity(localExternal.xml, DiagnosticSeverity::Error));
  assert(localExternal.schema.status == ValidationStatus::Valid);
  std::filesystem::remove(localDtd);

  SchemaDescriptor importing = schema;
  importing.xsd =
      "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" "
      "targetNamespace=\"urn:local\"><xs:import namespace=\"urn:external\" "
      "schemaLocation=\"file:///definitely-not-readable/external.xsd\"/>"
      "</xs:schema>";
  const auto denied = ValidateXmlAgainstSchema(valid, importing);
  assert(denied.schema.status == ValidationStatus::Unavailable);
  assert(HasCode(denied.schema, "validation.external_resource_denied"));

  const auto officialMvr = ValidateXmlAgainstSchema(
      ReadFixture("tests/fixtures/standards/mvr/1.6/official-vocabulary.xml"),
      Mvr16Schema());
  assert(officialMvr.schema.status == ValidationStatus::Valid);
  const auto officialGdtf = ValidateXmlAgainstSchema(
      ReadFixture("tests/fixtures/standards/gdtf/1.2/official-structures.xml"),
      Gdtf12Schema());
  assert(officialGdtf.schema.status == ValidationStatus::Valid);
  const auto invalidReference = ValidateXmlAgainstSchema(
      ReadFixture(
          "tests/fixtures/standards/gdtf/1.2/invalid-geometry-reference.xml"),
      Gdtf12Schema());
  assert(invalidReference.schema.status == ValidationStatus::Invalid);
  return 0;
}
