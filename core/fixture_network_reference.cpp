/* This file is part of Perastage. */
#include "fixture_network_reference.h"
#include "gdtf_archive_reader.h"
#include <tinyxml2.h>
#include <string_view>

namespace fixture_network {
std::optional<WireNames> ReadWireNames(const std::filesystem::path &path) {
  const auto archive = gdtf::ReadGdtfArchive(path);
  tinyxml2::XMLDocument document;
  if (!archive.Success() || document.Parse(archive.descriptionXml.c_str()) != tinyxml2::XML_SUCCESS)
    return std::nullopt;
  auto *root = document.FirstChildElement("GDTF");
  auto *type = root ? root->FirstChildElement("FixtureType") : nullptr;
  auto *geometries = type ? type->FirstChildElement("Geometries") : nullptr;
  if (!type) return std::nullopt;
  if (!geometries) return WireNames{};
  WireNames names, allNames;
  const auto visit = [&](auto &&self, const tinyxml2::XMLElement &node) -> void {
    if (const auto *name = node.Attribute("Name")) {
      ++allNames[name];
      if (std::string_view(node.Name()) == "WiringObject") names.try_emplace(name, 0u);
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, *child);
  };
  visit(visit, *geometries);
  for (auto &[name, count] : names) count = allNames.at(name);
  return names;
}
Result InspectReferences(const Fixture &fixture, const std::filesystem::path &path) {
  if (auto result = Validate(fixture); !result) return result;
  if (!fixture.dmxAddresses || fixture.dmxAddresses->networks.empty()) return {};
  return CheckReferences(fixture, ReadWireNames(path));
}
Result Rebind(Fixture &fixture, const std::string &reference, const std::filesystem::path &path) {
  if (auto binding = CheckBindingReference(fixture, reference); !binding) return binding;
  auto result = InspectReferences(fixture, path);
  if (result) fixture.gdtfSpec = reference;
  return result;
}
}
