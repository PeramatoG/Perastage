/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <cmath>
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
#include "mvr_preferences.h"
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


// Verifies generated truss GDTF archives use the canonical Structure root.
static void AssertGeneratedTrussGdtfStructure(const fs::path &gdtfPath,
                                             const std::string &crossSection,
                                             float expectedLengthMeters,
                                             float expectedWidthMeters,
                                             float expectedHeightMeters) {
  const auto entries = ReadArchiveTextEntries(gdtfPath);
  auto it = entries.find("description.xml");
  assert(it != entries.end());

  tinyxml2::XMLDocument doc;
  assert(doc.Parse(it->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  assert(root != nullptr);
  assert(root->Attribute("DataVersion") != nullptr);
  assert(std::string(root->Attribute("DataVersion")) == "1.2");

  tinyxml2::XMLElement *fixtureType = root->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);
  assert(fixtureType->Attribute("FixtureTypeID") != nullptr);
  assert(fixtureType->FirstChildElement("PerastageMutationAudit") == nullptr);

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  assert(models != nullptr);
  tinyxml2::XMLElement *model = models->FirstChildElement("Model");
  assert(model != nullptr);
  assert(std::abs(model->FloatAttribute("Length") - expectedLengthMeters) < 0.001f);
  assert(std::abs(model->FloatAttribute("Width") - expectedWidthMeters) < 0.001f);
  assert(std::abs(model->FloatAttribute("Height") - expectedHeightMeters) < 0.001f);

  tinyxml2::XMLElement *geometries = fixtureType->FirstChildElement("Geometries");
  assert(geometries != nullptr);
  assert(geometries->FirstChildElement("Geometry") == nullptr);
  tinyxml2::XMLElement *structure = geometries->FirstChildElement("Structure");
  assert(structure != nullptr);
  assert(std::string(structure->Attribute("Name")) == "Root");
  assert(std::string(structure->Attribute("Model")) == "Main");
  assert(std::string(structure->Attribute("StructureType")) == "Detail");
  assert(std::string(structure->Attribute("CrossSectionType")) ==
         "TrussFramework");
  assert(std::string(structure->Attribute("TrussCrossSection")) == crossSection);
  assert(fixtureType->FirstChildElement("Magnet") == nullptr);

  tinyxml2::XMLElement *dmxModes = fixtureType->FirstChildElement("DMXModes");
  assert(dmxModes != nullptr);
  tinyxml2::XMLElement *mode = dmxModes->FirstChildElement("DMXMode");
  assert(mode != nullptr);
  assert(std::string(mode->Attribute("Name")) == "Default");
  assert(std::string(mode->Attribute("Geometry")) == "Root");
  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  assert(revisions != nullptr);
  assert(revisions->FirstChildElement("Revision") != nullptr);
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
  scene.symdefFiles["44444444-4444-4444-4444-444444444444"] = symModel.generic_string();

  assert(mvr::preferences::LoadExportOptions(cfg).trussGeometryExportMode ==
         MvrTrussGeometryExportMode::Standard);
  MvrExportOptions directPreferenceOptions;
  directPreferenceOptions.trussGeometryExportMode =
      MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols;
  mvr::preferences::SaveExportOptions(cfg, directPreferenceOptions);
  assert(mvr::preferences::LoadExportOptions(cfg).trussGeometryExportMode ==
         MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols);
  mvr::preferences::SaveExportOptions(cfg, MvrExportOptions{});

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

  MvrExportOptions directOptions;
  directOptions.trussGeometryExportMode =
      MvrTrussGeometryExportMode::DirectGeometry3DForTrussSymbols;
  const fs::path directMvrPath = tempDir / "direct-truss-geometry.mvr";
  assert(exporter.ExportToFile(directMvrPath.string(), directOptions));
  const auto directEntries = ReadArchiveTextEntries(directMvrPath);
  tinyxml2::XMLDocument directXml;
  assert(directXml.Parse(directEntries.at("GeneralSceneDescription.xml").c_str()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *directSceneNode =
      directXml.FirstChildElement("GeneralSceneDescription")->FirstChildElement("Scene");
  tinyxml2::XMLElement *directTrussNode =
      directSceneNode->FirstChildElement("Layers")
          ->FirstChildElement("ChildList")
          ->FirstChildElement("GroupObject")
          ->FirstChildElement("ChildList")
          ->FirstChildElement("Truss");
  tinyxml2::XMLElement *directGeos = directTrussNode->FirstChildElement("Geometries");
  assert(directGeos != nullptr);
  assert(directGeos->FirstChildElement("Symbol") == nullptr);
  tinyxml2::XMLElement *directGeometry = directGeos->FirstChildElement("Geometry3D");
  assert(directGeometry != nullptr);
  const char *directFileName = directGeometry->Attribute("fileName");
  assert(directFileName != nullptr);
  assert(directEntries.find(directFileName) != directEntries.end());
  tinyxml2::XMLElement *directAux = directSceneNode->FirstChildElement("AUXData");
  if (directAux)
    assert(directAux->FirstChildElement("Symdef") == nullptr);

  MvrExportOptions projectOptions;
  projectOptions.trussGeometryExportMode = MvrTrussGeometryExportMode::Standard;
  std::vector<uint8_t> projectSceneBytes;
  assert(exporter.ExportToBuffer(projectSceneBytes, projectOptions));
  assert(!projectSceneBytes.empty());

  assert(sceneNode->FirstChildElement("UserData") == nullptr);
  tinyxml2::XMLElement *rootUserData = xml.FirstChildElement("GeneralSceneDescription")->FirstChildElement("UserData");
  assert(rootUserData != nullptr);
  tinyxml2::XMLElement *rootData = rootUserData->FirstChildElement("Data");
  assert(rootData != nullptr);
  assert(rootData->FirstChildElement("TrussSidecarManifest") == nullptr);
  tinyxml2::XMLElement *trussInfoMap = rootData->FirstChildElement("TrussInfoMap");
  assert(trussInfoMap != nullptr);
  tinyxml2::XMLElement *trussInfo = trussInfoMap->FirstChildElement("TrussInfo");
  assert(trussInfo != nullptr);
  assert(std::string(trussInfo->Attribute("uuid")) == truss.uuid);
  assert(std::string(trussInfo->FirstChildElement("Manufacturer")->GetText()) ==
         truss.manufacturer);
  assert(std::string(trussInfo->FirstChildElement("Model")->GetText()) ==
         truss.model);
  assert(std::abs(std::stof(trussInfo->FirstChildElement("Length")->GetText()) -
                  truss.lengthMm) < 0.001f);
  assert(std::abs(std::stof(trussInfo->FirstChildElement("Width")->GetText()) -
                  truss.widthMm) < 0.001f);
  assert(std::abs(std::stof(trussInfo->FirstChildElement("Height")->GetText()) -
                  truss.heightMm) < 0.001f);
  assert(std::abs(std::stof(trussInfo->FirstChildElement("Weight")->GetText()) -
                  truss.weightKg) < 0.001f);
  assert(trussInfo->FirstChildElement("Load") == nullptr);

  scene.trusses.at(truss.uuid).manualLoadKg = 123.45f;
  scene.trusses.at(truss.uuid).hasManualLoadOverride = true;
  const fs::path manualLoadMvrPath = tempDir / "manual-load.mvr";
  assert(exporter.ExportToFile(manualLoadMvrPath.string()));
  const auto manualLoadEntries = ReadArchiveTextEntries(manualLoadMvrPath);
  tinyxml2::XMLDocument manualLoadXml;
  assert(manualLoadXml.Parse(
             manualLoadEntries.at("GeneralSceneDescription.xml").c_str()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *manualLoadInfo =
      manualLoadXml.FirstChildElement("GeneralSceneDescription")
          ->FirstChildElement("UserData")
          ->FirstChildElement("Data")
          ->FirstChildElement("TrussInfoMap")
          ->FirstChildElement("TrussInfo");
  assert(manualLoadInfo != nullptr);
  tinyxml2::XMLElement *manualLoad = manualLoadInfo->FirstChildElement("Load");
  assert(manualLoad != nullptr);
  assert(std::string(manualLoad->Attribute("unit")) == "kg");
  assert(std::string(manualLoad->Attribute("source")) == "Manual");
  scene.trusses.at(truss.uuid).manualLoadKg = 0.0f;
  scene.trusses.at(truss.uuid).hasManualLoadOverride = false;

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
  assert(std::abs(importedTruss.lengthMm - truss.lengthMm) < 0.001f);
  assert(std::abs(importedTruss.widthMm - truss.widthMm) < 0.001f);
  assert(std::abs(importedTruss.heightMm - truss.heightMm) < 0.001f);
  assert(std::abs(importedTruss.weightKg - truss.weightKg) < 0.001f);
  assert(importedTruss.crossSection == truss.crossSection);

  cfg.Reset();
  assert(importer.ImportFromFile(manualLoadMvrPath.string(), false, false));
  const Truss &manualLoadImportedTruss =
      ConfigManager::Get().GetScene().trusses.begin()->second;
  assert(manualLoadImportedTruss.hasManualLoadOverride);
  assert(std::abs(manualLoadImportedTruss.manualLoadKg - 123.45f) < 0.001f);

  const fs::path legacyMvrPath = tempDir / "legacy-truss-userdata.mvr";
  {
    wxFileOutputStream legacyOut(legacyMvrPath.generic_string());
    assert(legacyOut.IsOk());
    wxZipOutputStream legacyZip(legacyOut);
    const std::string legacyXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Perastage\" providerVersion=\"test\">"
        "<Scene><Layers><Layer name=\"Default\"><ChildList>"
        "<Truss uuid=\"uuid_17311332189900\" name=\"Legacy Truss\">"
        "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
        "<FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric>"
        "<UserData><Data provider=\"Perastage\" ver=\"1.0\"><TrussInfo uuid=\"uuid_17311332189900\">"
        "<Manufacturer>Legacy Maker</Manufacturer><Model>Legacy Model</Model><Length unit=\"mm\">2500</Length>"
        "<Width unit=\"mm\">400</Width><Height unit=\"mm\">400</Height><Weight unit=\"kg\">30</Weight>"
        "<CrossSection>40x40</CrossSection><HangPos>Legacy Position</HangPos>"
        "</TrussInfo></Data></UserData></Truss>"
        "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
    assert(legacyZip.PutNextEntry("GeneralSceneDescription.xml"));
    legacyZip.Write(legacyXml.data(), legacyXml.size());
    legacyZip.Close();
  }

  cfg.Reset();
  assert(importer.ImportFromFile(legacyMvrPath.string(), false, false));
  MvrScene &legacyImportedScene = ConfigManager::Get().GetScene();
  assert(legacyImportedScene.trusses.size() == 1);
  const Truss &legacyImportedTruss = legacyImportedScene.trusses.begin()->second;
  assert(CanonicalizeUuid(legacyImportedTruss.uuid) == legacyImportedTruss.uuid);
  assert(legacyImportedTruss.manufacturer == "Legacy Maker");
  assert(legacyImportedTruss.model == "Legacy Model");
  assert(legacyImportedTruss.lengthMm == 2500.0f);
  const fs::path migratedMvrPath = tempDir / "legacy-truss-migrated.mvr";
  assert(exporter.ExportToFile(migratedMvrPath.string()));
  const auto migratedEntries = ReadArchiveTextEntries(migratedMvrPath);
  tinyxml2::XMLDocument migratedXml;
  assert(migratedXml.Parse(migratedEntries.at("GeneralSceneDescription.xml").c_str()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *migratedRoot = migratedXml.FirstChildElement("GeneralSceneDescription");
  assert(migratedRoot != nullptr);
  bool sawMigratedTrussInfo = false;
  std::vector<tinyxml2::XMLElement *> migratedStack{migratedRoot};
  while (!migratedStack.empty()) {
    tinyxml2::XMLElement *node = migratedStack.back();
    migratedStack.pop_back();
    if (std::string(node->Name()) == "Truss") {
      assert(node->FirstChildElement("UserData") == nullptr);
      const char *uuid = node->Attribute("uuid");
      assert(uuid != nullptr);
      assert(CanonicalizeUuid(uuid) == std::string(uuid));
    }
    if (std::string(node->Name()) == "TrussInfo") {
      const char *uuid = node->Attribute("uuid");
      assert(uuid != nullptr);
      assert(CanonicalizeUuid(uuid) == std::string(uuid));
      sawMigratedTrussInfo = true;
    }
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      migratedStack.push_back(child);
  }
  assert(sawMigratedTrussInfo);

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
  typeA.crossSection = "GenericTruss";

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
  AssertGeneratedTrussGdtfStructure(gdtfA1, "GenericTruss", 3.0f, 0.4f, 0.4f);
  assert(ReadFixtureTypeIdFromGdtf(gdtfA1) == ReadFixtureTypeIdFromGdtf(gdtfA2));
  assert(ReadFixtureTypeIdFromGdtf(gdtfA1) != ReadFixtureTypeIdFromGdtf(gdtfB));

  return 0;
}
