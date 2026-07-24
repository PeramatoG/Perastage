/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"

#include "../core/gdtf_canonicalizer.h"
#include "../viewer3d/gdtfloader.h"

namespace fs = std::filesystem;

namespace {

// Reads the current ZIP entry contents as bytes.
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
  tests::gdtf::BuildMinimalValidFixture()
      .WithArchiveEntry("resources/sentinel.bin", "sentinel-resource-bytes")
      .WriteArchive(outPath);
  return outPath.string();
}

// Reads all regular entries from a GDTF archive.
std::unordered_map<std::string, std::string> ReadArchiveEntries(const fs::path &archivePath) {
  wxFileInputStream input(archivePath.string());
  assert(input.IsOk());
  wxZipInputStream zipInput(input);
  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const auto logicalName =
        tests::archive::NormalizePresentedArchivePath(entry->GetName().ToStdString());
    assert(logicalName.ok);
    entries[logicalName.path] = ReadCurrentZipEntry(zipInput);
  }
  return entries;
}

// Reads an entire file as bytes for publication-preservation checks.
std::string ReadFileBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Parses a required GDTF float token and verifies exact float roundtrip semantics.
void AssertGdtfFloatRoundtrips(tinyxml2::XMLElement *element, float expected) {
  assert(element != nullptr);
  const char *value = element->Attribute("Value");
  assert(value != nullptr);
  assert(std::string(value).find(',') == std::string::npos);
  char *end = nullptr;
  const float parsed = std::strtof(value, &end);
  assert(end != value && end != nullptr && *end == '\0');
  assert(std::memcmp(&parsed, &expected, sizeof(float)) == 0);
}

// Returns an XML property node from a mutated GDTF description.
tinyxml2::XMLElement *FindPhysicalProperty(tinyxml2::XMLDocument &doc,
                                           const char *name) {
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  assert(fixtureType != nullptr);
  fixtureType = fixtureType->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);
  tinyxml2::XMLElement *properties =
      fixtureType->FirstChildElement("PhysicalDescriptions");
  assert(properties != nullptr);
  properties = properties->FirstChildElement("Properties");
  assert(properties != nullptr);
  tinyxml2::XMLElement *property = properties->FirstChildElement(name);
  assert(property != nullptr);
  return property;
}

// Reports whether a sibling mutation temporary archive remains beside the target.
bool HasSiblingMutationTempArchive(const fs::path &targetPath) {
  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(targetPath.parent_path(), ec)) {
    if (ec)
      return true;
    const std::string filename = entry.path().filename().string();
    if (filename.rfind(targetPath.filename().string() + ".tmp.", 0) == 0)
      return true;
  }
  return false;
}

// Verifies that stored ZIP central-directory paths are canonical.
void VerifyRawArchiveNames(const fs::path &archivePath,
                           const std::vector<std::string> &expectedNames) {
  std::string error;
  const std::vector<std::string> rawNames =
      tests::archive::ReadRawCentralDirectoryEntryNames(archivePath.string(), error);
  assert(error.empty());
  for (const std::string &expectedName : expectedNames) {
    assert(std::find(rawNames.begin(), rawNames.end(), expectedName) !=
           rawNames.end());
  }
  for (const std::string &rawName : rawNames) {
    assert(rawName.find('\\') == std::string::npos);
  }
}

// Verifies that an injected publication failure preserves the original archive.
void VerifyInjectedPublicationFailurePreservesOriginal() {
  const std::string gdtfPath = MakeBaseGdtf();
  const std::string before = ReadFileBytes(gdtfPath);
  GdtfDocumentMutationRequest request;
  request.weightSet = true;
  request.weightKg = 1.0f;
  GdtfDocumentMutationPublicationHooks hooks;
  hooks.beforeStage = [](const std::string &stage, std::string &error) {
    if (stage == "BeforeAtomicReplace") {
      error = "injected replace failure";
      return false;
    }
    return true;
  };
  const GdtfDocumentMutationResult mutation =
      MutateGdtfDocumentWithResult(gdtfPath, request, "Perastage Tests", &hooks);
  assert(!mutation.success);
  assert(!mutation.errors.empty());
  assert(mutation.errors.front().find("BeforeAtomicReplace") != std::string::npos);
  assert(ReadFileBytes(gdtfPath) == before);
  assert(!HasSiblingMutationTempArchive(gdtfPath));
  std::error_code ec;
  fs::remove(gdtfPath, ec);
}

