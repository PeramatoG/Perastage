/* This file is part of Perastage. */
#include "mvr_fixture_addresses.h"
#include "fixture_patch_service.h"
#include <tinyxml2.h>
#include <string_view>
#include <type_traits>

namespace mvr {
namespace {
template <class Node>
void Issue(const tinyxml2::XMLElement &node, const Node &fixture, MvrScene &scene,
           const std::string &reason) {
  tinyxml2::XMLPrinter printer(nullptr, true);
  node.Accept(&printer);
  scene.preservationIssues.push_back({"Addresses", fixture.uuid, std::is_same_v<Node, Support> ? "Support/Addresses" : "Fixture/Addresses", printer.CStr(),
      reason, "Keep the original MVR; resolve the unsupported or ambiguous assignment before exporting."});
}
bool TextOnly(const tinyxml2::XMLElement &element, std::string &text) {
  for (auto *child = element.FirstChild(); child; child = child->NextSibling()) {
    if (child->ToComment()) continue;
    if (!child->ToText()) return false;
    text += child->Value();
  }
  return true;
}
}
template <class Node>
void ReadAddresses(const tinyxml2::XMLElement &node, Node &fixture, MvrScene &scene) {
  auto *container = node.FirstChildElement("Addresses");
  if (!container) return;
  fixture.dmxAddresses.emplace();
  if (container->NextSiblingElement("Addresses") || container->FirstAttribute())
    Issue(*container, fixture, scene, "Duplicate Addresses containers or unsupported attributes.");
  for (auto *child = container->FirstChild(); child; child = child->NextSibling()) {
    if (child->ToComment()) continue;
    if (child->ToText() && perastage::patch::TrimDmx(child->Value()).empty()) continue;
    auto *entry = child->ToElement();
    if (entry && std::string_view(entry->Name()) == "Network") {
      FixtureNetworkAssignment network;
      if (const auto *geometry = entry->Attribute("geometry")) network.geometry = geometry;
      const std::pair<const char *, std::optional<std::string> *> attributes[] = {
          {"ipv4", &network.ipv4}, {"subnetmask", &network.subnetmask},
          {"ipv6", &network.ipv6}, {"dhcp", &network.dhcp}, {"hostname", &network.hostname}};
      for (const auto &[name, value] : attributes)
        if (const auto *text = entry->Attribute(name)) *value = text;
      for (auto *attribute = entry->FirstAttribute(); attribute; attribute = attribute->Next()) {
        const std::string_view name = attribute->Name();
        if (name != "geometry" && std::none_of(std::begin(attributes), std::end(attributes),
            [&](const auto &known) { return name == known.first; }))
          Issue(*entry, fixture, scene, "Unsupported Network attribute.");
      }
      std::string text;
      if (!TextOnly(*entry, text) || !perastage::patch::TrimDmx(text).empty())
        Issue(*entry, fixture, scene, "Network contains unsupported child content.");
      fixture.dmxAddresses->networks.push_back(std::move(network));
      continue;
    }
    if (!entry || std::string_view(entry->Name()) != "Address") {
      Issue(*container, fixture, scene, "Unsupported Addresses child.");
      continue;
    }
    if (!fixture.dmxAddresses->networks.empty())
      Issue(*entry, fixture, scene, "Address children must precede Network children.");
    FixtureDmxAddress value;
    if (const auto *identifier = entry->Attribute("break")) value.breakId = identifier;
    for (auto *attribute = entry->FirstAttribute(); attribute; attribute = attribute->Next())
      if (std::string_view(attribute->Name()) != "break")
        Issue(*entry, fixture, scene, "Unsupported Address attribute.");
    if (!TextOnly(*entry, value.value)) Issue(*entry, fixture, scene, "Address contains unsupported child structure.");
    fixture.dmxAddresses->entries.push_back(std::move(value));
  }
  const auto result = perastage::patch::InspectFixtureAddresses(fixture);
  if (!result) Issue(*container, fixture, scene, result.detail);
  else if constexpr (requires { fixture.address; }) fixture.address = result.primary;
}
void ReadFixtureAddresses(const tinyxml2::XMLElement &node, Fixture &fixture, MvrScene &scene) {
  ReadAddresses(node, fixture, scene);
}
void ReadSupportAddresses(const tinyxml2::XMLElement &node, Support &support, MvrScene &scene) {
  ReadAddresses(node, support, scene);
}
void ScanFixtureAddresses(const tinyxml2::XMLElement &node, MvrScene &scene) {
  Fixture value;
  if (const auto *uuid = node.Attribute("uuid")) value.uuid = uuid;
  if (std::string_view(node.Name()) == "Support") {
    Support support; support.uuid = value.uuid; ReadSupportAddresses(node, support, scene);
  } else ReadFixtureAddresses(node, value, scene);
}
std::vector<MvrExportDiagnostic> CheckFixtureAddressProjection(
    const tinyxml2::XMLElement &root, const MvrScene &scene) {
  std::vector<MvrExportDiagnostic> diagnostics;
  std::unordered_set<std::string> seen;
  const auto fail = [&](const std::string &uuid) {
    diagnostics.push_back({MvrExportDiagnosticCode::StandardDataPreservationBlocked,
        MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
        true, "Addresses", {}, uuid, "Addresses",
        "Serialized Addresses disagree with the canonical scene assignments."});
  };
  const auto visit = [&](auto &&self, const tinyxml2::XMLElement &node) -> void {
    if (std::string_view(node.Name()) == "UserData") return;
    if (std::string_view(node.Name()) == "Fixture" || std::string_view(node.Name()) == "Support") {
      const std::string uuid = node.Attribute("uuid") ? node.Attribute("uuid") : "";
      const auto *expected = [&]() -> const std::optional<FixtureDmxAddresses> * {
        if (std::string_view(node.Name()) == "Fixture" && scene.fixtures.contains(uuid) &&
            scene.fixtures.at(uuid).dmxAddresses)
          return &scene.fixtures.at(uuid).dmxAddresses;
        if (std::string_view(node.Name()) == "Support" && scene.supports.contains(uuid))
          return &scene.supports.at(uuid).dmxAddresses;
        return nullptr;
      }();
      if (expected) {
        Fixture actual;
        MvrScene evidence;
        ReadFixtureAddresses(node, actual, evidence);
        if (!seen.insert(uuid).second || !evidence.preservationIssues.empty() ||
            actual.dmxAddresses != *expected) fail(uuid);
      }
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, *child);
  };
  visit(visit, root);
  for (const auto &[uuid, fixture] : scene.fixtures)
    if (fixture.dmxAddresses && !seen.contains(uuid)) fail(uuid);
  for (const auto &[uuid, support] : scene.supports)
    if (support.dmxAddresses && !seen.contains(uuid)) fail(uuid);
  return diagnostics;
}
} // namespace mvr
