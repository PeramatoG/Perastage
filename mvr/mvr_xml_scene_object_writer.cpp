#include "mvr_xml_scene_object_writer.h"

#include "matrixutils.h"

#include <tinyxml2.h>

namespace mvr_xml_serialization {

// Appends one standard MVR Fixture node in the established child order.
void AppendFixture(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent,
                   const FixtureValues &values) {
  tinyxml2::XMLElement *fixture = document.NewElement("Fixture");
  fixture->SetAttribute("uuid", values.uuid.c_str());
  fixture->SetAttribute("name", values.name.c_str());

  auto addInt = [&](const char *name, int value) {
    if (value != 0) {
      tinyxml2::XMLElement *element = document.NewElement(name);
      element->SetText(std::to_string(value).c_str());
      fixture->InsertEndChild(element);
    }
  };
  auto addString = [&](const char *name, const std::string &value) {
    if (!value.empty()) {
      tinyxml2::XMLElement *element = document.NewElement(name);
      element->SetText(value.c_str());
      fixture->InsertEndChild(element);
    }
  };

  tinyxml2::XMLElement *matrix = document.NewElement("Matrix");
  matrix->SetText(MatrixUtils::FormatMatrix(values.matrix).c_str());
  fixture->InsertEndChild(matrix);
  addString("GDTFSpec", values.gdtfSpec);
  addString("GDTFMode", values.gdtfMode);
  addString("Position", values.position);
  addString("FixtureID", values.fixtureId);
  addInt("FixtureIDNumeric", values.fixtureIdNumeric);
  addInt("UnitNumber", values.unitNumber);
  if (values.absoluteDmxAddress.has_value()) {
    tinyxml2::XMLElement *addresses = document.NewElement("Addresses");
    tinyxml2::XMLElement *address = document.NewElement("Address");
    address->SetAttribute("break", 0);
    address->SetText(std::to_string(*values.absoluteDmxAddress).c_str());
    addresses->InsertEndChild(address);
    fixture->InsertEndChild(addresses);
  }
  addString("Color", values.color);
  parent->InsertEndChild(fixture);
}

} // namespace mvr_xml_serialization
