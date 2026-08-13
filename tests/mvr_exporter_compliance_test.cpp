/*
 * This file is part of Perastage.
 */
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "app_version.h"
#include "build_info.h"
#include "configmanager.h"
#include "fixture.h"
#include "gdtf_test_fixture_builder.h"
#include "matrixutils.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "sceneobject.h"
#include "support.h"
#include "truss.h"
#include "uuidutils.h"

namespace fs = std::filesystem;
static constexpr const char *kPerastageUserDataSchemaVersion = "1.0";

// Appends one little-endian integer to a binary test fixture.
template <typename T>
static void AppendBinaryValue(std::vector<uint8_t> &out, T value) {
  for (size_t index = 0; index < sizeof(T); ++index)
    out.push_back(static_cast<uint8_t>((value >> (index * 8)) & 0xffu));
}

// Appends one little-endian float to a binary test fixture.
static void AppendBinaryFloat(std::vector<uint8_t> &out, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(value));
  AppendBinaryValue(out, bits);
}

// Appends one nested 3DS chunk to a binary test fixture.
static void Append3dsChunk(std::vector<uint8_t> &out, uint16_t id,
                           const std::vector<uint8_t> &payload) {
  AppendBinaryValue<uint16_t>(out, id);
  AppendBinaryValue<uint32_t>(out,
                              static_cast<uint32_t>(payload.size() + 6));
  out.insert(out.end(), payload.begin(), payload.end());
}

// Builds a minimal valid 3DS resource for truss export compliance tests.
static std::string BuildTrussGeometry3ds() {
  std::vector<uint8_t> vertices;
  AppendBinaryValue<uint16_t>(vertices, 4);
  const float points[] = {0.0f, 0.0f, 0.0f, 2000.0f, 0.0f, 0.0f,
                          0.0f, 400.0f, 0.0f, 0.0f, 0.0f, 400.0f};
  for (float value : points)
    AppendBinaryFloat(vertices, value);
  std::vector<uint8_t> faces;
  AppendBinaryValue<uint16_t>(faces, 2);
  for (uint16_t value : {0, 1, 2, 0, 0, 2, 3, 0})
    AppendBinaryValue<uint16_t>(faces, value);
  std::vector<uint8_t> mesh;
  Append3dsChunk(mesh, 0x4110, vertices);
  Append3dsChunk(mesh, 0x4120, faces);
  std::vector<uint8_t> object{'t', 'r', 'u', 's', 's', 0};
  Append3dsChunk(object, 0x4100, mesh);
  std::vector<uint8_t> editor;
  Append3dsChunk(editor, 0x4000, object);
  std::vector<uint8_t> root;
  Append3dsChunk(root, 0x3D3D, editor);
  std::vector<uint8_t> archive;
  Append3dsChunk(archive, 0x4D4D, root);
  return {reinterpret_cast<const char *>(archive.data()), archive.size()};
}

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

// Reads the fixture name and identity from an in-memory GDTF archive.
static std::pair<std::string, std::string>
ReadGdtfFixtureIdentity(const std::string &archiveBytes) {
  wxMemoryInputStream input(archiveBytes.data(), archiveBytes.size());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName() != "description.xml") {
      ReadCurrentZipEntry(zip);
      continue;
    }
    tinyxml2::XMLDocument doc;
    const std::string description = ReadCurrentZipEntry(zip);
    assert(doc.Parse(description.c_str()) == tinyxml2::XML_SUCCESS);
    tinyxml2::XMLElement *fixtureType = FindFixtureType(doc);
    assert(fixtureType != nullptr);
    return {fixtureType->Attribute("Name") ? fixtureType->Attribute("Name") : "",
            fixtureType->Attribute("FixtureTypeID")
                ? fixtureType->Attribute("FixtureTypeID")
                : ""};
  }
  assert(false);
  return {};
}

