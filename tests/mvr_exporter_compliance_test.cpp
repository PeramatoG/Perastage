/*
 * This file is part of Perastage.
 */
#include <algorithm>
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
#include "app_version.h"
#include "fixture.h"
#include "matrixutils.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "sceneobject.h"
#include "truss.h"
#include "support.h"
#include "uuidutils.h"

namespace fs = std::filesystem;
static constexpr const char *kPerastageUserDataSchemaVersion = "1.0";

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

// Finds an exported Fixture XML element by UUID.
static tinyxml2::XMLElement *FindFixtureElementByUuid(tinyxml2::XMLElement *root,
                                                      const std::string &uuid) {
  for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    std::vector<tinyxml2::XMLElement *> stack{node};
    while (!stack.empty()) {
      tinyxml2::XMLElement *cur = stack.back();
      stack.pop_back();
      if (std::string(cur->Name()) == "Fixture") {
        const char *fixtureUuid = cur->Attribute("uuid");
        if (fixtureUuid != nullptr && uuid == fixtureUuid)
          return cur;
      }
      for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
           child = child->NextSiblingElement()) {
        stack.push_back(child);
      }
    }
  }
  return nullptr;
}

// Reads the exported UnitNumber for a fixture UUID.
static int ReadFixtureUnitNumber(tinyxml2::XMLElement *root, const std::string &uuid) {
  tinyxml2::XMLElement *fixture = FindFixtureElementByUuid(root, uuid);
  assert(fixture != nullptr);
  tinyxml2::XMLElement *unitNumber = fixture->FirstChildElement("UnitNumber");
  assert(unitNumber != nullptr && unitNumber->GetText() != nullptr);
  return std::stoi(unitNumber->GetText());
}

