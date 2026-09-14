#pragma once

#include "types.h"

#include <optional>
#include <string>

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
}

namespace mvr_xml_serialization {

struct FixtureValues {
  std::string uuid;
  std::string name;
  Matrix matrix;
  std::string gdtfSpec;
  std::string gdtfMode;
  std::string position;
  std::string fixtureId;
  int fixtureIdNumeric = 0;
  int unitNumber = 0;
  std::optional<int> absoluteDmxAddress;
  std::string color;
};

// Appends one standard MVR Fixture node using fully resolved export values.
void AppendFixture(tinyxml2::XMLDocument &document,
                   tinyxml2::XMLElement *parent,
                   const FixtureValues &values);

} // namespace mvr_xml_serialization
