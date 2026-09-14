#include "mvr_xml_document_writer.h"
#include "mvr_xml_extension_writer.h"
#include "mvr_xml_scene_object_writer.h"

#include "matrixutils.h"

#include <tinyxml2.h>

#include <cassert>
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

  tinyxml2::XMLElement *scene = document.NewElement("Scene");
  root->InsertEndChild(scene);
  tinyxml2::XMLElement *aux = mvr_xml_serialization::AppendPreparedPositions(
      document, {{"position-uuid", "Front"}});
  scene->InsertEndChild(aux);

  tinyxml2::XMLElement *layers = document.NewElement("Layers");
  tinyxml2::XMLElement *layer = document.NewElement("Layer");
  tinyxml2::XMLElement *children = document.NewElement("ChildList");
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
  layer->InsertEndChild(children);
  layers->InsertEndChild(layer);
  scene->InsertEndChild(layers);

  const tinyxml2::XMLElement *fixtureNode = children->FirstChildElement("Fixture");
  assert(fixtureNode);
  const std::vector<std::string> expectedChildren = {
      "Matrix", "GDTFSpec", "GDTFMode", "Position", "FixtureID",
      "FixtureIDNumeric", "UnitNumber", "Addresses", "Color"};
  std::size_t index = 0;
  for (const tinyxml2::XMLElement *child = fixtureNode->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    assert(index < expectedChildren.size());
    assert(expectedChildren[index++] == child->Name());
  }
  assert(index == expectedChildren.size());
  assert(std::string(scene->FirstChildElement()->Name()) == "AUXData");
  assert(std::string(scene->LastChildElement()->Name()) == "Layers");
  assert(std::string(root->FirstChildElement()->Name()) == "UserData");
  assert(std::string(root->LastChildElement()->Name()) == "Scene");
  return 0;
}
