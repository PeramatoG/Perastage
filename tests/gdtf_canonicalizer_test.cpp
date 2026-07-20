#include "gdtf_canonicalizer.h"

#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

// Parses XML text into a TinyXML document for canonicalizer checks.
void ParseInto(tinyxml2::XMLDocument &doc, const std::string &xml) {
  assert(doc.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS);
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

// Writes a test GDTF archive with description.xml in a nested root folder.
void WriteNestedDescriptionArchive(const std::filesystem::path &path) {
  wxFileOutputStream output(path.string());
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  auto *entry = new wxZipEntry("Dummy 1ch/description.xml");
  entry->SetMethod(wxZIP_METHOD_DEFLATE);
  assert(zip.PutNextEntry(entry));
  const std::string description = MinimalGdtf(
      "<AttributeDefinitions/><Models/><PhysicalDescriptions/><Geometries/><DMXModes/>");
  zip.Write(description.c_str(), description.size());
  assert(zip.CloseEntry());
  assert(zip.Close());
}

// Returns whether a ZIP archive contains an entry with the requested name.
bool ArchiveContainsEntry(const std::filesystem::path &path,
                          const std::string &entryName) {
  wxFileInputStream input(path.string());
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName().ToStdString() == entryName)
      return true;
  }
  return false;
}

} // namespace

// Verifies that legacy FixtureType structure is canonicalized for export.
int main() {
  {
    tinyxml2::XMLDocument doc;
    ParseInto(doc, MinimalGdtf(
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
    tinyxml2::XMLDocument doc;
    ParseInto(doc,
              "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Truss 300\" "
              "Manufacturer=\"Perastage\" "
              "FixtureTypeID=\"00000000-0000-0000-0000-000000000001\">"
              "<AttributeDefinitions/><Geometries/><DMXModes/></FixtureType>"
              "</GDTF>");
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
    tinyxml2::XMLDocument doc;
    ParseInto(doc, MinimalGdtf(
        "<AttributeDefinitions/><PhysicalDescriptions/><Models/><Geometries/><DMXModes/>"));
    GdtfCanonicalizer::Result result = GdtfCanonicalizer::CanonicalizeDescription(doc);
    assert(result.success);
    assert(!result.changed);
  }

  {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "gdtf_canonicalizer_test";
    std::filesystem::create_directories(tempDir);
    const std::filesystem::path source = tempDir / "nested_description.gdtf";
    const std::filesystem::path dest = tempDir / "canonicalized.gdtf";
    WriteNestedDescriptionArchive(source);
    GdtfCanonicalizer::Result result =
        GdtfCanonicalizer::CanonicalizeArchive(source, dest);
    assert(result.success);
    assert(std::filesystem::exists(dest));
    assert(ArchiveContainsEntry(dest, "description.xml"));
    assert(!ArchiveContainsEntry(dest, "Dummy 1ch/description.xml"));
    std::filesystem::remove_all(tempDir);
  }

  return 0;
}
