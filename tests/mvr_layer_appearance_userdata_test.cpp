/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "layer.h"
#include "mvrexporter.h"
#include "mvrimporter.h"

// Reads the current ZIP entry into a string.
static std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
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

// Reads GeneralSceneDescription.xml from an MVR ZIP archive.
static std::string ReadSceneXml(const std::filesystem::path &archivePath) {
  wxFileInputStream input(archivePath.generic_string());
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName().ToStdString() == "GeneralSceneDescription.xml")
      return ReadCurrentZipEntry(zip);
    ReadCurrentZipEntry(zip);
  }
  return {};
}

// Writes a minimal MVR archive containing the supplied scene XML.
static void WriteMvrWithSceneXml(const std::filesystem::path &archivePath,
                                 const std::string &xml) {
  wxFileOutputStream output(archivePath.generic_string());
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  assert(zip.PutNextEntry("GeneralSceneDescription.xml"));
  zip.Write(xml.data(), xml.size());
  zip.CloseEntry();
  zip.Close();
}

// Finds the first exported Perastage layer appearance entry for the requested layer name.
static tinyxml2::XMLElement *FindLayerAppearance(tinyxml2::XMLDocument &doc,
                                                 const char *layerName) {
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
  assert(userData != nullptr);
  tinyxml2::XMLElement *data = userData->FirstChildElement("Data");
  assert(data != nullptr);
  assert(std::string(data->Attribute("provider")) == "Perastage");
  tinyxml2::XMLElement *map = data->FirstChildElement("LayerAppearanceMap");
  assert(map != nullptr);
  for (tinyxml2::XMLElement *layer = map->FirstChildElement("Layer"); layer;
       layer = layer->NextSiblingElement("Layer")) {
    const char *name = layer->Attribute("name");
    if (name && std::string(name) == layerName)
      return layer;
  }
  return nullptr;
}

// Verifies the exported scene XML stores layer colors only in root Perastage UserData.
static void VerifyExportStructure(const std::filesystem::path &archivePath,
                                  const char *layerName,
                                  const char *expectedColor) {
  const std::string xml = ReadSceneXml(archivePath);
  assert(!xml.empty());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  tinyxml2::XMLElement *scene = root->FirstChildElement("Scene");
  assert(scene != nullptr);
  assert(scene->FirstChildElement("UserData") == nullptr);
  tinyxml2::XMLElement *layers = scene->FirstChildElement("Layers");
  assert(layers != nullptr);
  for (tinyxml2::XMLElement *layer = layers->FirstChildElement("Layer"); layer;
       layer = layer->NextSiblingElement("Layer")) {
    assert(layer->FirstChildElement("Color") == nullptr);
  }

  tinyxml2::XMLElement *appearance = FindLayerAppearance(doc, layerName);
  assert(appearance != nullptr);
  assert(std::string(appearance->Attribute("color")) == expectedColor);
}

// Exports and imports a colored layer using the Perastage UserData appearance map.
static void TestLayerAppearanceRoundtrip(const std::filesystem::path &tempDir) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  Layer layer;
  layer.uuid = "11111111-1111-4111-8111-111111111111";
  layer.name = "Color Layer";
  layer.color = "#3366AA";
  scene.layers[layer.uuid] = layer;

  const std::filesystem::path archivePath = tempDir / "layer-appearance.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(archivePath.generic_string()));
  VerifyExportStructure(archivePath, "Color Layer", "#3366AA");

  MvrScene imported;
  MvrImporter importer;
  assert(importer.ImportSceneFromFile(archivePath.generic_string(), imported, false, false));
  const auto importedLayer = imported.layers.find(layer.uuid);
  assert(importedLayer != imported.layers.end());
  assert(importedLayer->second.color == "#3366AA");
}

// Imports legacy Layer/Color data and reexports it using only Perastage UserData.
static void TestLegacyLayerColorCompatibility(const std::filesystem::path &tempDir) {
  const std::filesystem::path legacyPath = tempDir / "legacy-layer-color.mvr";
  const std::string legacyXml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Perastage\" providerVersion=\"test\">"
      "<Scene><Layers><Layer uuid=\"22222222-2222-4222-8222-222222222222\" name=\"Legacy Layer\">"
      "<Color>#445566</Color><ChildList/></Layer></Layers></Scene>"
      "</GeneralSceneDescription>";
  WriteMvrWithSceneXml(legacyPath, legacyXml);

  MvrScene imported;
  MvrImporter importer;
  assert(importer.ImportSceneFromFile(legacyPath.generic_string(), imported, false, false));
  const auto importedLayer = imported.layers.find("22222222-2222-4222-8222-222222222222");
  assert(importedLayer != imported.layers.end());
  assert(importedLayer->second.color == "#445566");

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  cfg.GetScene() = imported;
  const std::filesystem::path reexportPath = tempDir / "legacy-reexport.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(reexportPath.generic_string()));
  VerifyExportStructure(reexportPath, "Legacy Layer", "#445566");
}

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "mvr_layer_appearance_userdata_test";
  std::filesystem::create_directories(tempDir);

  TestLayerAppearanceRoundtrip(tempDir);
  TestLegacyLayerColorCompatibility(tempDir);
  return 0;
}
