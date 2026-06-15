/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixture.h"
#include "layer.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "matrixutils.h"
#include "projectutils.h"
#include "sceneobject.h"
#include "support.h"

// Reads the current ZIP entry into a string.
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

// Reads all text entries from an MVR ZIP archive.
static std::unordered_map<std::string, std::string>
ReadArchiveTextEntries(const std::filesystem::path &archivePath) {
  wxFileInputStream input(archivePath.generic_string());
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry)
    entries[entry->GetName().ToStdString()] = ReadCurrentZipEntry(zip);
  return entries;
}

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();

  Layer layer;
  layer.uuid = "layer1";
  layer.name = "Layer1";
  scene.layers[layer.uuid] = layer;

  std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "mvr_support_userdata_roundtrip";
  std::filesystem::create_directories(tempDir);
  std::ofstream(tempDir / "fixture.gdtf") << "fixture";
  std::ofstream(tempDir / "support.3ds") << "support-model";
  scene.basePath = tempDir.string();

  auto makeSupportGeometry = [&]() {
    GeometryInstance geometry;
    geometry.modelFile = (tempDir / "support.3ds").generic_string();
    geometry.localTransform = MatrixUtils::Identity();
    return geometry;
  };

  Fixture motorFixture;
  motorFixture.uuid = "fx-motor";
  motorFixture.instanceName = "Motor Fixture";
  motorFixture.layer = layer.name;
  motorFixture.typeName = "MotorType";
  motorFixture.gdtfSpec = "fixture.gdtf";
  motorFixture.gdtfMode = "ChainMode";
  motorFixture.category = "Spot";
  motorFixture.categorySource = "Manual";
  scene.fixtures[motorFixture.uuid] = motorFixture;

  Support linked;
  linked.uuid = "sup-linked";
  linked.name = "Linked";
  linked.layer = layer.name;
  linked.chainLength = 1.0f;
  linked.geometries.push_back(makeSupportGeometry());
  linked.gdtfSpec = "fixture.gdtf";
  linked.gdtfMode = "ChainMode";
  linked.motorFixtureUuid = motorFixture.uuid;
  linked.motorName = "CM Lodestar";
  linked.useMotorDefaults = false;
  linked.hoistDataSource = "Inherited";
  scene.supports[linked.uuid] = linked;

  Support dummy;
  dummy.uuid = "sup-dummy";
  dummy.name = "Dummy";
  dummy.layer = layer.name;
  dummy.chainLength = 1.0f;
  dummy.geometries.push_back(makeSupportGeometry());
  dummy.dummyProfileId = "d8plus_1000kg";
  dummy.hoistFunction = "Video";
  scene.supports[dummy.uuid] = dummy;

  Support inheritedDefaults;
  inheritedDefaults.uuid = "sup-inherited-defaults";
  inheritedDefaults.name = "Inherited Defaults";
  inheritedDefaults.layer = layer.name;
  inheritedDefaults.chainLength = 1.0f;
  inheritedDefaults.geometries.push_back(makeSupportGeometry());
  inheritedDefaults.motorFixtureUuid = motorFixture.uuid;
  inheritedDefaults.dummyProfileId = "d8plus_1000kg";
  inheritedDefaults.hoistDataSource = "Inherited";
  inheritedDefaults.useMotorDefaults = true;
  HoistPresetDefaults inheritedPreset;
  inheritedPreset.motorManufacturer = "Perastage";
  inheritedPreset.capacityKg = 1000.0f;
  inheritedPreset.weightKg = 40.0f;
  inheritedPreset.hoistFunction = "Lighting";

  const auto effectiveInherited = ResolveEffectiveSupportData(
      inheritedDefaults, inheritedPreset, BuildHoistFixtureDefaults(motorFixture));
  inheritedDefaults.motorManufacturer = effectiveInherited.motorManufacturer;
  inheritedDefaults.motorModel = effectiveInherited.motorModel;
  scene.supports[inheritedDefaults.uuid] = inheritedDefaults;

  Support manual;
  manual.uuid = "sup-manual";
  manual.name = "Manual";
  manual.layer = layer.name;
  manual.chainLength = 1.0f;
  manual.geometries.push_back(makeSupportGeometry());
  manual.hoistDataSource = "Manual";
  manual.capacityKg = 1250.0f;
  manual.weightKg = 61.0f;
  manual.loadKg = 410.0f;
  manual.hoistFunction = "Audio";
  manual.motorName = "ChainMaster D8+";
  manual.motorNameSource = "Manual";
  manual.motorManufacturer = "ChainMaster";
  manual.motorManufacturerSource = "Manual";
  manual.motorModel = "D8+";
  manual.motorModelSource = "Manual";
  manual.capacitySource = "Manual";
  manual.weightSource = "Manual";
  manual.hoistFunctionSource = "Manual";
  scene.supports[manual.uuid] = manual;

  Support logicalOnly;
  logicalOnly.uuid = "sup-logical-only";
  logicalOnly.name = "Logical Only";
  logicalOnly.layer = layer.name;
  logicalOnly.chainLength = 0.0f;
  scene.supports[logicalOnly.uuid] = logicalOnly;

  SceneObject emptySceneObject;
  emptySceneObject.uuid = "obj-empty";
  emptySceneObject.name = "Empty SceneObject";
  scene.sceneObjects[emptySceneObject.uuid] = emptySceneObject;

  const std::filesystem::path mvrPath = tempDir / "support_roundtrip.mvr";
  MvrExporter exporter;
  assert(exporter.ExportToFile(mvrPath.string()));

  const auto entries = ReadArchiveTextEntries(mvrPath);
  const auto xmlIt = entries.find("GeneralSceneDescription.xml");
  assert(xmlIt != entries.end());
  tinyxml2::XMLDocument xml;
  assert(xml.Parse(xmlIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = xml.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  bool foundLinkedSupport = false;
  bool foundLogicalOnlySupport = false;
  bool foundEmptySceneObjectPlaceholder = false;
  bool foundInvalidSceneObject = false;
  std::vector<tinyxml2::XMLElement *> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    tinyxml2::XMLElement *current = stack.back();
    stack.pop_back();
    if (std::string(current->Name()) == "Support" &&
        current->Attribute("uuid") && std::string(current->Attribute("uuid")) == "sup-linked") {
      foundLinkedSupport = true;
      assert(std::string(current->Attribute("name")) == "Linked");
      assert(current->FirstChildElement("Matrix") != nullptr);
      assert(current->FirstChildElement("Geometries") != nullptr);
      assert(current->FirstChildElement("GDTFSpec") != nullptr);
      assert(current->FirstChildElement("GDTFMode") != nullptr);
    }
    if (std::string(current->Name()) == "Support" &&
        current->Attribute("uuid") &&
        std::string(current->Attribute("uuid")) == "sup-logical-only") {
      foundLogicalOnlySupport = true;
      tinyxml2::XMLElement *geometries = current->FirstChildElement("Geometries");
      assert(geometries != nullptr);
      assert(geometries->FirstChildElement() == nullptr);
      assert(current->FirstChildElement("ChainLength") != nullptr);
    }
    if (std::string(current->Name()) == "SceneObject" &&
        current->Attribute("uuid") &&
        std::string(current->Attribute("uuid")) == "obj-empty") {
      foundEmptySceneObjectPlaceholder = true;
      tinyxml2::XMLElement *geometries = current->FirstChildElement("Geometries");
      assert(geometries != nullptr);
      tinyxml2::XMLElement *geometry3d = geometries->FirstChildElement("Geometry3D");
      assert(geometry3d != nullptr);
      tinyxml2::XMLElement *matrixNode = geometry3d->FirstChildElement("Matrix");
      assert(matrixNode != nullptr && matrixNode->GetText() != nullptr);
      Matrix placeholderMatrix;
      assert(MatrixUtils::ParseMatrix(matrixNode->GetText(), placeholderMatrix));
      assert(std::abs(placeholderMatrix.u[0] - 0.1f) < 0.001f);
      assert(std::abs(placeholderMatrix.v[1] - 0.1f) < 0.001f);
      assert(std::abs(placeholderMatrix.w[2] - 0.1f) < 0.001f);
    }
    if (std::string(current->Name()) == "SceneObject" &&
        current->FirstChildElement("Geometries") == nullptr)
      foundInvalidSceneObject = true;
    for (tinyxml2::XMLElement *child = current->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }
  assert(foundLinkedSupport);
  assert(foundLogicalOnlySupport);
  assert(foundEmptySceneObjectPlaceholder);
  assert(!foundInvalidSceneObject);

  cfg.Reset();
  MvrImporter importer;
  assert(importer.ImportFromFile(mvrPath.string(), false, false));

  const auto &loadedFixtures = cfg.GetScene().fixtures;
  assert(loadedFixtures.size() == 1);
  assert(loadedFixtures.at("fx-motor").category == "Spot");

  const auto &loaded = cfg.GetScene().supports;
  assert(loaded.size() == 5);

  const auto &loadedLinked = loaded.at("sup-linked");
  assert(loadedLinked.motorFixtureUuid == "fx-motor");
  assert(!loadedLinked.useMotorDefaults);

  const auto &loadedDummy = loaded.at("sup-dummy");
  assert(loadedDummy.dummyProfileId == "d8plus_1000kg");
  assert(loadedDummy.hoistFunction == "Video");

  const auto &loadedManual = loaded.at("sup-manual");
  assert(loadedManual.hoistDataSource == "Manual");
  assert(loadedManual.capacityKg == 1250.0f);
  assert(loadedManual.weightKg == 61.0f);
  assert(loadedManual.loadKg == 410.0f);
  assert(loadedManual.hoistFunction == "Audio");
  assert(loadedManual.motorName == "ChainMaster D8+");
  assert(loadedManual.motorManufacturer == "ChainMaster");
  assert(loadedManual.motorModel == "D8+");
  assert(loadedManual.motorNameSource == "Manual");
  assert(loadedManual.motorManufacturerSource == "Manual");
  assert(loadedManual.motorModelSource == "Manual");
  assert(loadedManual.capacitySource == "Manual");
  assert(loadedManual.weightSource == "Manual");
  assert(loadedManual.hoistFunctionSource == "Manual");


  const auto &loadedInheritedDefaults = loaded.at("sup-inherited-defaults");
  assert(loadedInheritedDefaults.hoistDataSource == "Inherited");
  assert(loadedInheritedDefaults.useMotorDefaults);
  assert(loadedInheritedDefaults.motorManufacturer == "Perastage");
  assert(loadedInheritedDefaults.motorModel == "ChainMode");
  std::filesystem::remove_all(tempDir);
  std::filesystem::remove(ProjectUtils::GetDefaultLibraryPath("fixtures") + "/fixture.gdtf");
  return 0;
}