// Verifies finite physical-property values serialize as shortest roundtrip floats.
void VerifyFiniteFloatSerialization(float value) {
  const std::string gdtfPath = MakeBaseGdtf();
  GdtfDocumentMutationRequest request;
  request.weightSet = true;
  request.weightKg = value;
  request.powerSet = true;
  request.powerW = value;
  const GdtfDocumentMutationResult mutation =
      MutateGdtfDocumentWithResult(gdtfPath, request, "Perastage Tests");
  assert(mutation.success);
  assert(mutation.errors.empty());
  assert(mutation.atomicReplacementCompleted);

  const auto entries = ReadArchiveEntries(gdtfPath);
  auto descriptionIt = entries.find("description.xml");
  assert(descriptionIt != entries.end());
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(descriptionIt->second.c_str(), descriptionIt->second.size()) ==
         tinyxml2::XML_SUCCESS);
  AssertGdtfFloatRoundtrips(FindPhysicalProperty(doc, "Weight"), value);
  AssertGdtfFloatRoundtrips(FindPhysicalProperty(doc, "PowerConsumption"), value);

  std::error_code ec;
  fs::remove(gdtfPath, ec);
}

// Verifies non-finite physical-property values fail before publication.
void VerifyNonFiniteFloatRejected(float value) {
  const std::string gdtfPath = MakeBaseGdtf();
  const std::string before = ReadFileBytes(gdtfPath);
  GdtfDocumentMutationRequest request;
  request.weightSet = true;
  request.weightKg = value;
  request.powerSet = true;
  request.powerW = value;
  const GdtfDocumentMutationResult mutation =
      MutateGdtfDocumentWithResult(gdtfPath, request, "Perastage Tests");
  assert(!mutation.success);
  assert(!mutation.errors.empty());
  assert(mutation.errors.front().find("non-finite") != std::string::npos);
  assert(!mutation.atomicReplacementCompleted);
  assert(ReadFileBytes(gdtfPath) == before);
  assert(!HasSiblingMutationTempArchive(gdtfPath));

  std::error_code ec;
  fs::remove(gdtfPath, ec);
}

} // namespace

// Runs the GDTF property mutation publication regression test.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  VerifyInjectedPublicationFailurePreservesOriginal();
  VerifyFiniteFloatSerialization(0.0f);
  VerifyFiniteFloatSerialization(-42.5f);
  VerifyFiniteFloatSerialization(12.345f);
  VerifyFiniteFloatSerialization(678.9f);
  VerifyFiniteFloatSerialization(1234567.0f);
  VerifyFiniteFloatSerialization(0.0001234567f);
  VerifyFiniteFloatSerialization(1234567000000.0f);
  VerifyNonFiniteFloatRejected(std::numeric_limits<float>::quiet_NaN());
  VerifyNonFiniteFloatRejected(std::numeric_limits<float>::infinity());
  VerifyNonFiniteFloatRejected(-std::numeric_limits<float>::infinity());

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

  const auto entries = ReadArchiveEntries(gdtfPath);
  auto descriptionIt = entries.find("description.xml");
  assert(descriptionIt != entries.end());
  const std::string &descriptionXml = descriptionIt->second;
  auto sentinelIt = entries.find("resources/sentinel.bin");
  assert(sentinelIt != entries.end());
  assert(sentinelIt->second == "sentinel-resource-bytes");

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
  AssertGdtfFloatRoundtrips(weight, 12.345f);
  AssertGdtfFloatRoundtrips(power, 678.9f);

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

  VerifyRawArchiveNames(gdtfPath, {"description.xml", "resources/sentinel.bin"});

  const auto validation = GdtfCanonicalizer::ValidateArchive(gdtfPath);
  assert(validation.success);

  std::error_code ec;
  fs::remove(gdtfPath, ec);
  return 0;
}
