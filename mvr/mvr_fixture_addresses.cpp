/* This file is part of Perastage. */
#include "mvr_fixture_addresses.h"
#include "fixture_patch_service.h"
#include <tinyxml2.h>
#include <string_view>

namespace mvr {
namespace {
void Issue(const tinyxml2::XMLElement &node, const Fixture &fixture, MvrScene &scene,
           const std::string &reason) {
  tinyxml2::XMLPrinter printer(nullptr, true);
  node.Accept(&printer);
  scene.preservationIssues.push_back({"Addresses", fixture.uuid, "Fixture/Addresses", printer.CStr(),
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
void ReadFixtureAddresses(const tinyxml2::XMLElement &node, Fixture &fixture, MvrScene &scene) {
  auto *container = node.FirstChildElement("Addresses");
  if (!container) return;
  fixture.dmxAddresses.emplace();
  if (container->NextSiblingElement("Addresses") || container->FirstAttribute())
    Issue(*container, fixture, scene, "Duplicate Addresses containers or unsupported attributes.");
  for (auto *child = container->FirstChild(); child; child = child->NextSibling()) {
    if (child->ToComment()) continue;
    if (child->ToText() && perastage::patch::TrimDmx(child->Value()).empty()) continue;
    auto *entry = child->ToElement();
    if (!entry || std::string_view(entry->Name()) != "Address") {
      Issue(*container, fixture, scene, "Only standard Fixture Address entries are represented; Network/extension data is unsupported.");
      continue;
    }
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
  else fixture.address = result.primary;
}
void ScanFixtureAddresses(const tinyxml2::XMLElement &node, MvrScene &scene) {
  Fixture value;
  if (const auto *uuid = node.Attribute("uuid")) value.uuid = uuid;
  ReadFixtureAddresses(node, value, scene);
}
std::vector<MvrExportDiagnostic> CheckFixtureAddressProjection(
    const tinyxml2::XMLElement &root, const MvrScene &scene) {
  std::vector<MvrExportDiagnostic> diagnostics;
  std::unordered_set<std::string> seen;
  const auto fail = [&](const std::string &uuid) {
    diagnostics.push_back({MvrExportDiagnosticCode::StandardDataPreservationBlocked,
        MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
        true, "Fixture", {}, uuid, "Addresses",
        "Serialized Fixture Addresses disagree with the canonical scene assignments."});
  };
  const auto visit = [&](auto &&self, const tinyxml2::XMLElement &node) -> void {
    if (std::string_view(node.Name()) == "UserData") return;
    if (std::string_view(node.Name()) == "Fixture") {
      const std::string uuid = node.Attribute("uuid") ? node.Attribute("uuid") : "";
      const auto found = scene.fixtures.find(uuid);
      if (found != scene.fixtures.end() && found->second.dmxAddresses) {
        Fixture actual;
        MvrScene evidence;
        ReadFixtureAddresses(node, actual, evidence);
        if (!seen.insert(uuid).second || !evidence.preservationIssues.empty() ||
            actual.dmxAddresses != found->second.dmxAddresses) fail(uuid);
      }
    }
    for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement())
      self(self, *child);
  };
  visit(visit, root);
  for (const auto &[uuid, fixture] : scene.fixtures)
    if (fixture.dmxAddresses && !seen.contains(uuid)) fail(uuid);
  return diagnostics;
}
} // namespace mvr
