/* This file is part of Perastage. */
#include "mvr_class_preservation.h"
#include "mvr_standard_preservation.h"
#include <tinyxml2.h>
#include <cassert>

constexpr auto classId = "a0000000-0000-4000-8000-000000000001";
constexpr auto objectId = "b0000000-0000-4000-8000-000000000001";

static MvrScene Scan(const std::string &classes, const std::string &objects,
                     const std::string &userData = {}) {
  const std::string xml = "<GeneralSceneDescription>" + userData +
      "<Scene><AUXData>" + classes + "</AUXData><Layers><Layer><ChildList>" + objects +
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  MvrScene scene;
  mvr::RecordStandardPreservationRisks(*doc.RootElement(), scene);
  return scene;
}

int main() {
  const auto definition = "<Class uuid='" + std::string(classId) + "' name='  Exact  '/>";
  const auto assignment = "<Classing>" + std::string(classId) + "</Classing>";
  const auto object = "<SceneObject uuid='" + std::string(objectId) + "'>" + assignment + "</SceneObject>";
  auto safe = Scan(definition, object);
  assert(safe.preservationIssues.empty());
  assert(Scan("<Class uuid='" + std::string(classId) + "'> \n </Class>", object).preservationIssues.empty());
  assert(safe.classes.at(classId).name == "  Exact  ");
  for (const auto &scene : {
      Scan("<Class/>", {}),
      Scan("<Class uuid='00000000-0000-0000-0000-000000000000'/>", {}),
      Scan("<Class uuid='invalid'/>", {}),
      Scan(definition + definition, {}),
      Scan(definition, "<SceneObject uuid='" + std::string(classId) + "'/>"),
      Scan(definition, "<Symbol uuid='" + std::string(classId) + "'/>"),
      Scan(definition + "<Class uuid='A0000000-0000-4000-8000-000000000001'/>", {}),
      Scan("<Class uuid='" + std::string(classId) + "' future='lost'/>", {}),
      Scan("<Class uuid='" + std::string(classId) + "'><Keep/></Class>", {}),
      Scan({}, object),
      Scan(definition, object + object),
      Scan(definition, "<SceneObject>" + assignment + "</SceneObject>"),
      Scan(definition, "<SceneObject uuid='" + std::string(objectId) + "'><Classing/></SceneObject>"),
      Scan(definition, "<SceneObject uuid='" + std::string(objectId) + "'>" + assignment + assignment + "</SceneObject>"),
      Scan(definition, "<SceneObject uuid='" + std::string(objectId) + "'><Classing future='lost'>" + classId + "</Classing></SceneObject>"),
      Scan(definition, "<SceneObject uuid='" + std::string(objectId) + "'><Classing>" + classId + "<Keep/></Classing></SceneObject>")}) {
    assert(!scene.preservationIssues.empty());
    assert(!mvr::CheckStandardPreservation(scene).empty());
    assert(!scene.preservationIssues.front().sourceLocation.empty());
    assert(!scene.preservationIssues.front().originalInformation.empty());
  }
  // Opaque provider payloads may repeat UUIDs and names in their own namespace.
  assert(Scan(definition, object, "<UserData><Data provider='Foreign'><Class uuid='" +
      std::string(classId) + "'/><SceneObject>" + assignment + "</SceneObject></Data></UserData>").preservationIssues.empty());
  const auto malformed = Scan("<Class uuid='invalid'/>", {});
  assert(malformed.preservationIssues.front().objectUuid == "invalid");
  auto model = safe;
  SceneObject instance;
  instance.uuid = objectId;
  instance.classing = classId;
  model.sceneObjects.emplace(objectId, instance);
  assert(mvr::CheckMvrClasses(model).empty());
  model.classes.erase(classId);
  assert(!mvr::CheckMvrClasses(model).empty());
  model = safe;
  model.classes.emplace("00000000-0000-0000-0000-000000000000", MvrClass{});
  assert(!mvr::CheckMvrClasses(model).empty());
  model = safe;
  model.positions.emplace(classId, "Cross-kind conflict");
  assert(!mvr::CheckMvrClasses(model).empty());
  model = safe;
  instance.uuid = classId;
  instance.classing.clear();
  model.sceneObjects.emplace(classId, instance);
  assert(!mvr::CheckMvrClasses(model).empty());
  model = safe;
  instance.uuid = objectId;
  instance.classing = classId;
  model.sceneObjects.emplace(objectId, instance);
  tinyxml2::XMLDocument serialized;
  const auto xml = "<Scene><AUXData>" + definition + "</AUXData><Layers>" + object + "</Layers></Scene>";
  assert(serialized.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);
  assert(mvr::CheckSerializedClasses(*serialized.RootElement(), model).empty());
  auto *instanceNode = serialized.RootElement()->FirstChildElement("Layers")->FirstChildElement();
  instanceNode->InsertEndChild(instanceNode->FirstChildElement()->DeepClone(&serialized));
  assert(!mvr::CheckSerializedClasses(*serialized.RootElement(), model).empty());
  instanceNode->DeleteChild(instanceNode->LastChild());
  serialized.RootElement()->FirstChildElement("AUXData")->DeleteChildren();
  assert(!mvr::CheckSerializedClasses(*serialized.RootElement(), model).empty());
}
