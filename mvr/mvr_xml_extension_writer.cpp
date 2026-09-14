#include "mvr_xml_extension_writer.h"

#include "mvrscene.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>

namespace {

// Trims ASCII whitespace from extension-owned identifier values.
std::string TrimAscii(std::string value) {
  auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(),
                           [&](unsigned char ch) { return !isSpace(ch); }));
  value.erase(std::find_if(value.rbegin(), value.rend(),
                           [&](unsigned char ch) { return !isSpace(ch); })
                  .base(),
              value.end());
  return value;
}

// Converts provider identifiers to lowercase for case-insensitive ownership checks.
std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return value;
}

// Finds the UserData container suitable for Perastage-owned data.
tinyxml2::XMLElement *FindPerastageUserData(tinyxml2::XMLElement *node) {
  if (!node)
    return nullptr;
  tinyxml2::XMLElement *first = node->FirstChildElement("UserData");
  for (tinyxml2::XMLElement *userData = first; userData;
       userData = userData->NextSiblingElement("UserData")) {
    for (tinyxml2::XMLElement *data = userData->FirstChildElement("Data"); data;
         data = data->NextSiblingElement("Data")) {
      const std::string provider = TrimAscii(
          data->Attribute("provider") ? data->Attribute("provider") : "");
      if (provider.empty() || ToLowerAscii(provider) == "perastage")
        return userData;
    }
  }
  return first;
}

// Validates a Perastage layer color metadata value.
bool IsLayerColor(const std::string &color) {
  return color.size() == 7 && color[0] == '#' &&
         std::all_of(color.begin() + 1, color.end(), [](unsigned char ch) {
           return std::isxdigit(ch) != 0;
         });
}

} // namespace

namespace mvr_xml_extension {

// Finds or creates the root Perastage-owned Data element.
tinyxml2::XMLElement *FindOrCreateDataNode(tinyxml2::XMLDocument &document,
                                           tinyxml2::XMLElement *node) {
  tinyxml2::XMLElement *userData = FindPerastageUserData(node);
  if (!userData) {
    userData = document.NewElement("UserData");
    node->InsertEndChild(userData);
  }
  for (tinyxml2::XMLElement *data = userData->FirstChildElement("Data"); data;
       data = data->NextSiblingElement("Data")) {
    const std::string provider = TrimAscii(
        data->Attribute("provider") ? data->Attribute("provider") : "");
    if (provider.empty() || ToLowerAscii(provider) == "perastage")
      return data;
  }
  tinyxml2::XMLElement *data = document.NewElement("Data");
  data->SetAttribute("provider", "Perastage");
  data->SetAttribute("ver", "1.0");
  userData->InsertEndChild(data);
  return data;
}

// Reports whether the scene contains valid Perastage layer color metadata.
bool HasLayerAppearance(const MvrScene &scene) {
  return std::any_of(scene.layers.begin(), scene.layers.end(),
                     [](const auto &entry) {
                       return IsLayerColor(entry.second.color);
                     });
}

// Appends the Perastage layer appearance map in established scene order.
void AppendLayerAppearance(
    tinyxml2::XMLDocument &document, tinyxml2::XMLElement *perastageData,
    const MvrScene &scene,
    const std::unordered_map<std::string, std::string> &layerUuids) {
  if (!perastageData)
    return;
  tinyxml2::XMLElement *map =
      perastageData->FirstChildElement("LayerAppearanceMap");
  for (const auto &[layerUuid, layer] : scene.layers) {
    if (!IsLayerColor(layer.color))
      continue;
    if (!map)
      map = document.NewElement("LayerAppearanceMap");
    tinyxml2::XMLElement *entry =
        document.NewElement("PerastageLayerAppearance");
    const auto preparedUuid = layerUuids.find(layerUuid);
    const std::string exportUuid = preparedUuid != layerUuids.end()
                                       ? preparedUuid->second
                                       : std::string{};
    if (!exportUuid.empty())
      entry->SetAttribute("uuid", exportUuid.c_str());
    if (!layer.name.empty())
      entry->SetAttribute("name", layer.name.c_str());
    entry->SetAttribute("color", layer.color.c_str());
    map->InsertEndChild(entry);
  }
  if (map && !map->Parent())
    perastageData->InsertEndChild(map);
}

} // namespace mvr_xml_extension
