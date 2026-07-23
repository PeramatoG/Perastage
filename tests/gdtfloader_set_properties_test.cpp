/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <tinyxml2.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "support/gdtf_test_fixture_builder.h"

#include "../viewer3d/gdtfloader.h"

namespace fs = std::filesystem;

namespace {

std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
  std::string content;
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    content.append(buffer, bytes);
  }
  return content;
}

// Writes a canonical minimal GDTF 1.2 archive for mutation tests.
std::string MakeBaseGdtf() {
  const fs::path outPath = fs::temp_directory_path() /
                           (std::string("makebasegdtf_") +
                            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                            ".gdtf");
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(outPath);
  return outPath.string();
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::string gdtfPath = MakeBaseGdtf();
  GdtfDocumentMutationRequest request;
  request.weightSet = true;
  request.weightKg = 12.345f;
  request.powerSet = true;
  request.powerW = 678.9f;
  request.revisionText =
      "Updated fixture physical properties (Weight/PowerConsumption) from Perastage";
  const GdtfDocumentMutationResult mutation =
      MutateGdtfDocumentWithResult(gdtfPath, request, "Perastage Tests");
  assert(mutation.success);
  assert(mutation.changed);
  assert(mutation.errors.empty());
  assert(mutation.atomicReplacementCompleted);

  wxFileInputStream input(gdtfPath);
  assert(input.IsOk());
  wxZipInputStream zipInput(input);

  std::unordered_set<std::string> entries;
  std::string descriptionXml;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string name = entry->GetName().ToStdString();
    entries.insert(name);
    if (name == "description.xml")
      descriptionXml = ReadCurrentZipEntry(zipInput);
  }

  assert(entries.find("description.xml") != entries.end());
  assert(!descriptionXml.empty());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(descriptionXml.c_str(), descriptionXml.size()) ==
         tinyxml2::XML_SUCCESS);

  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  assert(fixtureType != nullptr);
  fixtureType = fixtureType->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);

  tinyxml2::XMLElement *properties = fixtureType->FirstChildElement("PhysicalDescriptions");
  assert(properties != nullptr);
  properties = properties->FirstChildElement("Properties");
  assert(properties != nullptr);

  tinyxml2::XMLElement *weight = properties->FirstChildElement("Weight");
  tinyxml2::XMLElement *power = properties->FirstChildElement("PowerConsumption");
  assert(weight != nullptr);
  assert(power != nullptr);
  assert(std::string(weight->Attribute("Value")) == "12.345");
  assert(std::string(power->Attribute("Value")) == "678.900");

  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  assert(revisions != nullptr);
  tinyxml2::XMLElement *revision = revisions->FirstChildElement("Revision");
  assert(revision != nullptr);
  const char *date = revision->Attribute("Date");
  const char *text = revision->Attribute("Text");
  const char *modifiedBy = revision->Attribute("ModifiedBy");
  const char *userId = revision->Attribute("UserID");
  assert(date != nullptr && std::string(date).size() > 0);
  assert(text != nullptr && std::string(text).size() > 0);
  assert(modifiedBy != nullptr && std::string(modifiedBy) == "Perastage Tests");
  assert(userId != nullptr && std::string(userId).size() > 0);

  std::vector<GdtfObject> objects;
  std::string loadError;
  assert(LoadGdtf(gdtfPath, objects, &loadError));
  assert(loadError.empty());
  assert(!objects.empty());

  std::error_code ec;
  fs::remove(gdtfPath, ec);
  return 0;
}
