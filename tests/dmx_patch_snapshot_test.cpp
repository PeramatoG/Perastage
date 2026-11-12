/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_snapshot.h"
#include "fixture_patch_service.h"
#include "support/gdtf_channel_footprint_cases.h"
#include <cassert>
#include <algorithm>
using namespace dmx_patch;
namespace {
std::string Channel(const std::string &offset, const std::string &attribute, int br = 1) {
  return "<DMXChannel Geometry='Body' DMXBreak='" + std::to_string(br) + "' Offset='" + offset +
    "'><LogicalChannel Attribute='" + attribute + "'><ChannelFunction Name='" + attribute +
    "' Attribute='" + attribute + "'/></LogicalChannel></DMXChannel>";
}
std::shared_ptr<const gdtf::GdtfModeChannelDocument> Document(const std::string &channels) {
  return std::make_shared<gdtf::GdtfModeChannelDocument>(gdtf::ReadGdtfModeChannelDocument(
      tests::gdtf::Description("<Geometry Name='Body'/>", channels)));
}
Fixture Make(const std::string &uuid, const std::string &start) {
  Fixture fixture;
  fixture.uuid = uuid; fixture.address = start; fixture.gdtfMode = "Pixels";
  fixture.instanceName = "RGB pixel strip"; fixture.function = "Stage wash";
  return fixture;
}
bool Has(const Snapshot &snapshot, Issue issue) {
  return std::any_of(snapshot.diagnostics.begin(), snapshot.diagnostics.end(),
      [&](const auto &diagnostic) { return diagnostic.issue == issue; });
}
}
int main() {
  MvrScene scene;
  auto document = Document(Channel("1", "Dimmer"));
  auto resolve = [&](const Fixture &) { return document; };
  auto empty = BuildSnapshot(scene, 1, resolve);
  assert(empty.cells.size() == 512 && empty.occupied == 0);
  assert(std::all_of(empty.cells.begin(), empty.cells.end(), [](const auto &c) { return c.occupants.empty(); }));
  scene.fixtures["a"] = Make("a", "1.512");
  scene.fixtures["a"].visualColorHex = "#123456";
  scene.fixtures["a"].automaticVisualColorHex = "#AABBCC";
  auto single = BuildSnapshot(scene, 1, resolve);
  assert(single.fixtures.at("a").colorHex == "#123456");
  scene.fixtures["a"].visualColorHex.clear();
  assert(BuildSnapshot(scene, 1, resolve).fixtures.at("a").colorHex == "#AABBCC");
  assert(single.occupied == 1 && !Has(single, Issue::Overflow));
  assert(single.cells[511].occupants[0].attribute == "Dimmer");
  document = Document(Channel("1", "Red") + Channel("2", "Green") + Channel("3", "Blue"));
  scene.fixtures["a"].address = "1.1";
  auto rgb = BuildSnapshot(scene, 1, resolve);
  assert(rgb.occupied == 3 && rgb.cells[1].occupants[0].attribute == "Green");
  assert(rgb.cells[2].occupants[0].functions == "Blue");
  scene.fixtures["b"] = Make("b", "1.4");
  assert(BuildSnapshot(scene, 1, resolve).conflicts == 0);
  scene.fixtures["b"].address = "1.3";
  auto overlap = BuildSnapshot(scene, 1, resolve);
  assert(overlap.conflicts == 1 && overlap.cells[2].occupants.size() == 2);
  assert(overlap.cells[2].occupants[0].uuid == "a" && overlap.cells[2].occupants[1].uuid == "b");
  MvrScene reversed;
  reversed.fixtures["b"] = scene.fixtures.at("b"); reversed.fixtures["a"] = scene.fixtures.at("a");
  assert(BuildSnapshot(reversed, 1, resolve).cells[2].occupants == overlap.cells[2].occupants);
  document = Document(Channel("1,3,8,10", "Pan") + Channel("None", "Virtual"));
  scene.fixtures["b"].address = "1.2";
  auto sparse = BuildSnapshot(scene, 1, resolve);
  assert(sparse.occupied == 8 && sparse.conflicts == 0);
  assert(sparse.cells[2].occupants[0].byteIndex == 1);
  assert(sparse.cells[7].occupants[0].byteIndex == 2);
  assert(sparse.cells[4].occupants.empty());
  scene.fixtures.erase("b");
  document = std::make_shared<gdtf::GdtfModeChannelDocument>(
      gdtf::ReadGdtfModeChannelDocument(tests::gdtf::PixelDescription(60, true)));
  assert(BuildSnapshot(scene, 1, resolve).occupied == 180);
  scene.fixtures["a"].address = "1.400";
  auto overflow = BuildSnapshot(scene, 1, resolve);
  assert(overflow.occupied == 113 && Has(overflow, Issue::Overflow));
  assert(BuildSnapshot(scene, 2, resolve).occupied == 0);
  document = Document(Channel("1,2", "Pan") + Channel("10", "Dimmer", 2));
  auto &a = scene.fixtures.at("a");
  a.address = "1.1";
  a.dmxAddresses = FixtureDmxAddresses{{{std::string("0"), "1.1"}, {std::string("1"), "70000.501"}}};
  auto breaks = BuildSnapshot(scene, 70000, resolve);
  assert(breaks.occupied == 1 && breaks.cells[509].occupants[0].gdtfBreak == 2);
  assert(breaks.universes.contains(1) && breaks.universes.contains(70000));
  assert(BuildSnapshot(scene, 1, resolve).occupied == 2);
  const auto genuineBreaks = BuildSnapshot(scene, 1, resolve, Preview::DeclaredOffsets);
  assert(genuineBreaks.occupied == 2 && genuineBreaks.provisional == 0);
  const auto authored = a.dmxAddresses;
  BuildSnapshot(scene, 1, resolve);
  assert(a.dmxAddresses == authored && a.address == "1.1" && a.function == "Stage wash");
  a.gdtfMode = "Missing";
  assert(Has(BuildSnapshot(scene, 1, resolve), Issue::UnresolvedDefinition));
  a.gdtfMode = "Pixels";
  document.reset();
  assert(BuildSnapshot(scene, 1, resolve).occupied == 0);
  document = Document(Channel("1", "Dimmer") + Channel("1", "Intensity"));
  a.dmxAddresses.reset(); a.address = "";
  auto unpatched = BuildSnapshot(scene, 1, resolve);
  assert(unpatched.occupied == 0 && Has(unpatched, Issue::Unpatched));
  assert(BuildSnapshot(scene, 1, resolve, Preview::DeclaredOffsets).provisional == 0);
  a.address = "1.1";
  auto aliases = BuildSnapshot(scene, 1, resolve);
  assert(aliases.cells[0].occupants.size() == 2 && aliases.conflicts == 0);
  a.address = "1.513";
  assert(BuildSnapshot(scene, 1, resolve).occupied == 0);
  a.address.clear();
  a.dmxAddresses = FixtureDmxAddresses{{{std::string("0"), "1"}, {std::string("0"), "2"}}};
  assert(Has(BuildSnapshot(scene, 1, resolve), Issue::InvalidBreak));
  assert(BuildSnapshot(scene, 1, resolve).occupied == 0);
  assert(ReadAddress("18000000000.512")->universe == 18000000000ULL);
  assert(ReadAddress("400")->channel == 400 && !ReadAddress("1.513"));
  // Orphan Overwrite and a genuinely unassigned secondary break cannot be
  // interpreted as primary bytes. Keep their declared offsets in diagnostics.
  a.dmxAddresses.reset(); a.address = "1.273";
  std::string inconsistent = Channel("1,2", "Tilt") + Channel("3", "Dimmer") +
      Channel("4", "Duration") + Channel("5", "Rate") + Channel("7", "Control") +
      Channel("8", "PlateDimmer", 2) + Channel("12", "Red") + Channel("13", "Green") + Channel("14", "Blue");
  for (const auto offset : {6, 9, 10, 11}) {
    auto orphan = Channel(std::to_string(offset), "Shutter");
    orphan.replace(orphan.find("DMXBreak='1'"), 12, "DMXBreak='Overwrite'");
    inconsistent += orphan;
  }
  document = Document(inconsistent);
  const auto incomplete = BuildSnapshot(scene, 1, resolve);
  assert(incomplete.occupied == 9 && incomplete.diagnostics.size() == 5);
  const auto declared = std::find_if(incomplete.diagnostics.begin(), incomplete.diagnostics.end(),
      [](const auto &issue) { return issue.source && issue.source->offsets == "6"; });
  assert(declared != incomplete.diagnostics.end() && declared->source->breakId == "Overwrite");
  assert(declared->issue == Issue::UnresolvedChannel);
  assert(incomplete.cells[277].occupants.empty() && incomplete.cells[279].occupants.empty());
  assert(a.address == "1.273" && !a.dmxAddresses);
  const auto compatible = BuildSnapshot(scene, 1, resolve, Preview::DeclaredOffsets);
  assert(compatible.occupied == 9 && compatible.provisional == 5 && compatible.conflicts == 0);
  for (int i = 272; i < 286; ++i)
    assert(compatible.cells[i].occupants.size() + compatible.cells[i].provisional.size() == 1);
  assert(compatible.cells[277].provisional[0].attribute == "Shutter");
  assert(BuildSnapshot(scene, 2, resolve, Preview::DeclaredOffsets).provisional == 0);
  a.address = "1.511";
  const auto clippedPreview = BuildSnapshot(scene, 1, resolve, Preview::DeclaredOffsets);
  assert(clippedPreview.occupied == 2 && clippedPreview.provisional == 0);
  assert(a.address == "1.511" && !a.dmxAddresses);
  a.address = "1.273";
  // The shared footprint corpus covers GeometryReference breaks, sparse bytes,
  // virtual channels, invalid offsets and partially resolvable geometries.
  for (const auto &test : tests::gdtf::FootprintCases()) {
    document = std::make_shared<gdtf::GdtfModeChannelDocument>(gdtf::ReadGdtfModeChannelDocument(test.xml));
    a.address = test.bytes.contains(1) ? "1.1" : "";
    a.dmxAddresses = FixtureDmxAddresses{};
    for (const auto &[br, bytes] : test.bytes)
      a.dmxAddresses->entries.push_back({std::to_string(br - 1), std::to_string(br) + ".1"});
    for (const auto &[br, bytes] : test.bytes) {
      const auto actual = BuildSnapshot(scene, br, resolve);
      std::set<int> expected;
      for (auto byte : bytes) if (byte <= 512) expected.insert(byte);
      assert(actual.occupied == static_cast<int>(expected.size()));
      for (auto byte : expected) assert(!actual.cells[byte - 1].occupants.empty());
    }
  }
}
