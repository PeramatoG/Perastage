/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_snapshot.h"
#include "fixture_patch_service.h"
#include "fixture_visual_color.h"
#include <algorithm>
#include <charconv>
#include <limits>
#include <tuple>

namespace dmx_patch {
std::optional<Address> ReadAddress(const std::string &raw) {
  using namespace perastage::patch;
  if (!IsDmxValue(raw)) return {};
  const auto text = TrimDmx(raw);
  const auto dot = text.find('.');
  const auto first = *DmxInteger(text.substr(0, dot), false);
  Universe number = 0;
  const auto parsed = std::from_chars(first.data(), first.data() + first.size(), number);
  if (parsed.ec != std::errc{}) return {};
  if (dot == std::string_view::npos)
    return Address{(number - 1) / 512 + 1, static_cast<int>((number - 1) % 512 + 1)};
  return Address{number, std::stoi(*DmxInteger(text.substr(dot + 1), false))};
}
bool Cell::Conflict() const {
  return !occupants.empty() && std::any_of(occupants.begin(), occupants.end(),
      [&](const auto &entry) { return entry.uuid != occupants.front().uuid; });
}
namespace {
std::string Join(const std::set<std::string> &values) {
  std::string result;
  for (const auto &value : values) {
    if (!result.empty()) result += " / ";
    result += value;
  }
  return result;
}
}
Snapshot BuildSnapshot(const MvrScene &scene, Universe universe, const DocumentResolver &resolve) {
  Snapshot result;
  result.universe = universe;
  std::vector<const Fixture *> fixtures;
  for (const auto &[uuid, fixture] : scene.fixtures) fixtures.push_back(&fixture);
  std::sort(fixtures.begin(), fixtures.end(), [](auto a, auto b) { return a->uuid < b->uuid; });
  for (const auto *fixture : fixtures) {
    const auto &f = *fixture;
    std::set<std::pair<Issue, std::string>> reported;
    auto issue = [&](Issue code, const std::string &detail = {}) {
      if (reported.emplace(code, detail).second)
        result.diagnostics.push_back({f.uuid, code, detail});
    };
    result.fixtures[f.uuid] = {f.instanceName.empty() ? f.typeName : f.instanceName,
        f.fixtureIdText.empty() ? std::to_string(f.fixtureId) : f.fixtureIdText,
        f.function, ResolveFixturePresentationColor(f).colorHex};
    const auto resolved = perastage::patch::ResolveFixturePatch(f);
    // Even malformed source data stays untouched. Unambiguous valid breaks can
    // still be visualized alongside diagnostics for the incomplete assignment.
    const auto &addresses = resolved ? resolved.addresses : f.dmxAddresses;
    std::map<std::string, Address> starts;
    std::set<std::string> seen, ambiguous;
    if (addresses) {
      for (const auto &entry : addresses->entries) {
        const auto key = perastage::patch::DmxInteger(entry.breakId.value_or("0"), true);
        if (!key || key->front() == '-') { issue(Issue::InvalidBreak); continue; }
        if (!seen.insert(*key).second) ambiguous.insert(*key);
        const auto start = ReadAddress(entry.value);
        if (!start) { issue(Issue::InvalidAddress, entry.value); continue; }
        starts[*key] = *start;
        result.universes.insert(start->universe);
      }
      for (const auto &key : ambiguous) { starts.erase(key); issue(Issue::InvalidBreak, key); }
    } else if (!f.address.empty()) {
      if (const auto start = ReadAddress(f.address)) {
        starts["0"] = *start;
        result.universes.insert(start->universe);
      } else issue(Issue::InvalidAddress, f.address);
    }
    if (starts.empty()) issue(Issue::Unpatched);
    const auto document = resolve ? resolve(f) : nullptr;
    const gdtf::GdtfDmxModeNode *mode = nullptr;
    if (document) {
      for (const auto &candidate : document->modes)
        if (candidate.name == f.gdtfMode) {
          if (mode) { mode = nullptr; break; }
          mode = &candidate;
        }
    }
    if (!mode) { issue(Issue::UnresolvedDefinition); continue; }
    bool overflow = false, unresolved = false;
    for (const auto &channel : mode->channels) {
      if (channel.virtualChannel) continue;
      const SourceChannel source{channel.rawOffset, channel.rawDmxBreak, channel.geometry};
      if (!channel.effectiveDmxBreak || channel.offsets.empty()) {
        result.diagnostics.push_back({f.uuid, Issue::UnresolvedChannel, {}, source});
        continue;
      }
      const auto key = std::to_string(*channel.effectiveDmxBreak - 1);
      const auto start = starts.find(key);
      if (start == starts.end()) {
        result.diagnostics.push_back({f.uuid, Issue::Unpatched, key, source});
        continue;
      }
      std::set<std::string> attributes, functions;
      for (const auto &logical : channel.logicalChannels) {
        const auto label = logical.attributeInfo.pretty.empty() ? logical.attribute : logical.attributeInfo.pretty;
        if (!label.empty()) attributes.insert(label);
        for (const auto &function : logical.channelFunctions)
          if (!function.name.empty()) functions.insert(function.name);
      }
      for (size_t byte = 0; byte < channel.offsets.size(); ++byte) {
        const auto &offset = channel.offsets[byte];
        if (!offset.valid || !offset.normalized || *offset.normalized < 1 ||
            *offset.normalized > std::numeric_limits<int>::max()) { unresolved = true; continue; }
        const auto physical = static_cast<std::uint64_t>(start->second.channel) + *offset.normalized - 1;
        if (physical > 512) { overflow = true; continue; }
        if (start->second.universe != universe) continue;
        result.cells[physical - 1].occupants.push_back({f.uuid, *channel.effectiveDmxBreak,
            static_cast<int>(*offset.normalized), static_cast<int>(byte), Join(attributes), Join(functions)});
      }
    }
    if (unresolved) issue(Issue::UnresolvedChannel);
    if (overflow) issue(Issue::Overflow);
  }
  for (auto &cell : result.cells) {
    auto &entries = cell.occupants;
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      return std::tie(a.uuid, a.gdtfBreak, a.offset, a.byteIndex, a.attribute, a.functions) <
             std::tie(b.uuid, b.gdtfBreak, b.offset, b.byteIndex, b.attribute, b.functions);
    });
    entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
    result.occupied += !entries.empty();
    result.conflicts += cell.Conflict();
  }
  return result;
}
} // namespace dmx_patch
