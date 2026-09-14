#include "mvr_xml_document_writer.h"
#include "mvr_xml_extension_writer.h"
#include "mvr_xml_scene_object_writer.h"

#include "matrixutils.h"

#include <tinyxml2.h>

#include <cassert>
#include <map>
#include <string>
#include <vector>

// Characterizes the extracted MVR document and Fixture writers.
int main() {
  tinyxml2::XMLDocument document;
  tinyxml2::XMLElement *root =
      mvr_xml_serialization::CreateDocument(document, "1.6.42");
  assert(document.FirstChild()->ToDeclaration());
  assert(std::string(document.FirstChild()->Value()) ==
         "xml version=\"1.0\" encoding=\"UTF-8\"");
  assert(std::string(root->Name()) == "GeneralSceneDescription");
  assert(root->IntAttribute("verMajor") == 1);
  assert(root->IntAttribute("verMinor") == 6);
  assert(std::string(root->Attribute("provider")) == "Perastage");

  tinyxml2::XMLElement *perastageData =
      mvr_xml_extension::FindOrCreateDataNode(document, root);
  assert(std::string(perastageData->Attribute("provider")) == "Perastage");
  assert(std::string(perastageData->Attribute("ver")) == "1.0");

  tinyxml2::XMLElement *scene =
      mvr_xml_serialization::AppendScene(document, root);
  tinyxml2::XMLElement *aux = mvr_xml_serialization::AppendPreparedPositions(
      document, {{"position-uuid", "Front"}});
  mvr_xml_serialization::SymdefValues symdef;
  symdef.uuid = "symdef-uuid";
  symdef.geometryType = "Model";
  symdef.geometries.push_back(
      {"resolved-symbol.glb", "Model", MatrixUtils::Identity()});
  mvr_xml_serialization::AppendSymdef(document, aux, symdef);
  scene->InsertEndChild(aux);
  const tinyxml2::XMLElement *symdefNode = aux->FirstChildElement("Symdef");
  assert(symdefNode);
  const tinyxml2::XMLElement *geometry =
      symdefNode->FirstChildElement("ChildList")
          ->FirstChildElement("Geometry3D");
  assert(std::string(geometry->Attribute("fileName")) == "resolved-symbol.glb");
  assert(geometry->FirstChildElement("Matrix"));

  tinyxml2::XMLElement *layers = mvr_xml_serialization::CreateLayers(document);
  tinyxml2::XMLElement *children =
      mvr_xml_serialization::CreateChildList(document);
  mvr_xml_serialization::FixtureValues fixture;
  fixture.uuid = "fixture-uuid";
  fixture.name = "Fixture 1";
  fixture.matrix = MatrixUtils::Identity();
  fixture.gdtfSpec = "Fixture.gdtf";
  fixture.gdtfMode = "Default";
  fixture.position = "position-uuid";
  fixture.fixtureId = "A-1";
  fixture.fixtureIdNumeric = 1;
  fixture.unitNumber = 7;
  fixture.absoluteDmxAddress = 513;
  fixture.color = "0.3127,0.3290,100.0000";
  mvr_xml_serialization::AppendFixture(document, children, fixture);
  mvr_xml_serialization::TrussValues truss;
  truss.object = {"truss-uuid", "Truss"};
  truss.matrix = MatrixUtils::Identity();
  truss.position = "position-uuid";
  truss.geometries.push_back(
      {mvr_xml_serialization::GeometryValues{"truss.glb", "Model",
                                             MatrixUtils::Identity()},
       std::nullopt});
  truss.function = "Structure";
  truss.gdtfSpec = "Truss.gdtf";
  truss.gdtfMode = "Default";
  truss.fixtureId = "2";
  truss.fixtureIdNumeric = 2;
  mvr_xml_serialization::AppendTruss(document, children, truss);

  mvr_xml_serialization::SupportValues support;
  support.object = {"support-uuid", "Support"};
  support.matrix = MatrixUtils::Identity();
  support.position = "position-uuid";
  support.geometries.push_back({"support.glb", {}, MatrixUtils::Identity()});
  support.function = "GroundSupport";
  support.chainLength = 1.5f;
  support.gdtfSpec = "Support.gdtf";
  support.gdtfMode = "Default";
  support.fixtureId = "3";
  support.fixtureIdNumeric = 3;
  mvr_xml_serialization::AppendSupport(document, children, support);

  mvr_xml_serialization::SceneObjectValues object;
  object.object = {"object-uuid", "Object"};
  object.matrix = MatrixUtils::Identity();
  object.geometries.push_back(
      {std::nullopt,
       mvr_xml_serialization::SymbolValues{"symbol-uuid", "symdef-uuid",
                                           MatrixUtils::Identity()}});
  object.fixtureId = "4";
  object.fixtureIdNumeric = 4;
  mvr_xml_serialization::AppendSceneObject(document, children, object);

  tinyxml2::XMLElement *groupChildren =
      mvr_xml_serialization::CreateChildList(document);
  mvr_xml_serialization::AppendGroupObject(
      document, children, {"group-uuid", "Group"}, MatrixUtils::Identity(),
      groupChildren);
  mvr_xml_serialization::AppendLayer(document, layers, {"layer-uuid", "Layer"},
                                     children);
  scene->InsertEndChild(layers);

  auto assertChildOrder = [](const tinyxml2::XMLElement *node,
                             const std::vector<std::string> &expected) {
    std::size_t childIndex = 0;
    for (const tinyxml2::XMLElement *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      assert(childIndex < expected.size());
      assert(expected[childIndex++] == child->Name());
    }
    assert(childIndex == expected.size());
  };
  assertChildOrder(children->FirstChildElement("Truss"),
                   {"Matrix", "Position", "Geometries", "Function", "GDTFSpec",
                    "GDTFMode", "FixtureID", "FixtureIDNumeric"});
  assertChildOrder(children->FirstChildElement("Support"),
                   {"Matrix", "Position", "Geometries", "Function",
                    "ChainLength", "GDTFSpec", "GDTFMode", "FixtureID",
                    "FixtureIDNumeric"});
  assertChildOrder(children->FirstChildElement("SceneObject"),
                   {"Matrix", "Geometries", "FixtureID", "FixtureIDNumeric"});
  const tinyxml2::XMLElement *group =
      children->FirstChildElement("GroupObject");
  assert(group && group->FirstChildElement("Matrix"));
  assert(!group->FirstChildElement("ChildList"));

  const tinyxml2::XMLElement *fixtureNode =
      children->FirstChildElement("Fixture");
  assert(fixtureNode);
  const std::vector<std::string> expectedChildren = {
      "Matrix",           "GDTFSpec",   "GDTFMode",  "Position", "FixtureID",
      "FixtureIDNumeric", "UnitNumber", "Addresses", "Color"};
  std::size_t index = 0;
  for (const tinyxml2::XMLElement *child = fixtureNode->FirstChildElement();
       child; child = child->NextSiblingElement()) {
    assert(index < expectedChildren.size());
    assert(expectedChildren[index++] == child->Name());
  }
  assert(index == expectedChildren.size());
  assert(fixtureNode->FirstChildElement("Addresses")
             ->FirstChildElement("Address")
             ->IntAttribute("break") == 0);

  mvr_xml_serialization::FixtureValues minimalFixture;
  minimalFixture.uuid = "minimal-fixture";
  minimalFixture.name = "Minimal";
  minimalFixture.matrix = MatrixUtils::Identity();
  mvr_xml_serialization::AppendFixture(document, children, minimalFixture);
  const tinyxml2::XMLElement *minimalNode =
      fixtureNode->NextSiblingElement("Fixture");
  assert(minimalNode->FirstChildElement("Matrix"));
  assert(!minimalNode->FirstChildElement("GDTFSpec"));
  assert(!minimalNode->FirstChildElement("GDTFMode"));

  std::map<std::string, mvr_xml_extension::FixtureTypeMetadata> fixtureTypes;
  fixtureTypes["type"] = {"type",  "Fixture.gdtf", "Default", "Maker",
                          "Model", "Wash",         "Manual",  "#112233"};
  mvr_xml_extension::AppendFixtureTypes(document, perastageData, fixtureTypes);
  const tinyxml2::XMLElement *typeInfo =
      perastageData->FirstChildElement("FixtureTypeInfoMap")
          ->FirstChildElement("FixtureTypeInfo");
  assert(std::string(typeInfo->FirstChildElement()->Name()) == "Category");

  std::vector<mvr_xml_extension::PrimitiveGeometryMetadata> primitiveEntries = {
      {"object-uuid", "primitive.glb", "primitive:cube", 0}};
  mvr_xml_extension::AppendPrimitiveGeometryMap(document, perastageData,
                                                primitiveEntries);
  assert(perastageData->FirstChildElement("PrimitiveGeometryMap"));

  tinyxml2::XMLElement *trussMap =
      mvr_xml_extension::CreateTrussInfoMap(document);
  mvr_xml_extension::TrussInfoMetadata trussInfo;
  trussInfo.uuid = "truss-uuid";
  trussInfo.manualLoadKg = 10.0f;
  trussInfo.manufacturer = "Maker";
  trussInfo.crossSectionType = "TrussFramework";
  mvr_xml_extension::AppendTrussInfo(document, trussMap, trussInfo);
  assert(std::string(trussMap->FirstChildElement()->Name()) == "TrussInfo");

  tinyxml2::XMLElement *hoistMap =
      mvr_xml_extension::CreateHoistInfoMap(document);
  mvr_xml_extension::HoistInfoMetadata hoistInfo;
  hoistInfo.uuid = "support-uuid";
  hoistInfo.capacityKg = 500.0f;
  hoistInfo.motorName = "Motor";
  mvr_xml_extension::AppendHoistInfo(document, hoistMap, hoistInfo);
  assert(std::string(hoistMap->FirstChildElement()->Name()) == "HoistInfo");
  assert(std::string(scene->FirstChildElement()->Name()) == "AUXData");
  assert(std::string(scene->LastChildElement()->Name()) == "Layers");
  assert(std::string(root->FirstChildElement()->Name()) == "UserData");
  assert(std::string(root->LastChildElement()->Name()) == "Scene");
  return 0;
}
