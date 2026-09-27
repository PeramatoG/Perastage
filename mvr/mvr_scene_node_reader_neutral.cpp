#include "mvr_scene_node_reader_neutral.h"

#include "mvr_scene_node_reader_detail.h"

#include <tinyxml2.h>

#include <algorithm>

namespace mvr::scene_reader_detail {
namespace {

// Appends non-empty source references without changing authored path text.
void AddNeutralReference(MvrNeutralSceneNode &node, const char *kind,
                         const char *value) {
  if (!value)
    return;
  const std::string trimmed = Trim(value);
  if (!trimmed.empty())
    node.resourceReferences.emplace_back(kind, trimmed);
}

// Collects deterministic file references owned by one neutral scene node.
void CollectNeutralReferences(tinyxml2::XMLElement *element,
                              MvrNeutralSceneNode &node,
                              bool insideMediaSources = false) {
  if (!element)
    return;
  const std::string name = element->Name() ? element->Name() : "";
  const bool mediaSources =
      insideMediaSources || name == "Sources" || name == "Projections";
  if (name == "GDTFSpec")
    AddNeutralReference(node, "gdtf", element->GetText());
  if (name == "Geometry3D")
    AddNeutralReference(node, "geometry", element->Attribute("fileName"));
  if (mediaSources && name == "Source") {
    const char *type = element->Attribute("type");
    if (type && ToLowerCopy(type) == "file")
      AddNeutralReference(node, "media", element->GetText());
  }
  for (tinyxml2::XMLElement *child = element->FirstChildElement(); child;
       child = child->NextSiblingElement())
    CollectNeutralReferences(child, node, mediaSources);
}

} // namespace

// Builds a neutral descriptor for a standard node unsupported by editing.
MvrNeutralSceneNode BuildNeutralSceneNode(tinyxml2::XMLElement *element,
                                          const std::string &kind,
                                          const std::string &layerUuid,
                                          const std::string &layerName,
                                          const std::string &parentGroupUuid) {
  MvrNeutralSceneNode node;
  node.kind = kind;
  if (const char *uuid = element->Attribute("uuid"))
    node.uuid = uuid;
  if (const char *name = element->Attribute("name"))
    node.name = name;
  node.layerUuid = layerUuid;
  node.layerName = layerName;
  node.parentGroupUuid = parentGroupUuid;
  CollectNeutralReferences(element, node);
  std::sort(node.resourceReferences.begin(), node.resourceReferences.end());
  node.resourceReferences.erase(std::unique(node.resourceReferences.begin(),
                                            node.resourceReferences.end()),
                                node.resourceReferences.end());
  return node;
}

} // namespace mvr::scene_reader_detail
