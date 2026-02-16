/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "mvrexporter.h"
#include "fixture.h"
#include "truss.h"
#include "support.h"

namespace fs = std::filesystem;

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  fs::path tempDir = fs::temp_directory_path() / "mvr_exporter_compliance_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir / "A");
  fs::create_directories(tempDir / "B");
  fs::create_directories(tempDir / "models");

  std::ofstream(tempDir / "A" / "Same.gdtf") << "A";
  std::ofstream(tempDir / "B" / "Same.gdtf") << "B";
  std::ofstream(tempDir / "mesh.3ds") << "mesh";
  std::ofstream(tempDir / "models" / "truss_model.3ds") << "truss";

  scene.basePath = tempDir.generic_string();
  scene.provider.clear();
  scene.providerVersion.clear();
  scene.versionMajor = 1;
  scene.versionMinor = 6;

  Fixture f1;
  f1.uuid = "fx-1";
  f1.instanceName = "Front Key";
  f1.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  f1.fixtureId = 0;
  f1.fixtureIdNumeric = 0;
  f1.unitNumber = 101;
  f1.address = "1.1";
  scene.fixtures[f1.uuid] = f1;

  Fixture f2;
  f2.uuid = "fx-2";
  f2.instanceName = "Back Key";
  f2.gdtfSpec = (tempDir / "B" / "Same.gdtf").generic_string();
  f2.fixtureId = 0;
  f2.fixtureIdNumeric = 0;
  f2.unitNumber = 0;
  f2.address = "3.1";
  scene.fixtures[f2.uuid] = f2;

  Fixture f3;
  f3.uuid = "fx-3";
  f3.instanceName = "Floor Wash";
  f3.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  f3.address = "6.121";
  scene.fixtures[f3.uuid] = f3;

  Truss tr;
  tr.uuid = "tr-1";
  tr.name = "Main Truss";
  tr.symbolFile = "mesh.3ds";
  tr.modelFile = (tempDir / "models" / "truss_model.3ds").string();
  scene.trusses[tr.uuid] = tr;

  Truss trNonNumeric;
  trNonNumeric.uuid = "tr-2";
  trNonNumeric.name = "TRUSS 2M";
  trNonNumeric.symbolFile = "mesh.3ds";
  trNonNumeric.modelFile = (tempDir / "models" / "truss_model.3ds").string();
  scene.trusses[trNonNumeric.uuid] = trNonNumeric;

  Support sup;
  sup.uuid = "sup-1";
  sup.name = "Hoist 1";
  sup.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  scene.supports[sup.uuid] = sup;

  MvrExporter exporter;
  fs::path mvrPath = tempDir / "Test1.mvr";
  assert(exporter.ExportToFile(mvrPath.generic_string()));

  wxFileInputStream input(mvrPath.generic_string());
  assert(input.IsOk());
  wxZipInputStream zip(input);

  std::unordered_set<std::string> entries;
  std::string xml;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string name = entry->GetName().ToStdString();
    assert(entries.insert(name).second);

    if (name == "GeneralSceneDescription.xml") {
      char buffer[4096];
      while (true) {
        zip.Read(buffer, sizeof(buffer));
        size_t bytes = zip.LastRead();
        if (bytes == 0)
          break;
        xml.append(buffer, bytes);
      }
    }
  }

  assert(!xml.empty());
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  assert(root->IntAttribute("verMajor") == 1);
  assert(root->IntAttribute("verMinor") == 6);
  assert(std::string(root->Attribute("provider")) == "Perastage");
  assert(std::string(root->Attribute("providerVersion")) == "1.0");

  std::unordered_set<int> numericIds;
  std::unordered_map<std::string, int> gdtfCount;

  int fixtureAddressCount = 0;
  bool sawAddress1 = false;
  bool sawAddress1025 = false;
  bool sawAddress2681 = false;
  bool sawSafeModelFile = false;
  bool sawNonNumericTrussNameFixtureIdConsistency = false;
  for (const char *tagName : {"Fixture", "Truss", "Support"}) {
    for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
         node = node->NextSiblingElement()) {
      std::vector<tinyxml2::XMLElement *> stack{node};
      while (!stack.empty()) {
        tinyxml2::XMLElement *cur = stack.back();
        stack.pop_back();
        if (std::string(cur->Name()) == tagName) {
          auto *idNode = cur->FirstChildElement("FixtureID");
          auto *numNode = cur->FirstChildElement("FixtureIDNumeric");
          assert(idNode && idNode->GetText() && std::string(idNode->GetText()).size() > 0);
          assert(numNode && numNode->GetText());
          std::string fixtureIdText = idNode->GetText();
          assert(std::all_of(fixtureIdText.begin(), fixtureIdText.end(),
                             [](unsigned char c) { return std::isdigit(c) != 0; }));
          int value = std::stoi(numNode->GetText());
          assert(value > 0);
          assert(fixtureIdText == std::to_string(value));
          assert(numericIds.insert(value).second);

          if (std::string(cur->Name()) == "Fixture") {
            const char *uuidAttr = cur->Attribute("uuid");
            const char *nameAttr = cur->Attribute("name");
            assert(uuidAttr != nullptr);
            assert(nameAttr != nullptr);
            std::string fixtureUuid = uuidAttr;
            std::string fixtureNodeName = nameAttr;
            assert(fixtureNodeName == "Fixture_" + fixtureUuid);

            auto *unitNode = cur->FirstChildElement("UnitNumber");
            if (unitNode) {
              assert(unitNode->GetText() != nullptr);
              int unitValue = std::stoi(unitNode->GetText());
              assert(unitValue != value);
            }

            auto *ud = cur->FirstChildElement("UserData");
            assert(ud != nullptr);
            auto *data = ud->FirstChildElement("Data");
            assert(data != nullptr);
            auto *info = data->FirstChildElement("FixtureInfo");
            assert(info != nullptr);
            const char *metaUuid = info->Attribute("uuid");
            assert(metaUuid != nullptr);
            assert(std::string(metaUuid) == fixtureUuid);
            auto *stableIdNode = info->FirstChildElement("StableId");
            assert(stableIdNode != nullptr && stableIdNode->GetText() != nullptr);
            assert(std::string(stableIdNode->GetText()) == fixtureUuid);
            auto *scriptNode = info->FirstChildElement("Script");
            assert(scriptNode != nullptr && scriptNode->GetText() != nullptr);
            assert(std::string(scriptNode->GetText()) == fixtureNodeName);

            auto *addresses = cur->FirstChildElement("Addresses");
            assert(addresses != nullptr);
            auto *addr = addresses->FirstChildElement("Address");
            assert(addr != nullptr);
            assert(addr->IntAttribute("break", -1) == 0);
            assert(addr->GetText() != nullptr);
            const std::string addressText = addr->GetText();
            assert(!addressText.empty());
            assert(std::all_of(addressText.begin(), addressText.end(),
                               [](unsigned char c) { return std::isdigit(c) != 0; }));
            const int absoluteAddress = std::stoi(addressText);
            if (absoluteAddress == ComputeAbsoluteDmx(1, 1))
              sawAddress1 = true;
            if (absoluteAddress == ComputeAbsoluteDmx(3, 1))
              sawAddress1025 = true;
            if (absoluteAddress == ComputeAbsoluteDmx(6, 121))
              sawAddress2681 = true;
            ++fixtureAddressCount;
          }

          if (std::string(cur->Name()) == "Truss") {
            auto *userData = cur->FirstChildElement("UserData");
            assert(userData != nullptr);
            auto *data = userData->FirstChildElement("Data");
            assert(data != nullptr);
            auto *trussInfo = data->FirstChildElement("TrussInfo");
            assert(trussInfo != nullptr);
            auto *modelFile = trussInfo->FirstChildElement("ModelFile");
            assert(modelFile != nullptr);
            assert(modelFile->GetText() != nullptr);

            const std::string modelFileText = modelFile->GetText();
            assert(!modelFileText.empty());
            assert(modelFileText.find(':') == std::string::npos);
            assert(modelFileText.front() != '/');
            assert(modelFileText.front() != '\\');
            sawSafeModelFile = true;

            const char *trussUuid = cur->Attribute("uuid");
            if (trussUuid != nullptr && std::string(trussUuid) == trNonNumeric.uuid) {
              sawNonNumericTrussNameFixtureIdConsistency =
                  fixtureIdText == std::to_string(value);
            }
          }

          if (auto *gdtf = cur->FirstChildElement("GDTFSpec"); gdtf && gdtf->GetText()) {
            std::string spec = gdtf->GetText();
            assert(spec.find(':') == std::string::npos);
            assert(spec.find('\\') == std::string::npos);
            assert(spec.find('/') == std::string::npos);
            assert(!spec.empty() && spec.front() != '/');
            assert(entries.count(spec) == 1);
            ++gdtfCount[spec];
          }
        }
        for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
          stack.push_back(child);
        }
      }
    }
  }

  assert(gdtfCount.size() >= 2);
  assert(fixtureAddressCount == 3);
  assert(sawAddress1);
  assert(sawAddress1025);
  assert(sawAddress2681);
  assert(sawSafeModelFile);
  assert(sawNonNumericTrussNameFixtureIdConsistency);

  for (const auto &name : entries) {
    assert(name.rfind("gdtf/", 0) != 0);
  }
  fs::remove_all(tempDir);
  return 0;
}
