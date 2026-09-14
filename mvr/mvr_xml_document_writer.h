#pragma once

#include <string>
#include <unordered_map>

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
}

namespace mvr_xml_serialization {

// Creates the MVR declaration and GeneralSceneDescription root.
tinyxml2::XMLElement *CreateDocument(tinyxml2::XMLDocument &document,
                                    const std::string &providerVersion);

// Creates AUXData and appends prepared Position nodes without inserting it.
tinyxml2::XMLElement *AppendPreparedPositions(
    tinyxml2::XMLDocument &document,
    const std::unordered_map<std::string, std::string> &positions);

} // namespace mvr_xml_serialization
