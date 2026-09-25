#include "inspection/xml_schema_validation.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace perastage::inspection;

// Finds a validation diagnostic by stable code.
bool HasCode(const ValidationResult &result, const std::string &code) {
  for (const Diagnostic &diagnostic : result.diagnostics)
    if (diagnostic.code == code)
      return true;
  return false;
}

// Exercises independent XML, schema, determinism, location, and offline checks.
int main() {
  const SchemaDescriptor schema = Mvr16Schema();
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
  assert(external.schema.status == ValidationStatus::Invalid);

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
  assert(localExternal.schema.status == ValidationStatus::Invalid);
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
  return 0;
}