// Finds a fixture's GDTFSpec by its exported instance name.
static std::string FindFixtureGdtfSpec(tinyxml2::XMLElement *root,
                                       const std::string &fixtureName) {
  std::vector<tinyxml2::XMLElement *> stack{root};
  while (!stack.empty()) {
    tinyxml2::XMLElement *node = stack.back();
    stack.pop_back();
    if (std::string(node->Name()) == "Fixture" && node->Attribute("name") &&
        fixtureName == node->Attribute("name")) {
      tinyxml2::XMLElement *spec = node->FirstChildElement("GDTFSpec");
      return spec && spec->GetText() ? spec->GetText() : "";
    }
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }
  return {};
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

  tinyxml2::XMLElement *geometries =
      fixtureType->FirstChildElement("Geometries");
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
static tinyxml2::XMLElement *
FindFixtureElementByUuid(tinyxml2::XMLElement *root, const std::string &uuid) {
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

// Finds an exported SceneObject XML element by its unique name.
static tinyxml2::XMLElement *
FindSceneObjectElementByName(tinyxml2::XMLElement *root,
                             const std::string &name) {
  std::vector<tinyxml2::XMLElement *> stack{root};
  while (!stack.empty()) {
    tinyxml2::XMLElement *node = stack.back();
    stack.pop_back();
    if (std::string(node->Name()) == "SceneObject" &&
        node->Attribute("name") != nullptr && name == node->Attribute("name"))
      return node;
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }
  return nullptr;
}

// Reads the exported UnitNumber for a fixture instance name.
static int ReadFixtureUnitNumber(tinyxml2::XMLElement *root,
                                 const std::string &fixtureName) {
  tinyxml2::XMLElement *fixture = nullptr;
  std::vector<tinyxml2::XMLElement *> stack{root};
  while (!stack.empty() && !fixture) {
    tinyxml2::XMLElement *node = stack.back();
    stack.pop_back();
    if (std::string(node->Name()) == "Fixture" && node->Attribute("name") &&
        fixtureName == node->Attribute("name")) {
      fixture = node;
      break;
    }
    for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement())
      stack.push_back(child);
  }
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

// Verifies generated GDTF files use standard version and revision metadata.
static void AssertGeneratedGdtfVersioning(const fs::path &gdtfPath) {
  const auto gdtfEntries = ReadArchiveTextEntries(gdtfPath);
  auto descriptionIt = gdtfEntries.find("description.xml");
  assert(descriptionIt != gdtfEntries.end());

  tinyxml2::XMLDocument gdtfDoc;
  assert(gdtfDoc.Parse(descriptionIt->second.c_str()) == tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *root = gdtfDoc.FirstChildElement("GDTF");
  assert(root != nullptr);
  assert(std::string(root->Attribute("DataVersion")) == "1.2");
  tinyxml2::XMLElement *fixtureType = root->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);
  assert(fixtureType->FirstChildElement("PerastageMutationAudit") == nullptr);
  tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  assert(revisions != nullptr);
  tinyxml2::XMLElement *revision = revisions->FirstChildElement("Revision");
  assert(revision != nullptr);
  const std::string expectedModifiedBy =
      std::string("Perastage ") + std::string(perastage::build_info::appVersion());
  assert(std::string(revision->Attribute("ModifiedBy")) == expectedModifiedBy);
}

// Writes a minimal valid GDTF archive for exporter resource resolution tests.
static void WriteMinimalGdtfArchive(const fs::path &gdtfPath,
                                    const std::string &fixtureName,
                                    const std::string &fixtureTypeId) {
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity(fixtureName, "Acme", fixtureTypeId)
      .WriteArchive(gdtfPath);
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

  WriteMinimalGdtfArchive(tempDir / "A" / "Same.gdtf", "Same A",
                          "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
  WriteMinimalGdtfArchive(tempDir / "B" / "Same.gdtf", "Same B",
                          "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
  WriteMinimalGdtfArchive(tempDir / "case_a" / "CaseOnly.gdtf", "Case A",
                          "cccccccc-cccc-4ccc-8ccc-ccccccccccc3");
  WriteMinimalGdtfArchive(tempDir / "case_b" / "caseonly.gdtf", "Case B",
                          "dddddddd-dddd-4ddd-8ddd-ddddddddddd4");
  WriteMinimalGdtfArchive(tempDir / "@PerastageFixture.gdtf", "At Fixture",
                          "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeee5");
  WriteMinimalGdtfArchive(tempDir / "SiblingOnly.gdtf", "SiblingOnly",
                          "ffffffff-ffff-4fff-8fff-fffffffffff6");
  const std::string trussGeometry = BuildTrussGeometry3ds();
  std::ofstream(tempDir / "mesh.3ds", std::ios::binary) << trussGeometry;
  std::ofstream(tempDir / "models" / "truss_model.3ds", std::ios::binary)
      << trussGeometry;
  std::ofstream(tempDir / "models" / "support_model.3ds") << "support";

  const std::string longToken(320, "x"[0]);
  const fs::path longNamedGdtf =
      tempDir / ("VeryLongGeneratedName_" + longToken + ".gdtf");
  const fs::path duplicateLongNamedGdtf =
      tempDir / "C" / ("VeryLongGeneratedName_" + longToken + ".gdtf");
  std::ofstream(longNamedGdtf) << "LONG";
  std::ofstream(duplicateLongNamedGdtf) << "LONG2";

  scene.basePath = tempDir.generic_string();
  scene.provider = "ImportedApp";
  scene.providerVersion = "9.9";
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
  f2.visualColorHex = "#445566";
  scene.fixtures[f2.uuid] = f2;

  Fixture f3;
  f3.uuid = "fx-3";
  f3.instanceName = "Floor Wash";
  f3.gdtfSpec = (tempDir / "A" / "Same.gdtf").generic_string();
  f3.address = "6.121";
  f3.mvrFixtureColorHex = "#00FF00";
  f3.weightKg = 12.5f;
  f3.powerConsumptionW = 450.0f;
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
  fAt.gdtfSpec = "@PerastageFixture.gdtf";
  fAt.address = "21.1";
  scene.fixtures[fAt.uuid] = fAt;

  Fixture fSiblingResolved;
  fSiblingResolved.uuid = "fx-sibling-resolved";
  fSiblingResolved.instanceName = "Sibling Resolved";
  fSiblingResolved.typeName = "SiblingOnly";
  fSiblingResolved.gdtfSpec = "missing_import_folder/SiblingOnly.gdtf";
  fSiblingResolved.gdtfMode = "Default";
  fSiblingResolved.address = "22.1";
  scene.fixtures[fSiblingResolved.uuid] = fSiblingResolved;

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
  unitExistingMissingA.gdtfSpec =
      (tempDir / "A" / "Same.gdtf").generic_string();
  unitExistingMissingA.transform.o = {0.0f, -10.0f, 0.0f};
  unitExistingMissingA.address = "15.1";
  scene.fixtures[unitExistingMissingA.uuid] = unitExistingMissingA;

  Fixture unitExistingMissingB;
  unitExistingMissingB.uuid = "unit-existing-missing-b";
  unitExistingMissingB.instanceName = "Existing Unit Missing B";
  unitExistingMissingB.typeName = "Existing Unit Type";
  unitExistingMissingB.gdtfSpec =
      (tempDir / "A" / "Same.gdtf").generic_string();
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
  supportGeometry.modelFile =
      (tempDir / "models" / "support_model.3ds").string();
  supportGeometry.localTransform = MatrixUtils::Identity();
  sup.geometries.push_back(supportGeometry);
  scene.supports[sup.uuid] = sup;

  SceneObject primitiveSphere;
  primitiveSphere.uuid = "40000000-0000-4000-8000-000000000010";
  primitiveSphere.name = "Sphere";
  primitiveSphere.modelFile = "primitive:sphere";
  scene.sceneObjects[primitiveSphere.uuid] = primitiveSphere;

  SceneObject primitivePipe;
  primitivePipe.uuid = "40000000-0000-4000-8000-000000000011";
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
  primitivePipeBaked.uuid = "40000000-0000-4000-8000-000000000012";
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

  SceneObject malformedSceneObject;
  malformedSceneObject.uuid = "obj-invalid-recovery";
  malformedSceneObject.name = "Malformed SceneObject Recovery";
  malformedSceneObject.modelFile = "primitive:cube";
  scene.sceneObjects[malformedSceneObject.uuid] = malformedSceneObject;

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
  std::unordered_set<std::string> foldedEntries;
  for (const std::string &entryName : entries) {
    std::string lowerEntry = entryName;
    std::transform(
        lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    assert(foldedEntries.insert(lowerEntry).second);
  }
  assert(entries.count("Acme@SiblingOnly@Perastage.gdtf") == 1);
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
  assert(root->FirstChildElement("UserData") == nullptr ||
         root->FirstChildElement("UserData")
                 ->FirstChildElement("Data")
                 ->FirstChildElement("ProjectFixtureMetadataMap") == nullptr);
  const std::string caseAReference = FindFixtureGdtfSpec(root, "Case A");
  const std::string caseBReference = FindFixtureGdtfSpec(root, "Case B");
  assert(!caseAReference.empty());
  assert(!caseBReference.empty());
  assert(caseAReference != caseBReference);
  assert(caseAReference.find_first_of("/\\:") == std::string::npos);
  assert(caseBReference.find_first_of("/\\:") == std::string::npos);
  std::string foldedCaseA = caseAReference;
  std::string foldedCaseB = caseBReference;
  std::transform(foldedCaseA.begin(), foldedCaseA.end(), foldedCaseA.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  std::transform(foldedCaseB.begin(), foldedCaseB.end(), foldedCaseB.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  assert(foldedCaseA != foldedCaseB);
  assert(mvrGeometryEntries.count(caseAReference) == 1);
  assert(mvrGeometryEntries.count(caseBReference) == 1);
  assert(ReadGdtfFixtureIdentity(mvrGeometryEntries.at(caseAReference)) ==
         std::make_pair(std::string("Case A"),
                        std::string("cccccccc-cccc-4ccc-8ccc-ccccccccccc3")));
  assert(ReadGdtfFixtureIdentity(mvrGeometryEntries.at(caseBReference)) ==
         std::make_pair(std::string("Case B"),
                        std::string("dddddddd-dddd-4ddd-8ddd-ddddddddddd4")));
  const std::string atReference = FindFixtureGdtfSpec(root, "At Fixture");
  assert(!atReference.empty());
  assert(mvrGeometryEntries.count(atReference) == 1);
  assert(ReadGdtfFixtureIdentity(mvrGeometryEntries.at(atReference)) ==
         std::make_pair(std::string("At Fixture"),
                        std::string("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeee5")));
  assert(root->IntAttribute("verMajor") == 1);
  assert(root->IntAttribute("verMinor") == 6);
  assert(std::string(root->Attribute("provider")) == "Perastage");
  assert(std::string(root->Attribute("providerVersion")) == perastage::build_info::appVersion());
  assert(std::string(root->Attribute("providerVersion")) != "1.0" ||
         std::string(perastage::build_info::appVersion()) == "1.0");

  assert(ReadFixtureUnitNumber(root, "Unit Order Bottom") == 1);
  assert(ReadFixtureUnitNumber(root, "Unit Order Left") == 2);
  assert(ReadFixtureUnitNumber(root, "Unit Order Right") == 3);
  assert(ReadFixtureUnitNumber(root, "Existing Unit One") == 1);
  assert(ReadFixtureUnitNumber(root, "Existing Unit Four") == 4);
  assert(ReadFixtureUnitNumber(root, "Existing Unit Missing A") == 2);
  assert(ReadFixtureUnitNumber(root, "Existing Unit Missing B") == 3);
  assert(ReadFixtureUnitNumber(root, "Independent Unit Type") == 1);
  assert(ReadFixtureUnitNumber(root, "Unit Same As Fixture ID") == 77);

  MvrExporter deterministicExporter;
  fs::path deterministicMvrPath = tempDir / "Test1_repeat.mvr";
  assert(deterministicExporter.ExportToFile(
      deterministicMvrPath.generic_string()));
  const auto deterministicEntries =
      ReadArchiveTextEntries(deterministicMvrPath);
  auto deterministicXmlIt =
      deterministicEntries.find("GeneralSceneDescription.xml");
  assert(deterministicXmlIt != deterministicEntries.end());
  tinyxml2::XMLDocument deterministicDoc;
  assert(deterministicDoc.Parse(deterministicXmlIt->second.c_str()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *deterministicRoot =
      deterministicDoc.FirstChildElement("GeneralSceneDescription");
  assert(deterministicRoot != nullptr);
  assert(ReadFixtureUnitNumber(deterministicRoot, "Unit Order Bottom") == 1);
  assert(ReadFixtureUnitNumber(deterministicRoot, "Unit Order Left") == 2);
  assert(ReadFixtureUnitNumber(deterministicRoot, "Unit Order Right") == 3);
  assert(ReadFixtureUnitNumber(deterministicRoot, "Existing Unit Missing A") ==
         2);
  assert(ReadFixtureUnitNumber(deterministicRoot, "Existing Unit Missing B") ==
         3);
  assert(FindFixtureGdtfSpec(deterministicRoot, "Case A") == caseAReference);
  assert(FindFixtureGdtfSpec(deterministicRoot, "Case B") == caseBReference);
  assert(deterministicEntries.count(caseAReference) == 1);
  assert(deterministicEntries.count(caseBReference) == 1);

  tinyxml2::XMLElement *recoveredObject = FindSceneObjectElementByName(
      root, malformedSceneObject.name);
  tinyxml2::XMLElement *repeatedRecoveredObject = FindSceneObjectElementByName(
      deterministicRoot, malformedSceneObject.name);
  assert(recoveredObject != nullptr);
  assert(repeatedRecoveredObject != nullptr);
  const char *recoveredUuidAttribute = recoveredObject->Attribute("uuid");
  const char *repeatedRecoveredUuidAttribute =
      repeatedRecoveredObject->Attribute("uuid");
  assert(recoveredUuidAttribute != nullptr);
  assert(repeatedRecoveredUuidAttribute != nullptr);
  const std::string recoveredSceneObjectUuid = recoveredUuidAttribute;
  assert(CanonicalizeUuid(recoveredSceneObjectUuid) ==
         recoveredSceneObjectUuid);
  assert(recoveredSceneObjectUuid != malformedSceneObject.uuid);
  assert(recoveredSceneObjectUuid == repeatedRecoveredUuidAttribute);
  assert(xml.find("uuid=\"" + malformedSceneObject.uuid + "\"") ==
         std::string::npos);
  auto *recoveredGeometries =
      recoveredObject->FirstChildElement("Geometries");
  auto *repeatedRecoveredGeometries =
      repeatedRecoveredObject->FirstChildElement("Geometries");
  assert(recoveredGeometries != nullptr);
  assert(repeatedRecoveredGeometries != nullptr);
  auto *recoveredGeometry =
      recoveredGeometries->FirstChildElement("Geometry3D");
  auto *repeatedRecoveredGeometry =
      repeatedRecoveredGeometries->FirstChildElement("Geometry3D");
  assert(recoveredGeometry != nullptr);
  assert(repeatedRecoveredGeometry != nullptr);
  const char *recoveredFileName = recoveredGeometry->Attribute("fileName");
  const char *repeatedRecoveredFileName =
      repeatedRecoveredGeometry->Attribute("fileName");
  assert(recoveredFileName != nullptr);
  assert(repeatedRecoveredFileName != nullptr);
  assert(std::string(recoveredFileName).find("cube") != std::string::npos);
  assert(std::string(recoveredFileName) == repeatedRecoveredFileName);
  assert(mvrGeometryEntries.count(recoveredFileName) == 1);
  assert(deterministicEntries.count(repeatedRecoveredFileName) == 1);

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
  int sceneObjectCount = 0;
  int sceneObjectsWithMatrixBeforeGeometries = 0;
  std::unordered_set<std::string> exportedSceneObjectUuids;
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
          assert(idNode && idNode->GetText() &&
                 std::string(idNode->GetText()).size() > 0);
          assert(numNode && numNode->GetText());
          std::string fixtureIdText = idNode->GetText();
          int value = std::stoi(numNode->GetText());
          assert(value > 0);
          assert(numericIds.insert(value).second);

          if (std::string(cur->Name()) == "Fixture") {
            const char *uuidAttr = cur->Attribute("uuid");
            const char *nameAttr = cur->Attribute("name");
            assert(uuidAttr != nullptr);
            assert(nameAttr != nullptr);
            std::string fixtureUuid = uuidAttr;
            std::string fixtureNodeName = nameAttr;
            assert(!fixtureNodeName.empty());
            assert(fixtureNodeName != "Fixture_" + fixtureUuid);

            auto *colorNode = cur->FirstChildElement("Color");
            if (fixtureNodeName == f2.instanceName) {
              assert(colorNode == nullptr);
              sawFixtureVisualizationColorWithoutMvrColor = true;
            }
            if (fixtureNodeName == f3.instanceName) {
              assert(colorNode != nullptr);
              assert(colorNode->GetText() != nullptr);
              assert(std::string(colorNode->GetText()) ==
                     "0.300000,0.600000,0.715200");
              sawFixtureGelColor = true;
            }

            assert(cur->FirstChildElement("Weight") == nullptr);
            assert(cur->FirstChildElement("PowerConsumption") == nullptr);

            if (fixtureNodeName == fEditedId.instanceName) {
              assert(fixtureIdText == "909");
              assert(value == 909);
              sawEditedFixtureId = true;
            }

            auto *unitNode = cur->FirstChildElement("UnitNumber");
            assert(unitNode != nullptr && unitNode->GetText() != nullptr);
            int unitValue = std::stoi(unitNode->GetText());
            assert(unitValue > 0);

            auto *ud = cur->FirstChildElement("UserData");
            assert(ud == nullptr);

            auto *addresses = cur->FirstChildElement("Addresses");
            assert(addresses != nullptr);
            auto *addr = addresses->FirstChildElement("Address");
            assert(addr != nullptr);
            assert(addr->IntAttribute("break", -1) == 0);
            assert(addr->GetText() != nullptr);
            const std::string addressText = addr->GetText();
            assert(!addressText.empty());
            assert(std::all_of(
                addressText.begin(), addressText.end(),
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
            const char *trussUuidAttr = cur->Attribute("uuid");
            assert(trussUuidAttr != nullptr);
            auto *geometries = cur->FirstChildElement("Geometries");
            assert(geometries != nullptr);
            assert(geometries->FirstChildElement("Geometry3D") != nullptr);
            ++mvrGeometryTrussesWithGeometry3d;

            if (gdtfSpec != nullptr && gdtfSpec->GetText() != nullptr) {
              const std::string trussGdtfSpec = gdtfSpec->GetText();
              trussGdtfByUuid[trussUuidAttr] = trussGdtfSpec;
              assert(mvrGeometryEntries.count(trussGdtfSpec) == 1);
              const fs::path trussGdtfPath = tempDir / trussGdtfSpec;
              std::ofstream gdtfOut(trussGdtfPath, std::ios::binary);
              assert(gdtfOut.is_open());
              gdtfOut << mvrGeometryEntries.at(trussGdtfSpec);
              gdtfOut.close();
              const bool hasRenderableGdtf =
                  GdtfHasRenderable3DModel(trussGdtfPath);
              assert(hasRenderableGdtf);
              if (hasRenderableGdtf)
                ++mvrGeometryTrussesWithRenderableGdtf;
            }

            const char *trussUuid = cur->Attribute("uuid");
            assert(trussUuid != nullptr);
            assert(CanonicalizeUuid(trussUuid) == std::string(trussUuid));
            assert(cur->FirstChildElement("UserData") == nullptr);
            if (std::string(trussUuid) == CanonicalizeUuid(trCanonical.uuid)) {
              sawCanonicalTrussUuidUnchanged = true;
              sawCanonicalTrussStandardChildren =
                  cur->FirstChildElement("FixtureID") != nullptr &&
                  cur->FirstChildElement("FixtureIDNumeric") != nullptr &&
                  cur->FirstChildElement("UnitNumber") != nullptr &&
                  cur->FirstChildElement("CustomId") != nullptr &&
                  cur->FirstChildElement("CustomIdType") != nullptr;
            }
            const char *trussName = cur->Attribute("name");
            if (trussName != nullptr &&
                std::string(trussName) == trNonNumeric.name) {
              sawInvalidTrussUuidRepaired =
                  std::string(trussUuid) != trNonNumeric.uuid;
              repairedInvalidTrussUuid = trussUuid;
              sawNonNumericTrussNameFixtureIdConsistency =
                  fixtureIdText == std::to_string(value);
            }
          }

          if (auto *gdtf = cur->FirstChildElement("GDTFSpec");
              gdtf && gdtf->GetText()) {
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
        ++sceneObjectCount;
        assert(cur->FirstChildElement("UserData") == nullptr);
        int matrixOrder = -1;
        int geometriesOrder = -1;
        int childOrder = 0;
        for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
             child = child->NextSiblingElement(), ++childOrder) {
          const std::string childName = child->Name() ? child->Name() : "";
          if (childName == "Matrix" && matrixOrder < 0)
            matrixOrder = childOrder;
          if (childName == "Geometries" && geometriesOrder < 0)
            geometriesOrder = childOrder;
        }
        assert(matrixOrder >= 0);
        assert(geometriesOrder >= 0);
        assert(matrixOrder < geometriesOrder);
        ++sceneObjectsWithMatrixBeforeGeometries;
        const char *sceneObjectUuid = cur->Attribute("uuid");
        const char *sceneObjectName = cur->Attribute("name");
        assert(sceneObjectUuid != nullptr);
        assert(CanonicalizeUuid(sceneObjectUuid) ==
               std::string(sceneObjectUuid));
        assert(exportedSceneObjectUuids.insert(sceneObjectUuid).second);
        std::cerr << "Exported SceneObject name='"
                  << (sceneObjectName ? sceneObjectName : "") << "' uuid='"
                  << (sceneObjectUuid ? sceneObjectUuid : "") << "'\n";
        auto *geometries = cur->FirstChildElement("Geometries");
        if (sceneObjectUuid && geometries) {
          if (std::string(sceneObjectUuid) == primitiveSphere.uuid) {
            std::cerr << "Matched sphere UUID " << primitiveSphere.uuid
                      << "\n";
            auto *g3d = geometries->FirstChildElement("Geometry3D");
            assert(g3d != nullptr);
            const char *fileName = g3d->Attribute("fileName");
            assert(fileName != nullptr &&
                   std::string(fileName).find("sphere") != std::string::npos);
            std::cerr << "Sphere geometry fileName='" << fileName << "'\n";
            assert(mvrGeometryEntries.count(fileName) == 1);
            auto *geoMatrix = g3d->FirstChildElement("Matrix");
            assert(geoMatrix != nullptr && geoMatrix->GetText() != nullptr);
            Matrix parsedGeoMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(geoMatrix->GetText(),
                                            parsedGeoMatrix));
            std::cerr << "Sphere geometry matrix='" << geoMatrix->GetText()
                      << "'\n";
            const Matrix identity = MatrixUtils::Identity();
            assert(parsedGeoMatrix.u == identity.u);
            assert(parsedGeoMatrix.v == identity.v);
            assert(parsedGeoMatrix.w == identity.w);
            assert(parsedGeoMatrix.o == identity.o);
            sawPrimitiveSphereWithIdentityGeometryMatrix = true;
          }

          if (std::string(sceneObjectUuid) == primitivePipe.uuid) {
            auto *objMatrixNode = cur->FirstChildElement("Matrix");
            assert(objMatrixNode != nullptr &&
                   objMatrixNode->GetText() != nullptr);
            Matrix parsedObjMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(objMatrixNode->GetText(),
                                            parsedObjMatrix));
            std::cerr << "Pipe object matrix='" << objMatrixNode->GetText()
                      << "'\n";
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
            assert(MatrixUtils::ParseMatrix(geoMatrix->GetText(),
                                            parsedGeoMatrix));
            std::cerr << "Pipe geometry matrix='" << geoMatrix->GetText()
                      << "'\n";
            if (parsedGeoMatrix.u == primitivePipeGeo.localTransform.u &&
                parsedGeoMatrix.v == primitivePipeGeo.localTransform.v &&
                parsedGeoMatrix.w == primitivePipeGeo.localTransform.w &&
                parsedGeoMatrix.o == primitivePipeGeo.localTransform.o) {
              sawPrimitivePipeGeometryMatrixNormalized = true;
            }
          }

          if (std::string(sceneObjectUuid) == primitivePipeBaked.uuid) {
            auto *objMatrixNode = cur->FirstChildElement("Matrix");
            assert(objMatrixNode != nullptr &&
                   objMatrixNode->GetText() != nullptr);
            Matrix parsedObjMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(objMatrixNode->GetText(),
                                            parsedObjMatrix));
            std::cerr << "Baked pipe object matrix='"
                      << objMatrixNode->GetText() << "'\n";
            if (parsedObjMatrix.u == primitivePipeBaked.transform.u &&
                parsedObjMatrix.v == primitivePipeBaked.transform.v &&
                parsedObjMatrix.w == primitivePipeBaked.transform.w) {
              sawPrimitivePipeBakedMatrixCanonicalized = true;
            }
            if (parsedObjMatrix.o == primitivePipeBaked.transform.o) {
              sawPrimitivePipeBakedPositionPreserved = true;
            }
            auto *g3d = geometries->FirstChildElement("Geometry3D");
            assert(g3d != nullptr);
            auto *geoMatrix = g3d->FirstChildElement("Matrix");
            assert(geoMatrix != nullptr && geoMatrix->GetText() != nullptr);
            Matrix parsedGeoMatrix = MatrixUtils::Identity();
            assert(MatrixUtils::ParseMatrix(geoMatrix->GetText(),
                                            parsedGeoMatrix));
            std::cerr << "Baked pipe geometry matrix='"
                      << geoMatrix->GetText() << "'\n";
            const Matrix identity = MatrixUtils::Identity();
            assert(parsedGeoMatrix.u == identity.u);
            assert(parsedGeoMatrix.v == identity.v);
            assert(parsedGeoMatrix.w == identity.w);
            assert(parsedGeoMatrix.o == identity.o);
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
  assert(fixtureAddressCount == static_cast<int>(scene.fixtures.size()));
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
  std::cerr << "Expected primitive UUIDs: sphere=" << primitiveSphere.uuid
            << ", pipe=" << primitivePipe.uuid
            << ", baked pipe=" << primitivePipeBaked.uuid << "\n";
  assert(sawPrimitiveSphereWithIdentityGeometryMatrix);
  assert(sawPrimitivePipeObjectMatrixUnbaked);
  assert(sawPrimitivePipeGeometryMatrixNormalized);
  assert(sawPrimitivePipeBakedMatrixCanonicalized);
  assert(sawPrimitivePipePositionPreserved);
  assert(sawPrimitivePipeBakedPositionPreserved);
  assert(sceneObjectCount >= 3);
  assert(exportedSceneObjectUuids.count(recoveredSceneObjectUuid) == 1);
  assert(sceneObjectsWithMatrixBeforeGeometries == sceneObjectCount);
  assert(mvrGeometryTrussCount == static_cast<int>(scene.trusses.size()));
  assert(mvrGeometryTrussesWithGeometry3d == mvrGeometryTrussCount);
  assert(mvrGeometryTrussesWithRenderableGdtf ==
         static_cast<int>(trussGdtfByUuid.size()));

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
  for (tinyxml2::XMLElement *layerOrOther = layersNode->FirstChildElement();
       layerOrOther; layerOrOther = layerOrOther->NextSiblingElement()) {
    assert(std::string(layerOrOther->Name()) == "Layer");
    const char *layerName = layerOrOther->Attribute("name");
    const std::string layerNameText = layerName ? layerName : "";
    const char *layerUuid = layerOrOther->Attribute("uuid");
    assert(layerUuid != nullptr);
    assert(CanonicalizeUuid(layerUuid) == std::string(layerUuid));
    if (layerNameText == DEFAULT_LAYER_NAME)
      sawDefaultLayerNode = true;

    tinyxml2::XMLElement *childList =
        layerOrOther->FirstChildElement("ChildList");
    for (tinyxml2::XMLElement *child =
             childList ? childList->FirstChildElement() : nullptr;
         child; child = child->NextSiblingElement()) {
      if (std::string(child->Name()) != "SceneObject")
        continue;
      const char *uuidAttr = child->Attribute("uuid");
      if (uuidAttr != nullptr &&
          std::string(uuidAttr) == primitiveSphere.uuid &&
          layerNameText == DEFAULT_LAYER_NAME) {
        sawSphereInDefaultLayer = true;
      }
    }
  }
  assert(sawDefaultLayerNode);
  assert(sawSphereInDefaultLayer);
  tinyxml2::XMLElement *rootUserData = root->FirstChildElement("UserData");
  assert(rootUserData != nullptr);
  bool sawRootTrussInfoMap = false;
  bool sawRootPrimitiveGeometryMap = false;
  bool sawSpherePrimitiveMapEntry = false;
  bool sawPipePrimitiveMapEntry = false;
  bool sawRepairedTrussInfo = false;
  bool sawCanonicalTrussInfo = false;
  for (tinyxml2::XMLElement *data = rootUserData->FirstChildElement("Data");
       data; data = data->NextSiblingElement("Data")) {
    const char *provider = data->Attribute("provider");
    if (provider == nullptr || std::string(provider) != "Perastage")
      continue;
    for (tinyxml2::XMLElement *map =
             data->FirstChildElement("PrimitiveGeometryMap");
         map; map = map->NextSiblingElement("PrimitiveGeometryMap")) {
      sawRootPrimitiveGeometryMap = true;
      for (tinyxml2::XMLElement *entry = map->FirstChildElement("Entry");
           entry; entry = entry->NextSiblingElement("Entry")) {
        const char *sceneObjectUuid = entry->Attribute("sceneObjectUuid");
        const char *fileName = entry->Attribute("fileName");
        const char *modelRef = entry->Attribute("perastageModelRef");
        assert(sceneObjectUuid != nullptr);
        assert(fileName != nullptr);
        assert(modelRef != nullptr);
        assert(entry->Attribute("modelRef") == nullptr);
        if (std::string(sceneObjectUuid) == primitiveSphere.uuid &&
            std::string(modelRef) == primitiveSphere.modelFile)
          sawSpherePrimitiveMapEntry = true;
        if (std::string(sceneObjectUuid) == primitivePipe.uuid &&
            std::string(modelRef) == primitivePipeGeo.modelFile)
          sawPipePrimitiveMapEntry = true;
      }
    }
    for (tinyxml2::XMLElement *map = data->FirstChildElement("TrussInfoMap");
         map; map = map->NextSiblingElement("TrussInfoMap")) {
      sawRootTrussInfoMap = true;
      for (tinyxml2::XMLElement *info = map->FirstChildElement("TrussInfo");
           info; info = info->NextSiblingElement("TrussInfo")) {
        const char *uuid = info->Attribute("uuid");
        assert(uuid != nullptr);
        assert(CanonicalizeUuid(uuid) == std::string(uuid));
        if (std::string(uuid) == repairedInvalidTrussUuid)
          sawRepairedTrussInfo = true;
        if (std::string(uuid) == CanonicalizeUuid(trCanonical.uuid))
          sawCanonicalTrussInfo = true;
      }
    }
  }
  assert(sawRootTrussInfoMap);
  assert(sawRootPrimitiveGeometryMap);
  assert(sawSpherePrimitiveMapEntry);
  assert(sawPipePrimitiveMapEntry);
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
          cur->Attribute("name") != nullptr &&
          std::string(cur->Attribute("name")) == sup.name) {
        assert(false && "Support must not be exported as SceneObject");
      }
      if (std::string(cur->Name()) == "Support" &&
          cur->Attribute("name") != nullptr &&
          std::string(cur->Attribute("name")) == sup.name) {
        supportNode = cur;
      }
      for (tinyxml2::XMLElement *child = cur->FirstChildElement(); child;
           child = child->NextSiblingElement()) {
        stack.push_back(child);
      }
    }
  }
  assert(supportNode != nullptr);
  assert(supportNode->Attribute("uuid") != nullptr);
  const std::string recoveredSupportUuid = supportNode->Attribute("uuid");
  assert(CanonicalizeUuid(recoveredSupportUuid) == recoveredSupportUuid);
  assert(supportNode->FirstChildElement("Geometries") != nullptr);
  assert(supportNode->FirstChildElement("UserData") == nullptr);
  assert(supportNode->FirstChildElement("SupportInfo") == nullptr);
  assert(supportNode->FirstChildElement("HoistInfo") == nullptr);
  auto *supportPositionNode = supportNode->FirstChildElement("Position");
  assert(supportPositionNode != nullptr &&
         supportPositionNode->GetText() != nullptr);
  assert(CanonicalizeUuid(supportPositionNode->GetText()) ==
         std::string(supportPositionNode->GetText()));
  auto *rootHoistInfoMap = rootUserData->FirstChildElement("Data")->FirstChildElement("HoistInfoMap");
  assert(rootHoistInfoMap != nullptr);
  tinyxml2::XMLElement *hoistInfo = nullptr;
  for (auto *info = rootHoistInfoMap->FirstChildElement("HoistInfo"); info;
       info = info->NextSiblingElement("HoistInfo")) {
    if (info->Attribute("uuid") != nullptr &&
        std::string(info->Attribute("uuid")) == recoveredSupportUuid)
      hoistInfo = info;
  }
  assert(hoistInfo != nullptr);
  auto *capacityNode = hoistInfo->FirstChildElement("Capacity");
  assert(capacityNode != nullptr && capacityNode->GetText() != nullptr &&
         std::string(capacityNode->GetText()) == "1000.000000");

  std::unordered_map<std::string, std::string> fixtureTypeIdByTrussUuid;
  for (const auto &[trussUuid, gdtfSpec] : trussGdtfByUuid) {
    assert(mvrGeometryEntries.count(gdtfSpec) == 1);
    const fs::path trussGdtfPath = tempDir / ("ftid_" + trussUuid + ".gdtf");
    std::ofstream gdtfOut(trussGdtfPath, std::ios::binary);
    assert(gdtfOut.is_open());
    gdtfOut << mvrGeometryEntries.at(gdtfSpec);
    gdtfOut.close();
    AssertGeneratedGdtfVersioning(trussGdtfPath);
    fixtureTypeIdByTrussUuid[trussUuid] =
        ReadFixtureTypeIdFromGdtf(trussGdtfPath);
  }
  std::unordered_map<std::string, std::string> trussUuidByName;
  for (const auto &[uuid, exportedTruss] : scene.trusses)
    trussUuidByName[exportedTruss.name] = uuid;
  if (!fixtureTypeIdByTrussUuid.empty()) {
    assert(fixtureTypeIdByTrussUuid.at(trussUuidByName.at(tr.name)) ==
           fixtureTypeIdByTrussUuid.at(
               trussUuidByName.at(trNonNumeric.name)));
  }

  cfg.SetFloat("mvr_truss_geometry_authority", 1.0f);
  fs::path mvrPathGdtfAuthority = tempDir / "Test2_GdtfAuthority.mvr";
  assert(exporter.ExportToFile(mvrPathGdtfAuthority.generic_string()));

  const auto gdtfAuthorityEntries =
      ReadArchiveTextEntries(mvrPathGdtfAuthority);
  auto xmlGdtfAuthorityIt =
      gdtfAuthorityEntries.find("GeneralSceneDescription.xml");
  assert(xmlGdtfAuthorityIt != gdtfAuthorityEntries.end());
  std::string xmlGdtfAuthority = xmlGdtfAuthorityIt->second;

  assert(!xmlGdtfAuthority.empty());
  tinyxml2::XMLDocument docGdtfAuthority;
  assert(docGdtfAuthority.Parse(xmlGdtfAuthority.c_str()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *rootGdtfAuthority =
      docGdtfAuthority.FirstChildElement("GeneralSceneDescription");
  assert(rootGdtfAuthority != nullptr);

  bool sawTrussWithGdtfSpec = false;
  int gdtfAuthorityTrussCount = 0;
  int gdtfAuthorityTrussesWithGeometry3d = 0;
  int gdtfAuthorityTrussesWithRenderableGdtf = 0;
  for (tinyxml2::XMLElement *node = rootGdtfAuthority->FirstChildElement();
       node; node = node->NextSiblingElement()) {
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
        assert(geometries == nullptr ||
               geometries->FirstChildElement("Geometry3D") == nullptr);
        if (geometries != nullptr &&
            geometries->FirstChildElement("Geometry3D") != nullptr)
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

  MvrImporter primitiveImporter;
  assert(primitiveImporter.ImportFromFile(mvrPath.generic_string(), false,
                                          false));
  const MvrScene &roundtripScene = ConfigManager::Get().GetScene();
  bool sawRoundtripPrimitiveSphere = false;
  bool sawRoundtripPrimitivePipe = false;
  bool sawRoundtripRecoveredSceneObject = false;
  for (const auto &[uuid, obj] : roundtripScene.sceneObjects) {
    if (obj.name == primitiveSphere.name &&
        obj.modelFile == primitiveSphere.modelFile)
      sawRoundtripPrimitiveSphere = true;
    if (obj.name == primitivePipe.name && !obj.geometries.empty() &&
        obj.geometries.front().modelFile == primitivePipeGeo.modelFile)
      sawRoundtripPrimitivePipe = true;
    if (obj.name == malformedSceneObject.name) {
      assert(uuid == recoveredSceneObjectUuid);
      assert(obj.uuid == recoveredSceneObjectUuid);
      assert(obj.modelFile == malformedSceneObject.modelFile);
      sawRoundtripRecoveredSceneObject = true;
    }
  }
  assert(sawRoundtripPrimitiveSphere);
  assert(sawRoundtripPrimitivePipe);
  assert(sawRoundtripRecoveredSceneObject);

  MvrExporter roundtripExporter;
  const fs::path roundtripMvrPath = tempDir / "Test1_roundtrip.mvr";
  assert(roundtripExporter.ExportToFile(roundtripMvrPath.generic_string()));
  const auto roundtripEntries = ReadArchiveTextEntries(roundtripMvrPath);
  const auto roundtripXmlIt =
      roundtripEntries.find("GeneralSceneDescription.xml");
  assert(roundtripXmlIt != roundtripEntries.end());
  tinyxml2::XMLDocument roundtripDoc;
  assert(roundtripDoc.Parse(roundtripXmlIt->second.c_str()) ==
         tinyxml2::XML_SUCCESS);
  tinyxml2::XMLElement *roundtripRoot =
      roundtripDoc.FirstChildElement("GeneralSceneDescription");
  assert(roundtripRoot != nullptr);
  tinyxml2::XMLElement *roundtripRecoveredObject =
      FindSceneObjectElementByName(roundtripRoot, malformedSceneObject.name);
  assert(roundtripRecoveredObject != nullptr);
  assert(roundtripRecoveredObject->Attribute("uuid") != nullptr);
  assert(recoveredSceneObjectUuid ==
         roundtripRecoveredObject->Attribute("uuid"));

  const fs::path legacyPrimitiveMvrPath = tempDir / "legacy_primitive.mvr";
  const std::string legacyPrimitiveUuid =
      "22222222-2222-4222-8222-222222222222";
  {
    wxFileOutputStream legacyPrimitiveOut(
        legacyPrimitiveMvrPath.generic_string());
    assert(legacyPrimitiveOut.IsOk());
    wxZipOutputStream legacyPrimitiveZip(legacyPrimitiveOut);
    const std::string legacyPrimitiveXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
        "provider=\"Perastage\" providerVersion=\"" +
        std::string(perastage::build_info::appVersion()) +
        "\"><Scene><Layers><Layer name=\"Default\"><ChildList>"
        "<SceneObject uuid=\"" +
        legacyPrimitiveUuid +
        "\" name=\"Legacy Primitive\">"
        "<Geometries><Geometry3D fileName=\"primitive_cube.glb\">"
        "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
        "</Geometry3D></Geometries>"
        "<UserData><Data provider=\"Perastage\">"
        "<PrimitiveGeometryMap><Entry fileName=\"PRIMITIVE_CUBE.GLB\" "
        "perastageModelRef=\"primitive:cube\"/>"
        "</PrimitiveGeometryMap></Data></UserData>"
        "<Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix>"
        "</SceneObject></ChildList></Layer></Layers></Scene>"
        "</GeneralSceneDescription>";
    assert(legacyPrimitiveZip.PutNextEntry("GeneralSceneDescription.xml"));
    legacyPrimitiveZip.Write(legacyPrimitiveXml.data(),
                             legacyPrimitiveXml.size());
    legacyPrimitiveZip.Close();
  }

  MvrImporter legacyPrimitiveImporter;
  assert(legacyPrimitiveImporter.ImportFromFile(
      legacyPrimitiveMvrPath.generic_string(), false, false));
  const MvrScene &legacyPrimitiveScene = ConfigManager::Get().GetScene();
  assert(legacyPrimitiveScene.sceneObjects.count(legacyPrimitiveUuid) == 1);
  const SceneObject &legacyPrimitive =
      legacyPrimitiveScene.sceneObjects.at(legacyPrimitiveUuid);
  assert(!legacyPrimitive.geometries.empty());
  assert(legacyPrimitive.geometries.front().modelFile == "primitive:cube");

  const fs::path legacyMvrPath = tempDir / "legacy_positions.mvr";
  {
    wxFileOutputStream legacyOut(legacyMvrPath.generic_string());
    assert(legacyOut.IsOk());
    wxZipOutputStream legacyZip(legacyOut);
    const std::string legacyXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\" "
        "provider=\"Perastage\" providerVersion=\"" +
        std::string(perastage::build_info::appVersion()) +
        "\">"
        "<UserData><Data provider=\"Perastage\" ver=\"" +
        std::string(kPerastageUserDataSchemaVersion) +
        "\"><TrussSidecarManifest/></Data></UserData>"
        "<Scene>"
        "<AUXData><Position uuid=\"LX1\" name=\"LX 1\"/></AUXData>"
        "<Layers><Layer name=\"Default\"><ChildList>"
        "<Fixture uuid=\"fixture-legacy\" name=\"\">"
        "<FixtureID>1</FixtureID><FixtureIDNumeric>1</FixtureIDNumeric>"
        "<GDTFSpec>fixture.gdtf</GDTFSpec><Position>LX1</Position>"
        "<Color>0.300000,0.600000,0.715200</Color>"
        "<UserData><Data provider=\"Perastage\" ver=\"1.0\"><FixtureInfo>"
        "<InstanceName>Legacy Fixture Name</InstanceName>"
        "<StableId>11111111-1111-4111-8111-111111111111</StableId>"
        "<Script>LegacyScriptName</Script>"
        "</FixtureInfo></Data></UserData>"
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
  assert(importedScene.fixtures.count("11111111-1111-4111-8111-111111111111") ==
         1);
  assert(legacyFixture.uuid == "11111111-1111-4111-8111-111111111111");
  assert(legacyFixture.instanceName == "Legacy Fixture Name");
  assert(!legacyFixture.position.empty());
  assert(legacyFixture.position != "LX1");
  assert(CanonicalizeUuid(legacyFixture.position) == legacyFixture.position);
  assert(importedScene.positions.count(legacyFixture.position) == 1);
  assert(legacyFixture.visualColorHex.empty());
  assert(!legacyFixture.mvrFixtureColorHex.empty());

  fs::remove_all(tempDir);
  return 0;
}
