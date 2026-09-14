#include "mvr_xml_document_writer.h"
#include "mvr_xml_extension_writer.h"
#include "mvr_xml_scene_object_writer.h"

// Verifies that the internal serializer headers compile without exporter context.
int main() {
  mvr_xml_serialization::FixtureValues values;
  return values.fixtureIdNumeric;
}
