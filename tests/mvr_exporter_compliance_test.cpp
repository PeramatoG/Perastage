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

static std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
  std::string content;
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    content.append(buffer, bytes);
  }
  return content;
}

static std::unordered_map<std::string, std::string>
ReadArchiveTextEntries(const fs::path &archivePath) {
  wxFileInputStream input(archivePath.generic_string());
  assert(input.IsOk());
  wxZipInputStream zip(input);

  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string name = entry->GetName().ToStdString();
    assert(entries.find(name) == entries.end());
    entries[name] = ReadCurrentZipEntry(zip);
  }
  return entries;
}

static tinyxml2::XMLElement *FindFixtureType(tinyxml2::XMLDocument &doc) {
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  else
    fixtureType = doc.FirstChildElement("FixtureType");
  return fixtureType;
}

static bool GdtfHasRenderable3DModel(const fs::path &gdtfPath) {
  const auto gdtfEntries = ReadArchiveTextEntries(gdtfPath);
  auto descriptionIt = gdtfEntries.find("description.xml");
  assert(descriptionIt != gdtfEntries.end());

  tinyxml2::XMLDocument gdtfDoc;
  assert(gdtfDoc.Parse(descriptionIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *fixtureType = FindFixtureType(gdtfDoc);
  assert(fixtureType != nullptr);

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (models == nullptr)
    return false;

  std::unordered_set<std::string> modelNames;
  for (tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    const char *name = model->Attribute("Name");
    if (name != nullptr && std::string(name).size() > 0)
      modelNames.insert(name);
  }
  if (modelNames.empty())
    return false;

  tinyxml2::XMLElement *geometries = fixtureType->FirstChildElement("Geometries");
  if (!geometries)
    return false;

  std::vector<tinyxml2::XMLElement *> stack;
  for (tinyxml2::XMLElement *child = geometries->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    stack.push_back(child);
  }

  while (!stack.empty()) {
    tinyxml2::XMLElement *cur = stack.back();
    stack.pop_back();

    const char *modelRef = cur->Attribute("Model");
    if (modelRef != nullptr && modelNames.count(modelRef) == 1)
      return true;

    for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      stack.push_back(child);
    }
  }

  return false;
}

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
  fs::create_directories(tempDir / "C");

  std::ofstream(tempDir / "A" / "Same.gdtf") << "A";
  std::ofstream(tempDir / "B" / "Same.gdtf") << "B";
  std::ofstream(tempDir / "mesh.3ds") << "mesh";
  std::ofstream(tempDir / "models" / "truss_model.3ds") << "truss";

  const std::string longToken(320, "x"[0]);
  const fs::path longNamedGdtf = tempDir / ("VeryLongGeneratedName_" + longToken + ".gdtf");
  const fs::path duplicateLongNamedGdtf = tempDir / "C" / ("VeryLongGeneratedName_" + longToken + ".gdtf");
  std::ofstream(longNamedGdtf) << "LONG";
  std::ofstream(duplicateLongNamedGdtf) << "LONG2";

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

  Fixture fLong;
  fLong.uuid = "fx-long";
  fLong.instanceName = "Long Name Fixture";
  fLong.gdtfSpec = longNamedGdtf.generic_string();
  fLong.address = "7.1";
  scene.fixtures[fLong.uuid] = fLong;

  Fixture fLongDup;
  fLongDup.uuid = "fx-long-dup";
  fLongDup.instanceName = "Long Name Fixture Duplicate";
  fLongDup.gdtfSpec = duplicateLongNamedGdtf.generic_string();
  fLongDup.address = "8.1";
  scene.fixtures[fLongDup.uuid] = fLongDup;

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
  sup.motorName = "ChainMaster D8+";
  sup.dummyPreset = "D8+ 1000kg";
  sup.hoistDataSource = "Manual";
  sup.hoistFunction = "Lighting";
  sup.capacityKg = 1000.0f;
  sup.weightKg = 42.0f;
  scene.supports[sup.uuid] = sup;

  MvrExporter exporter;
  fs::path mvrPath = tempDir / "Test1.mvr";
  assert(exporter.ExportToFile(mvrPath.generic_string()));

  const auto mvrGeometryEntries = ReadArchiveTextEntries(mvrPath);
  std::unordered_set<std::string> entries;
  for (const auto &[entryName, _] : mvrGeometryEntries)
    entries.insert(entryName);
  auto xmlIt = mvrGeometryEntries.find("GeneralSceneDescription.xml");
  assert(xmlIt != mvrGeometryEntries.end());
  std::string xml = xmlIt->second;

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
  bool sawAddress3073 = false;
  bool sawAddress3585 = false;
  bool sawNonNumericTrussNameFixtureIdConsistency = false;
  int mvrGeometryTrussCount = 0;
  int mvrGeometryTrussesWithGeometry3d = 0;
  int mvrGeometryTrussesWithRenderableGdtf = 0;
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

          if (std::string(cur->Name()) == "Support") {
            auto *ud = cur->FirstChildElement("UserData");
            assert(ud != nullptr);
            auto *data = ud->FirstChildElement("Data");
            assert(data != nullptr);
            auto *info = data->FirstChildElement("HoistInfo");
            assert(info != nullptr);

            auto *motorNode = info->FirstChildElement("MotorName");
            assert(motorNode != nullptr && motorNode->GetText() != nullptr);
            assert(std::string(motorNode->GetText()) == "ChainMaster D8+");

            auto *dummyNode = info->FirstChildElement("DummyPreset");
            assert(dummyNode != nullptr && dummyNode->GetText() != nullptr);
            assert(std::string(dummyNode->GetText()) == "D8+ 1000kg");

            auto *sourceNode = info->FirstChildElement("DataSource");
            assert(sourceNode != nullptr && sourceNode->GetText() != nullptr);
            assert(std::string(sourceNode->GetText()) == "Manual");

            auto *legacyNode = info->FirstChildElement("RiggingPoint");
            assert(legacyNode == nullptr);
          }

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
            if (absoluteAddress == ComputeAbsoluteDmx(7, 1))
              sawAddress3073 = true;
            if (absoluteAddress == ComputeAbsoluteDmx(8, 1))
              sawAddress3585 = true;
            ++fixtureAddressCount;
          }

          if (std::string(cur->Name()) == "Truss") {
            ++mvrGeometryTrussCount;
            auto *gdtfSpec = cur->FirstChildElement("GDTFSpec");
            assert(gdtfSpec != nullptr && gdtfSpec->GetText() != nullptr);
            std::string trussGdtfSpec = gdtfSpec->GetText();
            auto *geometries = cur->FirstChildElement("Geometries");
            assert(geometries != nullptr);
            assert(geometries->FirstChildElement("Geometry3D") != nullptr);
            ++mvrGeometryTrussesWithGeometry3d;

            assert(mvrGeometryEntries.count(trussGdtfSpec) == 1);
            const fs::path trussGdtfPath = tempDir / trussGdtfSpec;
            std::ofstream gdtfOut(trussGdtfPath, std::ios::binary);
            assert(gdtfOut.is_open());
            gdtfOut << mvrGeometryEntries.at(trussGdtfSpec);
            gdtfOut.close();
            const bool hasRenderableGdtf = GdtfHasRenderable3DModel(trussGdtfPath);
            assert(hasRenderableGdtf);
            if (hasRenderableGdtf)
              ++mvrGeometryTrussesWithRenderableGdtf;

            const char *trussUuid = cur->Attribute("uuid");
            if (trussUuid != nullptr && std::string(trussUuid) == trNonNumeric.uuid) {
              sawNonNumericTrussNameFixtureIdConsistency =
                  fixtureIdText == std::to_string(value);
            }
          }

          if (auto *gdtf = cur->FirstChildElement("GDTFSpec"); gdtf && gdtf->GetText()) {
            std::string spec = gdtf->GetText();
            const fs::path specPath(spec);
            assert(specPath.filename().generic_string().size() <= 120);
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
  assert(fixtureAddressCount == 5);
  assert(sawAddress1);
  assert(sawAddress1025);
  assert(sawAddress2681);
  assert(sawAddress3073);
  assert(sawAddress3585);
  assert(sawNonNumericTrussNameFixtureIdConsistency);
  assert(mvrGeometryTrussCount == static_cast<int>(scene.trusses.size()));
  assert(mvrGeometryTrussesWithGeometry3d == mvrGeometryTrussCount);
  assert(mvrGeometryTrussesWithRenderableGdtf == mvrGeometryTrussCount);

  for (const auto &name : entries) {
    assert(name.rfind("gdtf/", 0) != 0);
  }


  cfg.SetFloat("mvr_truss_geometry_authority", 1.0f);
  fs::path mvrPathGdtfAuthority = tempDir / "Test2_GdtfAuthority.mvr";
  assert(exporter.ExportToFile(mvrPathGdtfAuthority.generic_string()));

  const auto gdtfAuthorityEntries = ReadArchiveTextEntries(mvrPathGdtfAuthority);
  auto xmlGdtfAuthorityIt = gdtfAuthorityEntries.find("GeneralSceneDescription.xml");
  assert(xmlGdtfAuthorityIt != gdtfAuthorityEntries.end());
  std::string xmlGdtfAuthority = xmlGdtfAuthorityIt->second;

  assert(!xmlGdtfAuthority.empty());
  tinyxml2::XMLDocument docGdtfAuthority;
  assert(docGdtfAuthority.Parse(xmlGdtfAuthority.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *rootGdtfAuthority = docGdtfAuthority.FirstChildElement("GeneralSceneDescription");
  assert(rootGdtfAuthority != nullptr);

  bool sawTrussWithGdtfSpec = false;
  int gdtfAuthorityTrussCount = 0;
  int gdtfAuthorityTrussesWithGeometry3d = 0;
  int gdtfAuthorityTrussesWithRenderableGdtf = 0;
  for (tinyxml2::XMLElement *node = rootGdtfAuthority->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    std::vector<tinyxml2::XMLElement *> stack{node};
    while (!stack.empty()) {
      tinyxml2::XMLElement *cur = stack.back();
      stack.pop_back();
      if (std::string(cur->Name()) == "Truss") {
        ++gdtfAuthorityTrussCount;
        auto *gdtfSpec = cur->FirstChildElement("GDTFSpec");
        assert(gdtfSpec != nullptr && gdtfSpec->GetText() != nullptr);
        const std::string trussGdtfSpec = gdtfSpec->GetText();
        sawTrussWithGdtfSpec = true;
        auto *geometries = cur->FirstChildElement("Geometries");
        assert(geometries == nullptr || geometries->FirstChildElement("Geometry3D") == nullptr);
        if (geometries != nullptr && geometries->FirstChildElement("Geometry3D") != nullptr)
          ++gdtfAuthorityTrussesWithGeometry3d;

        assert(gdtfAuthorityEntries.count(trussGdtfSpec) == 1);
        const fs::path trussGdtfPath = tempDir / ("gdtf_mode_" + trussGdtfSpec);
        std::ofstream gdtfOut(trussGdtfPath, std::ios::binary);
        assert(gdtfOut.is_open());
        gdtfOut << gdtfAuthorityEntries.at(trussGdtfSpec);
        gdtfOut.close();

        const bool hasRenderableGdtf = GdtfHasRenderable3DModel(trussGdtfPath);
        assert(hasRenderableGdtf);
        if (hasRenderableGdtf)
          ++gdtfAuthorityTrussesWithRenderableGdtf;
      }
      for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
           child = child->NextSiblingElement()) {
        stack.push_back(child);
      }
    }
  }
  assert(sawTrussWithGdtfSpec);
  assert(gdtfAuthorityTrussCount == static_cast<int>(scene.trusses.size()));
  assert(gdtfAuthorityTrussesWithGeometry3d == 0);
  assert(gdtfAuthorityTrussesWithRenderableGdtf == gdtfAuthorityTrussCount);
  fs::remove_all(tempDir);
  return 0;
}
