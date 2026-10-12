#pragma once

#include "gdtf_power_reader.h"

#include <set>
#include <tinyxml2.h>

namespace gdtf {
// Conservative shared write target: never manufactures connector/topology data.
// Multiple authored consumers or instances of an input subtree are unsafe.
inline tinyxml2::XMLElement *FindPowerWriteTarget(
    tinyxml2::XMLElement *fixtureType, const char *&attribute) {
  std::vector<tinyxml2::XMLElement *> consumers;
  std::set<std::string> referenceTargets;
  std::set<std::string> consumerAncestors;
  auto visit = [&](auto &&self, tinyxml2::XMLElement *node) -> void {
    if (!node) return;
    if (std::string(node->Name()) == "GeometryReference" && node->Attribute("Geometry"))
      referenceTargets.insert(node->Attribute("Geometry"));
    if (std::string(node->Name()) == "WiringObject" &&
        node->Attribute("ComponentType", "Consumer")) {
      consumers.push_back(node);
      for (auto *ancestor = node; ancestor; ancestor = ancestor->Parent() ? ancestor->Parent()->ToElement() : nullptr)
        if (ancestor->Attribute("Name")) consumerAncestors.insert(ancestor->Attribute("Name"));
    }
    for (auto *child = node->FirstChildElement(); child;
         child = child->NextSiblingElement()) self(self, child);
  };
  visit(visit, fixtureType ? fixtureType->FirstChildElement("Geometries") : nullptr);
  bool instancedConsumer = false;
  for (const auto &name : referenceTargets)
    if (consumerAncestors.count(name)) instancedConsumer = true;
  if (consumers.size() == 1 && !instancedConsumer) {
    attribute = "ElectricalPayLoad";
    return consumers.front();
  }
  auto *physical = fixtureType ? fixtureType->FirstChildElement("PhysicalDescriptions") : nullptr;
  auto *properties = physical ? physical->FirstChildElement("Properties") : nullptr;
  auto *power = properties ? properties->FirstChildElement("PowerConsumption") : nullptr;
  if (power && !power->NextSiblingElement("PowerConsumption") &&
      power->Attribute("Connector") && *power->Attribute("Connector") &&
      power->Attribute("Value")) {
    if (ParseNonnegativePower(power->Attribute("Value")) &&
        std::string_view(power->Attribute("Connector")).find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%()*+-/:;<=>@_` \"'") == std::string_view::npos) {
      attribute = "Value";
      return power;
    }
  }
  attribute = nullptr;
  return nullptr;
}
} // namespace gdtf