static std::string ReadFixtureTypeIdFromGdtf(const fs::path &gdtfPath) {
  const auto gdtfEntries = ReadArchiveTextEntries(gdtfPath);
  auto descriptionIt = gdtfEntries.find("description.xml");
  assert(descriptionIt != gdtfEntries.end());

  tinyxml2::XMLDocument gdtfDoc;
  assert(gdtfDoc.Parse(descriptionIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *fixtureType = FindFixtureType(gdtfDoc);
  assert(fixtureType != nullptr);
  const char *id = fixtureType->Attribute("FixtureTypeID");
  assert(id != nullptr);
  return id;
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
  fs::create_directories(tempDir / "case_a");
  fs::create_directories(tempDir / "case_b");

  std::ofstream(tempDir / "A" / "Same.gdtf") << "A";
  std::ofstream(tempDir / "B" / "Same.gdtf") << "B";
  std::ofstream(tempDir / "case_a" / "CaseOnly.gdtf") << "CASE A";
  std::ofstream(tempDir / "case_b" / "caseonly.gdtf") << "CASE B";
  std::ofstream(tempDir / "@PerastageFixture.gdtf") << "AT";
  std::ofstream(tempDir / "mesh.3ds") << "mesh";
  std::ofstream(tempDir / "models" / "truss_model.3ds") << "truss";
  std::ofstream(tempDir / "models" / "support_model.3ds") << "support";

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
  scene.positions["LX1"] = "LX1";
  scene.positions["P.A."] = "P.A.";
  scene.positions["SCREEN"] = "SCREEN";

  Fixture f1;
  f1.uuid = "fx-1";
  f1.instanceName = "Front Key";
  f1.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  f1.fixtureId = 0;
  f1.fixtureIdNumeric = 0;
  f1.unitNumber = 101;
  f1.address = "1.1";
  f1.position = "LX1";
  f1.positionName = "LX1";
  scene.fixtures[f1.uuid] = f1;

  Fixture f2;
  f2.uuid = "fx-2";
  f2.instanceName = "Back Key";
  f2.gdtfSpec = (tempDir / "B" / "Same.gdtf").generic_string();
  f2.fixtureId = 0;
  f2.fixtureIdNumeric = 0;
  f2.unitNumber = 0;
  f2.address = "3.1";
  f2.color = "#445566";
  scene.fixtures[f2.uuid] = f2;

  Fixture f3;
  f3.uuid = "fx-3";
  f3.instanceName = "Floor Wash";
  f3.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  f3.address = "6.121";
  f3.gelColor = "#00FF00";
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

  Fixture fCaseA;
  fCaseA.uuid = "fx-case-a";
  fCaseA.instanceName = "Case A";
  fCaseA.gdtfSpec = (tempDir / "case_a" / "CaseOnly.gdtf").generic_string();
  fCaseA.address = "19.1";
  scene.fixtures[fCaseA.uuid] = fCaseA;

  Fixture fCaseB;
  fCaseB.uuid = "fx-case-b";
  fCaseB.instanceName = "Case B";
  fCaseB.gdtfSpec = (tempDir / "case_b" / "caseonly.gdtf").generic_string();
  fCaseB.address = "20.1";
  scene.fixtures[fCaseB.uuid] = fCaseB;

  Fixture fAt;
  fAt.uuid = "fx-at";
  fAt.instanceName = "At Fixture";
  fAt.gdtfSpec = (tempDir / "@PerastageFixture.gdtf").generic_string();
  fAt.address = "21.1";
  scene.fixtures[fAt.uuid] = fAt;

  Fixture fEditedId;
  fEditedId.uuid = "fx-edited-id";
  fEditedId.instanceName = "Edited ID Fixture";
  fEditedId.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  fEditedId.fixtureIdText = "Old imported ID";
  fEditedId.fixtureIdNumeric = 12;
  fEditedId.fixtureId = 909;
  fEditedId.address = "9.1";
  scene.fixtures[fEditedId.uuid] = fEditedId;

  Fixture unitOrderBottom;
  unitOrderBottom.uuid = "unit-order-bottom";
  unitOrderBottom.instanceName = "Unit Order Bottom";
  unitOrderBottom.typeName = "  Unit   Order Type  ";
  unitOrderBottom.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitOrderBottom.transform.o = {10.0f, -20.0f, 0.0f};
  unitOrderBottom.address = "10.1";
  scene.fixtures[unitOrderBottom.uuid] = unitOrderBottom;

  Fixture unitOrderLeft;
  unitOrderLeft.uuid = "unit-order-left";
  unitOrderLeft.instanceName = "Unit Order Left";
  unitOrderLeft.typeName = "Unit Order Type";
  unitOrderLeft.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitOrderLeft.transform.o = {-5.0f, 0.0f, 0.0f};
  unitOrderLeft.address = "11.1";
  scene.fixtures[unitOrderLeft.uuid] = unitOrderLeft;

  Fixture unitOrderRight;
  unitOrderRight.uuid = "unit-order-right";
  unitOrderRight.instanceName = "Unit Order Right";
  unitOrderRight.typeName = "Unit Order Type";
  unitOrderRight.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitOrderRight.transform.o = {5.0f, 0.0f, 0.0f};
  unitOrderRight.address = "12.1";
  scene.fixtures[unitOrderRight.uuid] = unitOrderRight;

  Fixture unitExistingOne;
  unitExistingOne.uuid = "unit-existing-one";
  unitExistingOne.instanceName = "Existing Unit One";
  unitExistingOne.typeName = "Existing Unit Type";
  unitExistingOne.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitExistingOne.unitNumber = 1;
  unitExistingOne.address = "13.1";
  scene.fixtures[unitExistingOne.uuid] = unitExistingOne;

  Fixture unitExistingFour;
  unitExistingFour.uuid = "unit-existing-four";
  unitExistingFour.instanceName = "Existing Unit Four";
  unitExistingFour.typeName = "Existing Unit Type";
  unitExistingFour.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitExistingFour.unitNumber = 4;
  unitExistingFour.address = "14.1";
  scene.fixtures[unitExistingFour.uuid] = unitExistingFour;

  Fixture unitExistingMissingA;
  unitExistingMissingA.uuid = "unit-existing-missing-a";
  unitExistingMissingA.instanceName = "Existing Unit Missing A";
  unitExistingMissingA.typeName = "Existing Unit Type";
  unitExistingMissingA.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitExistingMissingA.transform.o = {0.0f, -10.0f, 0.0f};
  unitExistingMissingA.address = "15.1";
  scene.fixtures[unitExistingMissingA.uuid] = unitExistingMissingA;

  Fixture unitExistingMissingB;
  unitExistingMissingB.uuid = "unit-existing-missing-b";
  unitExistingMissingB.instanceName = "Existing Unit Missing B";
  unitExistingMissingB.typeName = "Existing Unit Type";
  unitExistingMissingB.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitExistingMissingB.transform.o = {0.0f, 10.0f, 0.0f};
  unitExistingMissingB.address = "16.1";
  scene.fixtures[unitExistingMissingB.uuid] = unitExistingMissingB;

  Fixture unitIndependentType;
  unitIndependentType.uuid = "unit-independent-type";
  unitIndependentType.instanceName = "Independent Unit Type";
  unitIndependentType.typeName = "Independent Unit Type";
  unitIndependentType.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitIndependentType.address = "17.1";
  scene.fixtures[unitIndependentType.uuid] = unitIndependentType;

  Fixture unitSameAsFixtureId;
  unitSameAsFixtureId.uuid = "unit-same-as-fixture-id";
  unitSameAsFixtureId.instanceName = "Unit Same As Fixture ID";
  unitSameAsFixtureId.typeName = "Same As Fixture ID Type";
  unitSameAsFixtureId.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  unitSameAsFixtureId.fixtureId = 77;
  unitSameAsFixtureId.unitNumber = 77;
  unitSameAsFixtureId.address = "18.1";
  scene.fixtures[unitSameAsFixtureId.uuid] = unitSameAsFixtureId;

  Truss tr;
  tr.uuid = "tr-1";
  tr.name = "Main Truss";
  tr.symbolFile = "mesh.3ds";
  tr.modelFile = (tempDir / "models" / "truss_model.3ds").string();
  tr.lengthMm = 2000.0f;
  tr.position = "P.A.";
  tr.positionName = "P.A.";
  scene.trusses[tr.uuid] = tr;

  Truss trNonNumeric;
  trNonNumeric.uuid = "tr-2";
  trNonNumeric.name = "TRUSS 2M";
  trNonNumeric.symbolFile = "mesh.3ds";
  trNonNumeric.modelFile = (tempDir / "models" / "truss_model.3ds").string();
  trNonNumeric.lengthMm = 2000.0f;
  trNonNumeric.positionName = "P.A.";
  scene.trusses[trNonNumeric.uuid] = trNonNumeric;

  Truss trDifferentType = trNonNumeric;
  trDifferentType.uuid = "tr-3";
  trDifferentType.name = "TRUSS 3M";
  trDifferentType.lengthMm = 3000.0f;
  Truss trCanonical = trNonNumeric;
  trCanonical.uuid = "12345678-1234-4234-9234-123456789ABC";
  trCanonical.name = "Canonical Truss";
  trCanonical.lengthMm = 4000.0f;
  trCanonical.unitNumber = 12;
  trCanonical.customId = 34;
  trCanonical.customIdType = 56;
  scene.trusses[trCanonical.uuid] = trCanonical;

  Support sup;
  sup.uuid = "sup-1";
  sup.name = "Hoist 1";
  sup.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  sup.motorName = "ChainMaster D8+";
  sup.motorManufacturer = "ChainMaster";
  sup.motorModel = "D8+";
  sup.motorFixtureUuid = "fx-1";
  sup.useMotorDefaults = false;
  sup.dummyPreset = "D8+ 1000kg";
  sup.hoistDataSource = "Manual";
  sup.hoistFunction = "Lighting";
  sup.capacityKg = 1000.0f;
  sup.weightKg = 42.0f;
  sup.loadKg = 600.0f;
  sup.position = "SCREEN";
  sup.positionName = "SCREEN";
  sup.chainLength = 1.0f;
  GeometryInstance supportGeometry;
  supportGeometry.modelFile = (tempDir / "models" / "support_model.3ds").string();
  supportGeometry.localTransform = MatrixUtils::Identity();
  sup.geometries.push_back(supportGeometry);
  scene.supports[sup.uuid] = sup;

  SceneObject primitiveSphere;
  primitiveSphere.uuid = "obj-sphere";
  primitiveSphere.name = "Sphere";
  primitiveSphere.modelFile = "primitive:sphere";
  scene.sceneObjects[primitiveSphere.uuid] = primitiveSphere;

  SceneObject primitivePipe;
  primitivePipe.uuid = "obj-pipe";
  primitivePipe.name = "Pipe";
  primitivePipe.transform.u = {14.0f, 0.0f, 0.0f};
  primitivePipe.transform.v = {0.0f, 0.05f, 0.0f};
  primitivePipe.transform.w = {0.0f, 0.0f, 0.05f};
  primitivePipe.transform.o = {0.0f, -2000.0f, 10000.0f};
  GeometryInstance primitivePipeGeo;
  primitivePipeGeo.modelFile = "primitive:cylinder";
  primitivePipeGeo.localTransform.u = {0.0f, 0.0f, -1.0f};
  primitivePipeGeo.localTransform.v = {0.0f, 1.0f, 0.0f};
  primitivePipeGeo.localTransform.w = {1.0f, 0.0f, 0.0f};
  primitivePipe.geometries.push_back(primitivePipeGeo);
  scene.sceneObjects[primitivePipe.uuid] = primitivePipe;

  SceneObject primitivePipeBaked = primitivePipe;
  primitivePipeBaked.uuid = "obj-pipe-baked";
  primitivePipeBaked.name = "Pipe Baked";
  primitivePipeBaked.transform.u = {0.0f, 0.0f, -0.05f};
  primitivePipeBaked.transform.v = {0.0f, 0.05f, 0.0f};
  primitivePipeBaked.transform.w = {14.0f, 0.0f, 0.0f};
  primitivePipeBaked.geometries.clear();
  GeometryInstance primitivePipeBakedGeo;
  primitivePipeBakedGeo.modelFile = "primitive:cylinder";
  primitivePipeBakedGeo.localTransform = MatrixUtils::Identity();
  primitivePipeBaked.geometries.push_back(primitivePipeBakedGeo);
  scene.sceneObjects[primitivePipeBaked.uuid] = primitivePipeBaked;

  MvrExporter exporter;
  fs::path mvrPath = tempDir / "Test1.mvr";
  assert(exporter.ExportToFile(mvrPath.generic_string()));

  const auto mvrGeometryEntries = ReadArchiveTextEntries(mvrPath);
  std::unordered_set<std::string> entries;
  for (const auto &[entryName, _] : mvrGeometryEntries) {
    assert(entryName.find('/') == std::string::npos);
    assert(entryName.find('\\') == std::string::npos);
    assert(entryName.rfind("./", 0) != 0);
    entries.insert(entryName);
  }
  int caseOnlyEntryCount = 0;
  for (const std::string &entryName : entries) {
    std::string lowerEntry = entryName;
    std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowerEntry == "caseonly.gdtf" || lowerEntry == "caseonly_2.gdtf")
      ++caseOnlyEntryCount;
  }
  assert(caseOnlyEntryCount == 2);
  assert(entries.count("@PerastageFixture.gdtf") == 1);
  auto xmlIt = mvrGeometryEntries.find("GeneralSceneDescription.xml");
  assert(xmlIt != mvrGeometryEntries.end());
  std::string xml = xmlIt->second;

  assert(!xml.empty());
  assert(xml.find("Perastage/truss_types") == std::string::npos);
  assert(xml.find("C:\\") == std::string::npos);
  assert(xml.find('\\') == std::string::npos);
  assert(xml.find(tempDir.generic_string()) == std::string::npos);
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = doc.FirstChildElement("GeneralSceneDescription");
  assert(root != nullptr);
  assert(root->IntAttribute("verMajor") == 1);
  assert(root->IntAttribute("verMinor") == 6);
  assert(std::string(root->Attribute("provider")) == "Perastage");
  assert(std::string(root->Attribute("providerVersion")) == app::kVersion);

  assert(ReadFixtureUnitNumber(root, "unit-order-bottom") == 1);
  assert(ReadFixtureUnitNumber(root, "unit-order-left") == 2);
  assert(ReadFixtureUnitNumber(root, "unit-order-right") == 3);
  assert(ReadFixtureUnitNumber(root, "unit-existing-one") == 1);
  assert(ReadFixtureUnitNumber(root, "unit-existing-four") == 4);
  assert(ReadFixtureUnitNumber(root, "unit-existing-missing-a") == 2);
  assert(ReadFixtureUnitNumber(root, "unit-existing-missing-b") == 3);
  assert(ReadFixtureUnitNumber(root, "unit-independent-type") == 1);
  assert(ReadFixtureUnitNumber(root, "unit-same-as-fixture-id") == 77);

  MvrExporter deterministicExporter;
  fs::path deterministicMvrPath = tempDir / "Test1_repeat.mvr";
  assert(deterministicExporter.ExportToFile(deterministicMvrPath.generic_string()));
  const auto deterministicEntries = ReadArchiveTextEntries(deterministicMvrPath);
  auto deterministicXmlIt = deterministicEntries.find("GeneralSceneDescription.xml");
  assert(deterministicXmlIt != deterministicEntries.end());
  tinyxml2::XMLDocument deterministicDoc;
  assert(deterministicDoc.Parse(deterministicXmlIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *deterministicRoot =
      deterministicDoc.FirstChildElement("GeneralSceneDescription");
  assert(deterministicRoot != nullptr);
  assert(ReadFixtureUnitNumber(deterministicRoot, "unit-order-bottom") == 1);
  assert(ReadFixtureUnitNumber(deterministicRoot, "unit-order-left") == 2);
  assert(ReadFixtureUnitNumber(deterministicRoot, "unit-order-right") == 3);
  assert(ReadFixtureUnitNumber(deterministicRoot, "unit-existing-missing-a") == 2);
  assert(ReadFixtureUnitNumber(deterministicRoot, "unit-existing-missing-b") == 3);

  std::unordered_set<int> numericIds;
  std::unordered_map<std::string, int> gdtfCount;

  int fixtureAddressCount = 0;
  bool sawAddress1 = false;
  bool sawAddress1025 = false;
  bool sawAddress2681 = false;
  bool sawAddress3073 = false;
  bool sawAddress3585 = false;
  bool sawAddress4097 = false;
  bool sawEditedFixtureId = false;
  bool sawFixtureVisualizationColorWithoutMvrColor = false;
  bool sawFixtureGelColor = false;
  bool sawNonNumericTrussNameFixtureIdConsistency = false;
  bool sawCanonicalTrussUuidUnchanged = false;
  bool sawInvalidTrussUuidRepaired = false;
  bool sawCanonicalTrussStandardChildren = false;
  std::string repairedInvalidTrussUuid;
  bool sawPrimitiveSphereWithIdentityGeometryMatrix = false;
  bool sawPrimitivePipeObjectMatrixUnbaked = false;
  bool sawPrimitivePipeGeometryMatrixNormalized = false;
  bool sawPrimitivePipeBakedMatrixCanonicalized = false;
  bool sawPrimitivePipePositionPreserved = false;
  bool sawPrimitivePipeBakedPositionPreserved = false;
  int mvrGeometryTrussCount = 0;
  int mvrGeometryTrussesWithGeometry3d = 0;
  int mvrGeometryTrussesWithRenderableGdtf = 0;
  std::unordered_map<std::string, std::string> trussGdtfByUuid;
  for (const char *tagName : {"Fixture", "Truss"}) {
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

            auto *colorNode = cur->FirstChildElement("Color");
            if (fixtureUuid == f2.uuid) {
              assert(colorNode == nullptr);
              sawFixtureVisualizationColorWithoutMvrColor = true;
            }
            if (fixtureUuid == f3.uuid) {
              assert(colorNode != nullptr);
              assert(colorNode->GetText() != nullptr);
              assert(std::string(colorNode->GetText()) == "0.300000,0.600000,0.715200");
              sawFixtureGelColor = true;
            }

            if (fixtureUuid == fEditedId.uuid) {
              assert(fixtureIdText == "909");
              assert(value == 909);
              sawEditedFixtureId = true;
            }

            auto *unitNode = cur->FirstChildElement("UnitNumber");
            assert(unitNode != nullptr && unitNode->GetText() != nullptr);
            int unitValue = std::stoi(unitNode->GetText());
            assert(unitValue > 0);

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
            if (absoluteAddress == ComputeAbsoluteDmx(9, 1))
              sawAddress4097 = true;
            ++fixtureAddressCount;
          }

          if (std::string(cur->Name()) == "Truss") {
            ++mvrGeometryTrussCount;
            auto *gdtfSpec = cur->FirstChildElement("GDTFSpec");
            assert(gdtfSpec != nullptr && gdtfSpec->GetText() != nullptr);
            std::string trussGdtfSpec = gdtfSpec->GetText();
            const char *trussUuidAttr = cur->Attribute("uuid");
            assert(trussUuidAttr != nullptr);
            trussGdtfByUuid[trussUuidAttr] = trussGdtfSpec;
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
            assert(trussUuid != nullptr);
            assert(CanonicalizeUuid(trussUuid) == std::string(trussUuid));
            assert(cur->FirstChildElement("UserData") == nullptr);
            if (std::string(trussUuid) == trCanonical.uuid) {
              sawCanonicalTrussUuidUnchanged = true;
              sawCanonicalTrussStandardChildren =
                  cur->FirstChildElement("FixtureID") != nullptr &&
                  cur->FirstChildElement("FixtureIDNumeric") != nullptr &&
                  cur->FirstChildElement("UnitNumber") != nullptr &&
                  cur->FirstChildElement("CustomId") != nullptr &&
                  cur->FirstChildElement("CustomIdType") != nullptr;
            }
            const char *trussName = cur->Attribute("name");
            if (trussName != nullptr && std::string(trussName) == trNonNumeric.name) {
              sawInvalidTrussUuidRepaired = std::string(trussUuid) != trNonNumeric.uuid;
              repairedInvalidTrussUuid = trussUuid;
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

  for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    std::vector<tinyxml2::XMLElement *> stack{node};
    while (!stack.empty()) {
      tinyxml2::XMLElement *cur = stack.back();
      stack.pop_back();
      if (std::string(cur->Name()) == "SceneObject") {
        const char *sceneObjectUuid = cur->Attribute("uuid");
        auto *geometries = cur->FirstChildElement("Geometries");
        if (sceneObjectUuid && geometries) {
          if (std::string(sceneObjectUuid) == primitiveSphere.uuid) {
            auto *g3d = geometries->FirstChildElement("Geometry3D");
            assert(g3d != nullptr);
            const char *fileName = g3d->Attribute("fileName");
            assert(fileName != nullptr && std::string(fileName).find("sphere") != std::string::npos);
            assert(mvrGeometryEntries.count(fileName) == 1);
            auto *geoMatrix = g3d->FirstChildElement("Matrix");
            assert(geoMatrix != nullptr && geoMatrix->GetText() != nullptr);
            Matrix parsedGeoMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(geoMatrix->GetText(), parsedGeoMatrix));
            const Matrix identity = MatrixUtils::Identity();
            assert(parsedGeoMatrix.u == identity.u);
            assert(parsedGeoMatrix.v == identity.v);
            assert(parsedGeoMatrix.w == identity.w);
            assert(parsedGeoMatrix.o == identity.o);
            sawPrimitiveSphereWithIdentityGeometryMatrix = true;
          }

          if (std::string(sceneObjectUuid) == primitivePipe.uuid) {
            auto *objMatrixNode = cur->FirstChildElement("Matrix");
            assert(objMatrixNode != nullptr && objMatrixNode->GetText() != nullptr);
            Matrix parsedObjMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(objMatrixNode->GetText(), parsedObjMatrix));
            if (parsedObjMatrix.u == primitivePipe.transform.u &&
                parsedObjMatrix.v == primitivePipe.transform.v &&
                parsedObjMatrix.w == primitivePipe.transform.w) {
              sawPrimitivePipeObjectMatrixUnbaked = true;
            }
            if (parsedObjMatrix.o == primitivePipe.transform.o) {
              sawPrimitivePipePositionPreserved = true;
            }

            auto *g3d = geometries->FirstChildElement("Geometry3D");
            assert(g3d != nullptr);
            auto *geoMatrix = g3d->FirstChildElement("Matrix");
            assert(geoMatrix != nullptr && geoMatrix->GetText() != nullptr);
            Matrix parsedGeoMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(geoMatrix->GetText(), parsedGeoMatrix));
            const Matrix identity = MatrixUtils::Identity();
            if (parsedGeoMatrix.u == identity.u &&
                parsedGeoMatrix.v == identity.v &&
                parsedGeoMatrix.w == identity.w &&
                parsedGeoMatrix.o == identity.o) {
              sawPrimitivePipeGeometryMatrixNormalized = true;
            }
          }

          if (std::string(sceneObjectUuid) == primitivePipeBaked.uuid) {
            auto *objMatrixNode = cur->FirstChildElement("Matrix");
            assert(objMatrixNode != nullptr && objMatrixNode->GetText() != nullptr);
            Matrix parsedObjMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(objMatrixNode->GetText(), parsedObjMatrix));
            if (parsedObjMatrix.u == primitivePipe.transform.u &&
                parsedObjMatrix.v == primitivePipe.transform.v &&
                parsedObjMatrix.w == primitivePipe.transform.w) {
              sawPrimitivePipeBakedMatrixCanonicalized = true;
            }
            if (parsedObjMatrix.o == primitivePipe.transform.o) {
              sawPrimitivePipeBakedPositionPreserved = true;
            }
          }
        }
      }
      for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
           child = child->NextSiblingElement()) {
        stack.push_back(child);
      }
    }
  }

  assert(gdtfCount.size() >= 2);
  assert(fixtureAddressCount == 6);
  assert(sawAddress1);
  assert(sawAddress1025);
  assert(sawAddress2681);
  assert(sawAddress3073);
  assert(sawAddress3585);
  assert(sawAddress4097);
  assert(sawEditedFixtureId);
  assert(sawFixtureVisualizationColorWithoutMvrColor);
  assert(sawFixtureGelColor);
  assert(sawNonNumericTrussNameFixtureIdConsistency);
  assert(sawCanonicalTrussUuidUnchanged);
  assert(sawInvalidTrussUuidRepaired);
  assert(!repairedInvalidTrussUuid.empty());
  assert(sawCanonicalTrussStandardChildren);
  assert(sawPrimitiveSphereWithIdentityGeometryMatrix);
  assert(sawPrimitivePipeObjectMatrixUnbaked);
  assert(sawPrimitivePipeGeometryMatrixNormalized);
  assert(sawPrimitivePipeBakedMatrixCanonicalized);
  assert(sawPrimitivePipePositionPreserved);
  assert(sawPrimitivePipeBakedPositionPreserved);
  assert(mvrGeometryTrussCount == static_cast<int>(scene.trusses.size()));
  assert(mvrGeometryTrussesWithGeometry3d == mvrGeometryTrussCount);
  assert(mvrGeometryTrussesWithRenderableGdtf == mvrGeometryTrussCount);

  for (const auto &name : entries) {
    assert(name.rfind("gdtf/", 0) != 0);
  }

  tinyxml2::XMLElement *sceneNode = root->FirstChildElement("Scene");
  assert(sceneNode != nullptr);
  assert(sceneNode->FirstChildElement("UserData") == nullptr);
  tinyxml2::XMLElement *layersNode = sceneNode->FirstChildElement("Layers");
  assert(layersNode != nullptr);
  bool sawDefaultLayerNode = false;
  bool sawSphereInDefaultLayer = false;
  for (tinyxml2::XMLElement *layerOrOther = layersNode->FirstChildElement(); layerOrOther;
       layerOrOther = layerOrOther->NextSiblingElement()) {
    assert(std::string(layerOrOther->Name()) == "Layer");
    const char *layerName = layerOrOther->Attribute("name");
    const std::string layerNameText = layerName ? layerName : "";
    const char *layerUuid = layerOrOther->Attribute("uuid");
    assert(layerUuid != nullptr);
    assert(CanonicalizeUuid(layerUuid) == std::string(layerUuid));
    if (layerNameText == "Default")
      sawDefaultLayerNode = true;

    tinyxml2::XMLElement *childList = layerOrOther->FirstChildElement("ChildList");
    for (tinyxml2::XMLElement *child = childList ? childList->FirstChildElement() : nullptr; child;
         child = child->NextSiblingElement()) {
      if (std::string(child->Name()) != "SceneObject")
        continue;
      const char *uuidAttr = child->Attribute("uuid");
      if (uuidAttr != nullptr && std::string(uuidAttr) == primitiveSphere.uuid &&
          layerNameText == "Default") {
        sawSphereInDefaultLayer = true;
      }
    }
  }
  assert(sawDefaultLayerNode);
  assert(sawSphereInDefaultLayer);
  tinyxml2::XMLElement *rootUserData = root->FirstChildElement("UserData");
  assert(rootUserData != nullptr);
  bool sawRootTrussInfoMap = false;
  bool sawRepairedTrussInfo = false;
  bool sawCanonicalTrussInfo = false;
  for (tinyxml2::XMLElement *data = rootUserData->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    const char *provider = data->Attribute("provider");
    if (provider == nullptr || std::string(provider) != "Perastage")
      continue;
    for (tinyxml2::XMLElement *map = data->FirstChildElement("TrussInfoMap"); map;
         map = map->NextSiblingElement("TrussInfoMap")) {
      sawRootTrussInfoMap = true;
      for (tinyxml2::XMLElement *info = map->FirstChildElement("TrussInfo"); info;
           info = info->NextSiblingElement("TrussInfo")) {
        const char *uuid = info->Attribute("uuid");
        assert(uuid != nullptr);
        assert(CanonicalizeUuid(uuid) == std::string(uuid));
        if (std::string(uuid) == repairedInvalidTrussUuid)
          sawRepairedTrussInfo = true;
        if (std::string(uuid) == trCanonical.uuid)
          sawCanonicalTrussInfo = true;
      }
    }
  }
  assert(sawRootTrussInfoMap);
  assert(sawRepairedTrussInfo);
  assert(sawCanonicalTrussInfo);

  tinyxml2::XMLElement *auxNode = sceneNode->FirstChildElement("AUXData");
  assert(auxNode != nullptr);
  for (tinyxml2::XMLElement *pos = auxNode->FirstChildElement("Position"); pos;
       pos = pos->NextSiblingElement("Position")) {
    const char *uuidAttr = pos->Attribute("uuid");
    assert(uuidAttr != nullptr);
    assert(CanonicalizeUuid(uuidAttr) == std::string(uuidAttr));
  }

  tinyxml2::XMLElement *supportNode = nullptr;
  for (tinyxml2::XMLElement *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement()) {
    std::vector<tinyxml2::XMLElement *> stack{node};
    while (!stack.empty()) {
      tinyxml2::XMLElement *cur = stack.back();
      stack.pop_back();
      if (std::string(cur->Name()) == "SceneObject" &&
          cur->Attribute("uuid") != nullptr &&
          std::string(cur->Attribute("uuid")) == sup.uuid) {
        assert(false && "Support must not be exported as SceneObject");
      }
      if (std::string(cur->Name()) == "Support" &&
          cur->Attribute("uuid") != nullptr &&
          std::string(cur->Attribute("uuid")) == sup.uuid) {
        supportNode = cur;
      }
      for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
           child = child->NextSiblingElement()) {
        stack.push_back(child);
      }
    }
  }
  assert(supportNode != nullptr);
  assert(supportNode->FirstChildElement("Geometries") != nullptr);
  auto *supportUserData = supportNode->FirstChildElement("UserData");
  assert(supportUserData != nullptr);
  assert(supportUserData->NextSiblingElement("UserData") == nullptr);
  auto *supportData = supportUserData->FirstChildElement("Data");
  assert(supportData != nullptr);
  auto *supportInfo = supportData->FirstChildElement("SupportInfo");
  assert(supportInfo != nullptr);
  auto *hoistInfo = supportData->FirstChildElement("HoistInfo");
  assert(hoistInfo != nullptr);
  auto *capacityNode = hoistInfo->FirstChildElement("Capacity");
  assert(capacityNode != nullptr && capacityNode->GetText() != nullptr &&
         std::string(capacityNode->GetText()) == "1000.000000");
  auto *supportPositionNode = supportInfo->FirstChildElement("Position");
  assert(supportPositionNode != nullptr && supportPositionNode->GetText() != nullptr);
  assert(CanonicalizeUuid(supportPositionNode->GetText()) ==
         std::string(supportPositionNode->GetText()));

  std::unordered_map<std::string, std::string> fixtureTypeIdByTrussUuid;
  for (const auto &[trussUuid, gdtfSpec] : trussGdtfByUuid) {
    assert(mvrGeometryEntries.count(gdtfSpec) == 1);
    const fs::path trussGdtfPath = tempDir / ("ftid_" + trussUuid + ".gdtf");
    std::ofstream gdtfOut(trussGdtfPath, std::ios::binary);
    assert(gdtfOut.is_open());
    gdtfOut << mvrGeometryEntries.at(gdtfSpec);
    gdtfOut.close();
    fixtureTypeIdByTrussUuid[trussUuid] = ReadFixtureTypeIdFromGdtf(trussGdtfPath);
  }
  assert(fixtureTypeIdByTrussUuid.at(tr.uuid) == fixtureTypeIdByTrussUuid.at(trNonNumeric.uuid));
  assert(fixtureTypeIdByTrussUuid.at(tr.uuid) !=
         fixtureTypeIdByTrussUuid.at(trDifferentType.uuid));


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

  const fs::path legacyMvrPath = tempDir / "legacy_positions.mvr";
  {
    wxFileOutputStream legacyOut(legacyMvrPath.generic_string());
    assert(legacyOut.IsOk());
    wxZipOutputStream legacyZip(legacyOut);
    const std::string legacyXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" provider=\"Perastage\" providerVersion=\"" +
        std::string(app::kVersion) + "\">"
        "<UserData><Data provider=\"Perastage\" ver=\"" +
        std::string(kPerastageUserDataSchemaVersion) +
        "\"><TrussSidecarManifest/></Data></UserData>"
        "<Scene>"
        "<AUXData><Position uuid=\"LX1\" name=\"LX 1\"/></AUXData>"
        "<Layers><Layer name=\"Default\"><ChildList>"
        "<Fixture uuid=\"fixture-legacy\" name=\"Fixture\">"
        "<FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric>"
        "<GDTFSpec>fixture.gdtf</GDTFSpec><Position>LX1</Position>"
        "</Fixture>"
        "</ChildList></Layer></Layers>"
        "</Scene></GeneralSceneDescription>";
    assert(legacyZip.PutNextEntry("GeneralSceneDescription.xml"));
    legacyZip.Write(legacyXml.data(), legacyXml.size());
    assert(legacyZip.PutNextEntry("fixture.gdtf"));
    legacyZip.Write("dummy", 5);
    legacyZip.Close();
  }

  MvrImporter importer;
  assert(importer.ImportFromFile(legacyMvrPath.generic_string(), false, false));
  const MvrScene &importedScene = ConfigManager::Get().GetScene();
  assert(!importedScene.fixtures.empty());
  const Fixture &legacyFixture = importedScene.fixtures.begin()->second;
  assert(!legacyFixture.position.empty());
  assert(legacyFixture.position != "LX1");
  assert(CanonicalizeUuid(legacyFixture.position) == legacyFixture.position);
  assert(importedScene.positions.count(legacyFixture.position) == 1);
  assert(legacyFixture.color.empty());
  assert(!legacyFixture.gelColor.empty());

  fs::remove_all(tempDir);
  return 0;
}
