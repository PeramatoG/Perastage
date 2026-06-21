#include "gdtf_canonicalizer.h"

#include <cassert>
#include <string>
#include <vector>

#include <tinyxml2.h>

namespace {

// Parses XML text into a TinyXML document for canonicalizer checks.
tinyxml2::XMLDocument Parse(const std::string &xml) {
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS);
  return doc;
}

// Collects FixtureType child names in document order.
std::vector<std::string> FixtureTypeChildNames(tinyxml2::XMLDocument &doc) {
  std::vector<std::string> names;
  tinyxml2::XMLElement *fixtureType =
      doc.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  for (tinyxml2::XMLElement *child = fixtureType->FirstChildElement(); child;
       child = child->NextSiblingElement())
    names.emplace_back(child->Name());
  return names;
}

// Counts standard Revision nodes with the requested Text attribute.
int CountRevisionText(tinyxml2::XMLDocument &doc, const std::string &text) {
  int count = 0;
  tinyxml2::XMLElement *fixtureType =
      doc.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  for (tinyxml2::XMLElement *revision =
           revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *value = revision->Attribute("Text");
    if (value && text == value)
      ++count;
  }
  return count;
}

// Returns a valid minimal GDTF document with configurable FixtureType content.
std::string MinimalGdtf(const std::string &fixtureTypeInner) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Demo\" Manufacturer=\"Perastage\" "
         "FixtureTypeID=\"308EA87D-7164-42DE-8106-A6D273F57A51\">" +
         fixtureTypeInner + "</FixtureType></GDTF>";
}

} // namespace

// Verifies that legacy FixtureType structure is canonicalized for export.
int main() {
  {
    tinyxml2::XMLDocument doc = Parse(MinimalGdtf(
        "<AttributeDefinitions/><Models/><PerastageMutationAudit SchemaVersion=\"1\"/>"
        "<PhysicalDescriptions/><Geometries/><DMXModes/>"));
    GdtfCanonicalizer::Result result = GdtfCanonicalizer::CanonicalizeDescription(doc);
    assert(result.success);
    assert(result.changed);
    const std::vector<std::string> names = FixtureTypeChildNames(doc);
    const std::vector<std::string> expected = {"AttributeDefinitions", "PhysicalDescriptions",
                                              "Models", "Geometries", "DMXModes",
                                              "Revisions"};
    assert(names == expected);
    assert(doc.FirstChildElement("GDTF")
               ->FirstChildElement("FixtureType")
               ->FirstChildElement("PerastageMutationAudit") == nullptr);
    assert(CountRevisionText(doc, "Canonicalized GDTF structure for Perastage export") == 1);

    GdtfCanonicalizer::Result second = GdtfCanonicalizer::CanonicalizeDescription(doc);
    assert(second.success);
    assert(CountRevisionText(doc, "Canonicalized GDTF structure for Perastage export") == 1);
  }

  {
    tinyxml2::XMLDocument doc = Parse(
        "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Truss 300\" Manufacturer=\"Perastage\" "
        "FixtureTypeID=\"00000000-0000-0000-0000-000000000001\">"
        "<AttributeDefinitions/><Geometries/><DMXModes/></FixtureType></GDTF>");
    GdtfCanonicalizer::Options options;
    options.allowFixtureTypeIdRepair = true;
    options.stableIdSeed = "truss:300:12kg";
    GdtfCanonicalizer::Result result = GdtfCanonicalizer::CanonicalizeDescription(doc, options);
    assert(result.success);
    const char *id = doc.FirstChildElement("GDTF")
                         ->FirstChildElement("FixtureType")
                         ->Attribute("FixtureTypeID");
    assert(id);
    assert(!GdtfCanonicalizer::IsPlaceholderFixtureTypeId(id));
  }

  {
    tinyxml2::XMLDocument doc = Parse(MinimalGdtf(
        "<AttributeDefinitions/><PhysicalDescriptions/><Models/><Geometries/><DMXModes/>"));
    GdtfCanonicalizer::Result result = GdtfCanonicalizer::CanonicalizeDescription(doc);
    assert(result.success);
    assert(!result.changed);
  }

  return 0;
}
