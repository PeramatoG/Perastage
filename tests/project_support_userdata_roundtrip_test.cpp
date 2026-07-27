/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
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
#include "matrixutils.h"
#include "support.h"

namespace fs = std::filesystem;

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

// Reads all archive entries from a ZIP file.
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
  std::ofstream(tempDir / "support.3ds") << "support-model";

  Layer layer;
  layer.uuid = "11111111-2222-4333-8444-555555555555";
  layer.name = "Layer 1";
  scene.layers[layer.uuid] = layer;

  Support support;
  support.uuid = "66666666-7777-4888-8999-aaaaaaaaaaaa";
  support.name = "Imported Manual Hoist";
  support.layer = layer.name;
  support.hoistDataSource = "Manual";
  support.capacityKg = 1000.0f;
  support.weightKg = 40.0f;
  support.loadKg = 275.0f;
  support.function = "Other";
  support.hoistFunction = "Audio";
  support.capacitySource = "Manual";
  support.weightSource = "Manual";
  support.loadSource = "Manual";
  support.hoistFunctionSource = "Manual";
  support.motorName = "ChainMaster D8+";
  support.motorNameSource = "Manual";
  support.motorManufacturer = "ChainMaster";
  support.motorManufacturerSource = "Manual";
  support.motorModel = "D8+";
  support.motorModelSource = "Manual";
  support.chainLength = 1.0f;
  GeometryInstance supportGeometry;
  supportGeometry.modelFile = (tempDir / "support.3ds").generic_string();
  supportGeometry.localTransform = MatrixUtils::Identity();
  support.geometries.push_back(supportGeometry);
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
      const char *uuid = current->Attribute("uuid");
      if (std::string(current->Name()) == "Support" && uuid != nullptr &&
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
  assert(supportNode->FirstChildElement("UserData") == nullptr);
  tinyxml2::XMLElement *userData = root->FirstChildElement("UserData");
  assert(userData != nullptr);
  tinyxml2::XMLElement *data = userData->FirstChildElement("Data");
  assert(data != nullptr);
  assert(std::string(data->Attribute("provider")) == "Perastage");
  assert(std::string(data->Attribute("ver")) == "1.0");
  tinyxml2::XMLElement *hoistInfoMap =
      data->FirstChildElement("HoistInfoMap");
  assert(hoistInfoMap != nullptr);
  tinyxml2::XMLElement *hoistInfo =
      hoistInfoMap->FirstChildElement("HoistInfo");
  assert(hoistInfo != nullptr);
  assert(std::string(hoistInfo->Attribute("uuid")) == support.uuid);
  assert(hoistInfo->NextSiblingElement("HoistInfo") == nullptr);

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
  assert(loaded.weightKg == 40.0f);
  assert(loaded.loadKg == 275.0f);
  assert(loaded.hoistFunction == "Audio");
  assert(loaded.motorName == "ChainMaster D8+");
  assert(loaded.motorManufacturer == "ChainMaster");
  assert(loaded.motorModel == "D8+");
  assert(loaded.capacitySource == "Manual");
  assert(loaded.weightSource == "Manual");
  assert(loaded.hoistFunctionSource == "Manual");
  const Support loadedCopy = loaded;

  const fs::path secondProjectPath =
      tempDir / "support_userdata_roundtrip_second.pstg";
  assert(cfg.SaveProject(secondProjectPath.string()));
  cfg.Reset();
  assert(cfg.LoadProject(secondProjectPath.string()));
  const auto &secondLoaded = cfg.GetScene().supports.at(support.uuid);
  assert(secondLoaded.capacityKg == loadedCopy.capacityKg);
  assert(secondLoaded.weightKg == loadedCopy.weightKg);
  assert(secondLoaded.loadKg == loadedCopy.loadKg);
  assert(secondLoaded.hoistFunction == loadedCopy.hoistFunction);
  assert(secondLoaded.motorName == loadedCopy.motorName);

  fs::remove_all(tempDir, ec);
  return 0;
}
