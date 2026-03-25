/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "layer.h"
#include "support.h"

namespace fs = std::filesystem;

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

static std::unordered_map<std::string, std::string>
ReadArchiveEntries(const fs::path &archivePath) {
  wxFileInputStream input(archivePath.generic_string());
  assert(input.IsOk());
  wxZipInputStream zip(input);

  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    entries[entry->GetName().ToStdString()] = ReadCurrentZipEntry(zip);
  }
  return entries;
}

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempDir =
      fs::temp_directory_path() / "project_support_userdata_roundtrip_test";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir, ec);

  auto &cfg = ConfigManager::Get();
  cfg.Reset();

  MvrScene &scene = cfg.GetScene();
  scene.basePath = tempDir.generic_string();

  Layer layer;
  layer.uuid = "layer1";
  layer.name = "Layer 1";
  scene.layers[layer.uuid] = layer;

  Support support;
  support.uuid = "sup-text-to-scene";
  support.name = "Imported Manual Hoist";
  support.layer = layer.name;
  support.hoistDataSource = "Manual";
  support.capacityKg = 1000.0f;
  support.weightKg = 40.0f;
  support.loadKg = 275.0f;
  support.hoistFunction = "Audio";
  support.capacitySource = "Manual";
  support.weightSource = "Manual";
  support.hoistFunctionSource = "Manual";
  support.motorName = "ChainMaster D8+";
  support.motorNameSource = "Manual";
  support.motorManufacturer = "ChainMaster";
  support.motorManufacturerSource = "Manual";
  support.motorModel = "D8+";
  support.motorModelSource = "Manual";
  scene.supports[support.uuid] = support;

  const fs::path projectPath = tempDir / "support_userdata_roundtrip.pstg";
  assert(cfg.SaveProject(projectPath.string()));

  const auto projectEntries = ReadArchiveEntries(projectPath);
  const auto sceneMvrIt = projectEntries.find("scene.mvr");
  assert(sceneMvrIt != projectEntries.end());

  const std::string &sceneMvr = sceneMvrIt->second;
  wxMemoryInputStream sceneMvrStream(sceneMvr.data(), sceneMvr.size());
  wxZipInputStream mvrZip(sceneMvrStream);
  std::string sceneXml;
  std::unique_ptr<wxZipEntry> mvrEntry;
  while ((mvrEntry.reset(mvrZip.GetNextEntry())), mvrEntry) {
    if (mvrEntry->GetName().ToStdString() == "GeneralSceneDescription.xml") {
      sceneXml = ReadCurrentZipEntry(mvrZip);
      break;
    }
  }
  assert(!sceneXml.empty());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(sceneXml.c_str()) == tinyxml2::XML_SUCCESS);

  tinyxml2::XMLElement *supportNode = nullptr;
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);

  for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    std::vector<tinyxml2::XMLElement *> stack{node};
    while (!stack.empty()) {
      tinyxml2::XMLElement *current = stack.back();
      stack.pop_back();
      const char *geometryType = current->Attribute("geometryType");
      const char *uuid = current->Attribute("uuid");
      if (std::string(current->Name()) == "SceneObject" && geometryType != nullptr &&
          std::string(geometryType) == "support" && uuid != nullptr &&
          std::string(uuid) == support.uuid) {
        supportNode = current;
        break;
      }
      for (tinyxml2::XMLElement *child = current->FirstChildElement(); child;
           child = child->NextSiblingElement()) {
        stack.push_back(child);
      }
    }
    if (supportNode != nullptr)
      break;
  }

  assert(supportNode != nullptr);
  tinyxml2::XMLElement *userData = supportNode->FirstChildElement("UserData");
  assert(userData != nullptr);
  tinyxml2::XMLElement *data = userData->FirstChildElement("Data");
  assert(data != nullptr);
  tinyxml2::XMLElement *hoistInfo = data->FirstChildElement("HoistInfo");
  assert(hoistInfo != nullptr);

  tinyxml2::XMLElement *capacity = hoistInfo->FirstChildElement("Capacity");
  tinyxml2::XMLElement *load = hoistInfo->FirstChildElement("Load");
  tinyxml2::XMLElement *function = hoistInfo->FirstChildElement("RiggingPoint");
  if (function == nullptr)
    function = hoistInfo->FirstChildElement("Function");

  assert(capacity != nullptr);
  assert(load != nullptr);
  assert(function != nullptr);
  assert(capacity->GetText() != nullptr);
  assert(load->GetText() != nullptr);
  assert(function->GetText() != nullptr);

  cfg.Reset();
  assert(cfg.LoadProject(projectPath.string()));

  const auto &loadedSupports = cfg.GetScene().supports;
  assert(loadedSupports.size() == 1);
  const auto &loaded = loadedSupports.at(support.uuid);
  assert(loaded.hoistDataSource == "Manual");
  assert(loaded.capacityKg == 1000.0f);
  assert(loaded.capacityKg > 0.0f);
  assert(loaded.loadKg == 275.0f);
  assert(loaded.hoistFunction == "Audio");

  fs::remove_all(tempDir, ec);
  return 0;
}
