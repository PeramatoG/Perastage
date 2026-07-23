/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "support/gdtf_test_fixture_builder.h"

#include "../core/configmanager.h"
#include "../models/truss.h"
#include "../mvr/mvrexporter.h"
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

std::unordered_map<std::string, std::string> ReadArchiveEntries(const fs::path &archivePath) {
  wxFileInputStream input(archivePath.generic_string());
  assert(input.IsOk());
  wxZipInputStream zip(input);

  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    entries[entry->GetName().ToStdString()] = ReadCurrentZipEntry(zip);
  }
  return entries;
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

tinyxml2::XMLElement *FindFixtureType(tinyxml2::XMLDocument &doc) {
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  else
    fixtureType = doc.FirstChildElement("FixtureType");
  return fixtureType;
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  const fs::path tempDir =
      fs::temp_directory_path() /
      ("mvr_patched_gdtf_export_test_" +
       std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
  fs::create_directories(tempDir);

  const std::string sourceGdtf = MakeBaseGdtf();

  Truss truss;
  truss.uuid = "truss-override-1";
  truss.name = "Main Truss";
  truss.gdtfSpec = sourceGdtf;
  truss.gdtfMode = "Default";
  truss.lengthMm = 3200.0f;
  truss.widthMm = 420.0f;
  truss.heightMm = 380.0f;
  truss.weightKg = 215.0f;
  truss.manufacturer = "Perastage QA";
  truss.model = "Truss QA Model";
  scene.trusses[truss.uuid] = truss;

  const fs::path mvrPath = tempDir / "patched_export.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(mvrPath.string()));

  const auto mvrEntries = ReadArchiveEntries(mvrPath);
  const std::string expectedGdtfName = "Perastage_QA@Truss_QA_Model@Perastage.gdtf";
  auto patchedIt = mvrEntries.find(expectedGdtfName);
  assert(patchedIt != mvrEntries.end());
  const std::string patchedGdtfBytes = patchedIt->second;

  auto gsdIt = mvrEntries.find("GeneralSceneDescription.xml");
  assert(gsdIt != mvrEntries.end());
  assert(gsdIt->second.find("<GDTFSpec>" + expectedGdtfName + "</GDTFSpec>") !=
         std::string::npos);

  const fs::path extractedGdtfPath = tempDir / "patched_truss.gdtf";
  {
    std::ofstream out(extractedGdtfPath, std::ios::binary);
    out.write(patchedGdtfBytes.data(), static_cast<std::streamsize>(patchedGdtfBytes.size()));
  }

  const auto gdtfEntries = ReadArchiveEntries(extractedGdtfPath);
  auto itDescription = gdtfEntries.find("description.xml");
  assert(itDescription != gdtfEntries.end());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(itDescription->second.c_str(), itDescription->second.size()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *fixtureType = FindFixtureType(doc);
  assert(fixtureType != nullptr);

  const char *manufacturer = fixtureType->Attribute("Manufacturer");
  const char *name = fixtureType->Attribute("Name");
  assert(manufacturer != nullptr && std::string(manufacturer) == truss.manufacturer);
  assert(name != nullptr && std::string(name) == truss.model);

  tinyxml2::XMLElement *properties = fixtureType->FirstChildElement("PhysicalDescriptions");
  assert(properties != nullptr);
  properties = properties->FirstChildElement("Properties");
  assert(properties != nullptr);
  tinyxml2::XMLElement *weight = properties->FirstChildElement("Weight");
  assert(weight != nullptr);
  assert(std::abs(weight->FloatAttribute("Value") - truss.weightKg) < 0.001f);

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  assert(models != nullptr);
  tinyxml2::XMLElement *model = models->FirstChildElement("Model");
  assert(model != nullptr);
  assert(std::abs(model->FloatAttribute("Length") - (truss.lengthMm / 1000.0f)) < 0.001f);
  assert(std::abs(model->FloatAttribute("Width") - (truss.widthMm / 1000.0f)) < 0.001f);
  assert(std::abs(model->FloatAttribute("Height") - (truss.heightMm / 1000.0f)) < 0.001f);

  std::vector<GdtfObject> objects;
  std::string loadError;
  assert(LoadGdtf(extractedGdtfPath.string(), objects, &loadError));
  assert(loadError.empty());
  assert(!objects.empty());

  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::remove(sourceGdtf, ec);
  cfg.Reset();
  return 0;
}
