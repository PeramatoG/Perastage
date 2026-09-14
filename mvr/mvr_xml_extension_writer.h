#pragma once

#include <string>
#include <unordered_map>

class MvrScene;
namespace tinyxml2 {
class XMLDocument;
class XMLElement;
}

namespace mvr_xml_extension {

// Finds or creates the root Perastage-owned Data element.
tinyxml2::XMLElement *FindOrCreateDataNode(tinyxml2::XMLDocument &document,
                                           tinyxml2::XMLElement *node);

// Reports whether the scene contains Perastage layer appearance metadata.
bool HasLayerAppearance(const MvrScene &scene);

// Appends Perastage layer appearance metadata using prepared layer UUIDs.
void AppendLayerAppearance(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const MvrScene &scene,
    const std::unordered_map<std::string, std::string> &layerUuids);

} // namespace mvr_xml_extension
