#include "mvr_xml_document_writer.h"

#include <tinyxml2.h>

namespace mvr_xml_serialization {

// Creates the fixed MVR 1.6 document envelope in its established order.
tinyxml2::XMLElement *CreateDocument(tinyxml2::XMLDocument &document,
                                    const std::string &providerVersion) {
  document.InsertEndChild(
      document.NewDeclaration("xml version=\"1.0\" encoding=\"UTF-8\""));
  tinyxml2::XMLElement *root =
      document.NewElement("GeneralSceneDescription");
  root->SetAttribute("verMajor", 1);
  root->SetAttribute("verMinor", 6);
  root->SetAttribute("provider", "Perastage");
  root->SetAttribute("providerVersion", providerVersion.c_str());
  document.InsertEndChild(root);
  return root;
}

// Writes prepared Position UUID and name values without further normalization.
tinyxml2::XMLElement *AppendPreparedPositions(
    tinyxml2::XMLDocument &document,
    const std::unordered_map<std::string, std::string> &positions) {
  tinyxml2::XMLElement *aux = document.NewElement("AUXData");
  for (const auto &[uuid, name] : positions) {
    tinyxml2::XMLElement *position = document.NewElement("Position");
    position->SetAttribute("uuid", uuid.c_str());
    if (!name.empty())
      position->SetAttribute("name", name.c_str());
    aux->InsertEndChild(position);
  }
  return aux;
}

} // namespace mvr_xml_serialization
