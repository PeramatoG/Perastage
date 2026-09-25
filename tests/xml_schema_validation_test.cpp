#include "inspection/xml_schema_validation.h"

#include <cassert>
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"><Scene/>"
      "</GeneralSceneDescription>";
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\">"
      "<UserData/><Scene/></GeneralSceneDescription>",
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
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"/>",
      schema);
  assert(external.xml.status == ValidationStatus::Valid);
  assert(external.schema.status == ValidationStatus::Valid);
  return 0;
}
