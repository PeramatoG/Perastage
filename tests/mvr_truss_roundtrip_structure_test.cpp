/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "matrixutils.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "truss_gdtf_builder.h"
#include "uuidutils.h"

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
    entries[entry->GetName().ToStdString()] = ReadCurrentZipEntry(zip);
  }
  return entries;
}

static std::string ReadFixtureTypeIdFromGdtf(const fs::path &gdtfPath) {
  const auto entries = ReadArchiveTextEntries(gdtfPath);
  auto it = entries.find("description.xml");
  assert(it != entries.end());
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(it->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  assert(root != nullptr);
  tinyxml2::XMLElement *fixtureType = root->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);
  const char *id = fixtureType->Attribute("FixtureTypeID");
  assert(id != nullptr);
  return id;
}

// Returns the first Symbol UUID exported in GeneralSceneDescription.xml.
static std::string ReadFirstSymbolUuid(const fs::path &mvrPath) {
  const auto entries = ReadArchiveTextEntries(mvrPath);
  tinyxml2::XMLDocument xml;
  assert(xml.Parse(entries.at("GeneralSceneDescription.xml").c_str()) == tinyxml2::XML_SUCCESS);
  std::vector<tinyxml2::XMLElement *> stack{xml.FirstChildElement("GeneralSceneDescription")};
  while (!stack.empty()) {
    tinyxml2::XMLElement *node = stack.back();
    stack.pop_back();
    if (std::string(node->Name()) == "Symbol")
      return node->Attribute("uuid") ? node->Attribute("uuid") : "";
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }
  return {};
}

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempDir =
      fs::temp_directory_path() / "mvr_truss_roundtrip_structure_test";
  fs::remove_all(tempDir);
  fs::create_directories(tempDir / "models");

  const fs::path symModel = tempDir / "models" / "sym_truss.3ds";
  std::ofstream(symModel) << "sym-model";

  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  MvrScene &scene = cfg.GetScene();
  scene.basePath = tempDir.generic_string();

  const std::string symdefUuid = "11111111-1111-1111-1111-111111111111";
  const std::string sourceSymbolUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  SymdefGeometry symGeo;
  symGeo.file = symModel.generic_string();
  symGeo.transform = MatrixUtils::Identity();
  scene.symdefGeometries[symdefUuid] = {symGeo};
  scene.symdefFiles[symdefUuid] = symModel.generic_string();
  scene.symdefMatrices[symdefUuid] = MatrixUtils::Identity();

  GroupObject group;
  group.uuid = "22222222-2222-2222-2222-222222222222";
  group.name = "Imported Group";
  group.layer = DEFAULT_LAYER_NAME;
  group.localTransform = MatrixUtils::Identity();
  group.transform = MatrixUtils::Identity();
  scene.groupObjects[group.uuid] = group;

  Truss truss;
  truss.uuid = "33333333-3333-3333-3333-333333333333";
  truss.name = "Truss A";
  truss.layer = DEFAULT_LAYER_NAME;
  truss.localTransform = MatrixUtils::Identity();
  truss.transform = MatrixUtils::Identity();
  truss.sourceRepresentation = Truss::GeometryRepresentation::SymbolSymdef;
  truss.sourceSymbolUuid = sourceSymbolUuid;
  truss.sourceSymdefUuid = symdefUuid;
  truss.sourceSymbolMatrix = MatrixUtils::Identity();
  truss.parentGroupUuid = group.uuid;
  truss.symbolFile = symModel.generic_string();
  truss.modelFile = symModel.generic_string();
  truss.manufacturer = "Perastage";
  truss.model = "Tower 40";
  truss.lengthMm = 3000.0f;
  truss.widthMm = 400.0f;
  truss.heightMm = 400.0f;
  truss.weightKg = 40.0f;
  truss.crossSection = "40x40";
  scene.trusses[truss.uuid] = truss;
  scene.groupObjects[group.uuid].children.push_back({MvrNodeType::Truss, truss.uuid});

  MvrExporter exporter;
  const fs::path mvrPath = tempDir / "roundtrip.mvr";
  assert(exporter.ExportToFile(mvrPath.string()));

  const auto entries = ReadArchiveTextEntries(mvrPath);
  const auto xmlIt = entries.find("GeneralSceneDescription.xml");
  assert(xmlIt != entries.end());

  tinyxml2::XMLDocument xml;
  assert(xml.Parse(xmlIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *sceneNode =
      xml.FirstChildElement("GeneralSceneDescription")->FirstChildElement("Scene");
  assert(sceneNode != nullptr);

  tinyxml2::XMLElement *layers = sceneNode->FirstChildElement("Layers");
  assert(layers != nullptr);
  tinyxml2::XMLElement *rootChildList = layers->FirstChildElement("ChildList");
  assert(rootChildList != nullptr);
  tinyxml2::XMLElement *groupNode = rootChildList->FirstChildElement("GroupObject");
  assert(groupNode != nullptr);
  tinyxml2::XMLElement *groupChildList = groupNode->FirstChildElement("ChildList");
  assert(groupChildList != nullptr);
  tinyxml2::XMLElement *trussNode = groupChildList->FirstChildElement("Truss");
  assert(trussNode != nullptr);
  int matrixOrder = -1;
  int geosOrder = -1;
  int fixtureIdOrder = -1;
  int fixtureNumericOrder = -1;
  int childOrder = 0;
  for (tinyxml2::XMLElement *child = trussNode->FirstChildElement(); child;
       child = child->NextSiblingElement(), ++childOrder) {
    const std::string childName = child->Name();
    if (childName == "Matrix" && matrixOrder < 0)
      matrixOrder = childOrder;
    else if (childName == "Geometries" && geosOrder < 0)
      geosOrder = childOrder;
    else if (childName == "FixtureID" && fixtureIdOrder < 0)
      fixtureIdOrder = childOrder;
    else if (childName == "FixtureIDNumeric" && fixtureNumericOrder < 0)
      fixtureNumericOrder = childOrder;
  }
  assert(matrixOrder >= 0);
  assert(geosOrder >= 0);
  assert(fixtureIdOrder >= 0);
  assert(fixtureNumericOrder >= 0);
  assert(matrixOrder < geosOrder);
  assert(geosOrder < fixtureIdOrder);
  assert(fixtureIdOrder < fixtureNumericOrder);

  tinyxml2::XMLElement *geos = trussNode->FirstChildElement("Geometries");
  assert(geos != nullptr);
  tinyxml2::XMLElement *symbol = geos->FirstChildElement("Symbol");
  assert(symbol != nullptr);
  assert(geos->FirstChildElement("Geometry3D") == nullptr);
  assert(std::string(symbol->Attribute("uuid")) == sourceSymbolUuid);
  assert(std::string(symbol->Attribute("symdef")) == symdefUuid);

  tinyxml2::XMLElement *aux = sceneNode->FirstChildElement("AUXData");
  assert(aux != nullptr);
  bool foundSymdef = false;
  for (tinyxml2::XMLElement *sym = aux->FirstChildElement("Symdef"); sym;
       sym = sym->NextSiblingElement("Symdef")) {
    if (std::string(sym->Attribute("uuid")) == symdefUuid) {
      foundSymdef = true;
      break;
    }
  }
  assert(foundSymdef);

  tinyxml2::XMLElement *sceneUserData = sceneNode->FirstChildElement("UserData");
  assert(sceneUserData != nullptr);
  tinyxml2::XMLElement *manifest = sceneUserData->FirstChildElement("Data")->FirstChildElement("TrussSidecarManifest");
  assert(manifest != nullptr);
  tinyxml2::XMLElement *typeNode = manifest->FirstChildElement("Type");
  assert(typeNode != nullptr);
  assert(std::string(typeNode->Attribute("gdtf")).rfind("Perastage/truss_types/", 0) == 0);

  cfg.Reset();
  MvrImporter importer;
  assert(importer.ImportFromFile(mvrPath.string(), false, false));
  auto &importedScene = ConfigManager::Get().GetScene();
  assert(importedScene.groupObjects.size() == 1);
  assert(importedScene.trusses.size() == 1);
  const Truss &importedTruss = importedScene.trusses.begin()->second;
  assert(importedTruss.sourceRepresentation == Truss::GeometryRepresentation::SymbolSymdef);
  assert(importedTruss.sourceSymbolUuid == sourceSymbolUuid);
  assert(!importedTruss.perastageAuxGdtfArchivePath.empty());
  assert(importedTruss.manufacturer == "Perastage");
  assert(importedTruss.model == "Tower 40");

  cfg.Reset();
  MvrScene &generatedScene = cfg.GetScene();
  generatedScene.basePath = tempDir.generic_string();
  generatedScene.symdefGeometries[symdefUuid] = {symGeo};
  generatedScene.symdefFiles[symdefUuid] = symModel.generic_string();
  generatedScene.symdefMatrices[symdefUuid] = MatrixUtils::Identity();
  GroupObject generatedGroup = group;
  generatedGroup.uuid = "44444444-4444-4444-4444-444444444444";
  generatedGroup.children.clear();
  generatedScene.groupObjects[generatedGroup.uuid] = generatedGroup;
  Truss generatedTruss = truss;
  generatedTruss.uuid = "55555555-5555-5555-5555-555555555555";
  generatedTruss.sourceSymbolUuid.clear();
  generatedTruss.parentGroupUuid = generatedGroup.uuid;
  generatedScene.trusses[generatedTruss.uuid] = generatedTruss;
  generatedScene.groupObjects[generatedGroup.uuid].children.push_back(
      {MvrNodeType::Truss, generatedTruss.uuid});

  const fs::path generatedA = tempDir / "generated-a.mvr";
  const fs::path generatedB = tempDir / "generated-b.mvr";
  assert(exporter.ExportToFile(generatedA.string()));
  const std::string generatedSymbolUuidA = ReadFirstSymbolUuid(generatedA);
  assert(CanonicalizeUuid(generatedSymbolUuidA) == generatedSymbolUuidA);
  assert(generatedSymbolUuidA != generatedTruss.uuid);
  assert(generatedSymbolUuidA != symdefUuid);
  assert(exporter.ExportToFile(generatedB.string()));
  assert(ReadFirstSymbolUuid(generatedB) == generatedSymbolUuidA);

  Truss collidingTruss = generatedTruss;
  collidingTruss.uuid = "66666666-6666-6666-6666-666666666666";
  collidingTruss.sourceSymbolUuid = sourceSymbolUuid;
  generatedScene.trusses[generatedTruss.uuid].sourceSymbolUuid = sourceSymbolUuid;
  generatedScene.trusses[collidingTruss.uuid] = collidingTruss;
  generatedScene.groupObjects[generatedGroup.uuid].children.push_back(
      {MvrNodeType::Truss, collidingTruss.uuid});
  const fs::path collisionMvr = tempDir / "collision.mvr";
  assert(exporter.ExportToFile(collisionMvr.string()));
  assert(!exporter.GetExportWarnings().empty());
  const auto collisionEntries = ReadArchiveTextEntries(collisionMvr);
  tinyxml2::XMLDocument collisionXml;
  assert(collisionXml.Parse(collisionEntries.at("GeneralSceneDescription.xml").c_str()) ==
         tinyxml2::XML_SUCCESS);
  std::unordered_set<std::string> collisionSymbolUuids;
  std::vector<tinyxml2::XMLElement *> collisionStack{
      collisionXml.FirstChildElement("GeneralSceneDescription")};
  while (!collisionStack.empty()) {
    tinyxml2::XMLElement *node = collisionStack.back();
    collisionStack.pop_back();
    if (std::string(node->Name()) == "Symbol") {
      const std::string exportedUuid = node->Attribute("uuid") ? node->Attribute("uuid") : "";
      assert(CanonicalizeUuid(exportedUuid) == exportedUuid);
      assert(node->Attribute("symdef") != nullptr);
      assert(collisionSymbolUuids.insert(exportedUuid).second);
    }
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      collisionStack.push_back(child);
  }
  assert(collisionSymbolUuids.size() == 2);

  Truss typeA;
  typeA.symbolFile = symModel.generic_string();
  typeA.modelFile = symModel.generic_string();
  typeA.manufacturer = "Perastage";
  typeA.model = "Tower 40";
  typeA.lengthMm = 3000.0f;
  typeA.widthMm = 400.0f;
  typeA.heightMm = 400.0f;
  typeA.weightKg = 40.0f;

  Truss typeASame = typeA;
  Truss typeB = typeA;
  typeB.model = "Tower 50";

  const fs::path gdtfA1 = tempDir / "typeA1.gdtf";
  const fs::path gdtfA2 = tempDir / "typeA2.gdtf";
  const fs::path gdtfB = tempDir / "typeB.gdtf";
  std::string error;
  assert(BuildTrussGdtfFromInstance(typeA, gdtfA1, &error));
  assert(BuildTrussGdtfFromInstance(typeASame, gdtfA2, &error));
  assert(BuildTrussGdtfFromInstance(typeB, gdtfB, &error));
  assert(ReadFixtureTypeIdFromGdtf(gdtfA1) == ReadFixtureTypeIdFromGdtf(gdtfA2));
  assert(ReadFixtureTypeIdFromGdtf(gdtfA1) != ReadFixtureTypeIdFromGdtf(gdtfB));

  return 0;
}
