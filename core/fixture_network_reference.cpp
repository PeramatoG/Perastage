/* This file is part of Perastage. */
#include "fixture_network_reference.h"
#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "../models/truss.h"
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
// Truss publication must validate the mode and wires against the same definition.
Result InspectTrussReferences(const Truss &truss, const std::filesystem::path &path) {
  if (const auto binding = CheckBindingReference(truss, truss.gdtfSpec); !binding) return binding;
  if (truss.gdtfSpec.empty() && truss.gdtfMode.empty()) return {};
  if (truss.gdtfSpec.empty() || truss.gdtfMode.empty())
    return Failure(truss, Conflict::InvalidBinding, "GDTFMode", "a standard definition and explicit mode must be bound together");
  const auto archive = gdtf::ReadGdtfArchive(path);
  if (!archive.Success())
    return Failure(truss, Conflict::UnavailableGdtf, "GDTFSpec", "the selected GDTF definition cannot be read");
  const auto description = gdtf::ReadGdtfDescription(archive.descriptionXml);
  if (!description.Success() || std::find(description.dmxModeNames.begin(), description.dmxModeNames.end(),
      truss.gdtfMode) == description.dmxModeNames.end())
    return Failure(truss, Conflict::InvalidBinding, "GDTFMode", "the authored mode does not identify a DMXMode in the selected GDTF");
  return InspectNodeReferences(truss, path);
}
Result RebindNode(Truss &truss, const std::string &reference, const std::filesystem::path &path) {
  auto candidate = truss; candidate.gdtfSpec = reference;
  auto result = InspectTrussReferences(candidate, path);
  if (result) truss.gdtfSpec = reference;
  return result;
}
Result InspectReferences(const Fixture &fixture, const std::filesystem::path &path) {
  return InspectNodeReferences(fixture, path);
}
Result Rebind(Fixture &fixture, const std::string &reference, const std::filesystem::path &path) {
  return RebindNode(fixture, reference, path);
}
}
