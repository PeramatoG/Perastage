#include "gdtf/gdtf_mode_browser_presenter.h"

#include <initializer_list>
#include <sstream>
#include <set>

namespace {
// Returns a fallback label for empty GDTF names without modifying source data.
std::string DisplayName(const std::string &value) {
  return value.empty() ? "(unnamed)" : value;
}

// Formats an omitted raw source value for the details inspector.
std::string RawOrNotSpecified(const std::string &value) {
  return value.empty() ? "Not specified" : value;
}

// Converts a boolean value to deterministic display text.
std::string YesNo(bool value) { return value ? "Yes" : "No"; }

// Formats a DMX range when calculation was reliable.
std::string FormatDmxRange(const std::optional<gdtf::GdtfDmxRange> &range) {
  if (!range)
    return {};
  return std::to_string(range->start) + " -> " + std::to_string(range->end);
}

// Formats a physical range when effective values are available.
std::string FormatPhysicalRange(const gdtf::GdtfPhysicalRange &range) {
  if (!range.available)
    return {};
  return RawOrNotSpecified(range.from) + " -> " + RawOrNotSpecified(range.to);
}

// Formats parsed offsets for the details inspector.
std::string FormatOffsets(const std::vector<gdtf::GdtfDmxValue> &offsets) {
  std::string result;
  for (const auto &offset : offsets) {
    if (!result.empty())
      result += ", ";
    result += offset.valid && offset.normalized ? std::to_string(*offset.normalized) : offset.raw;
  }
  return result.empty() ? "Not specified" : result;
}

// Formats one parsed offset for one physical DMX channel row.
std::string FormatOffsetLabel(const gdtf::GdtfDmxValue &offset) {
  return offset.valid && offset.normalized ? std::to_string(*offset.normalized)
                                           : offset.raw;
}


// Joins non-empty labels with a comma separator while preserving order.
std::string JoinLabels(const std::vector<std::string> &labels) {
  std::string result;
  for (const auto &label : labels) {
    if (label.empty())
      continue;
    if (!result.empty())
      result += ", ";
    result += label;
  }
  return result;
}


// Returns a readable terminal label from GDTF reference-like function text.
std::string NormalizeChannelFunctionLabel(const std::string &value) {
  std::string text = value;
  const size_t slash = text.find_last_of("/\\");
  if (slash != std::string::npos && slash + 1 < text.size())
    text = text.substr(slash + 1);
  const size_t dot = text.find_last_of('.');
  if (dot != std::string::npos && dot + 1 < text.size())
    text = text.substr(dot + 1);
  return text;
}

// Returns the first readable function label from ordered candidates.
std::string FirstFunctionLabel(std::initializer_list<std::string> candidates) {
  for (const auto &candidate : candidates) {
    const std::string normalized = NormalizeChannelFunctionLabel(candidate);
    if (!normalized.empty())
      return normalized;
  }
  return {};
}

// Returns display names for the coarse/fine bytes of one resolved channel name.
std::vector<std::string> ExpandResolutionNames(const std::string &baseName,
                                               int resolution) {
  if (baseName.empty())
    return {};
  std::vector<std::string> names{baseName};
  static const char *kSuffixes[] = {" Fine", " Ultra", " Ultra Fine"};
  for (int i = 1; i < resolution && i <= 3; ++i)
    names.push_back(baseName + kSuffixes[i - 1]);
  return names;
}

// Collects readable channel function names while preserving source order.
std::vector<std::string> CollectChannelFunctionNames(
    const gdtf::GdtfDmxChannelNode &channel) {
  std::vector<std::string> names;
  std::set<std::string> seen;
  auto addName = [&](const std::string &name) {
    if (!name.empty() && seen.insert(name).second)
      names.push_back(name);
  };
  for (const auto &logical : channel.logicalChannels) {
    addName(NormalizeChannelFunctionLabel(logical.attribute));
    for (const auto &function : logical.channelFunctions)
      addName(FirstFunctionLabel({function.attribute, function.originalAttribute,
                                  function.name}));
  }
  addName(NormalizeChannelFunctionLabel(channel.initialFunction));
  return names;
}

// Returns one display name for each physical byte represented by the channel.
std::vector<std::string> BuildPerByteChannelFunctionNames(
    const gdtf::GdtfDmxChannelNode &channel) {
  std::vector<std::string> names = CollectChannelFunctionNames(channel);
  const int byteCount = static_cast<int>(channel.offsets.size());
  if (names.size() == 1 && byteCount > 1)
    names = ExpandResolutionNames(names.front(), byteCount);
  if (byteCount > 0 && static_cast<int>(names.size()) > byteCount)
    names.resize(static_cast<size_t>(byteCount));
  return names;
}

// Returns grouped channel functions for the hierarchical browser root row.
std::string FormatGroupedChannelFunctions(
    const gdtf::GdtfDmxChannelNode &channel) {
  std::vector<std::string> names = BuildPerByteChannelFunctionNames(channel);
  if (names.empty())
    names = CollectChannelFunctionNames(channel);
  const std::string joined = JoinLabels(names);
  if (!joined.empty())
    return joined;
  return channel.virtualChannel ? "Virtual" : "Not specified";
}

// Returns the summary label for one physical channel byte.
std::string FormatSummaryFunctionAt(
    const std::vector<std::string> &names, size_t index) {
  if (index < names.size() && !names[index].empty())
    return names[index];
  return "-";
}

// Adds a detail row to a presentation node.
void Detail(GdtfModeBrowserNodePresentation &node, std::string key, std::string value) {
  node.details.push_back({std::move(key), std::move(value)});
}
} // namespace

// Builds flattened node presentations while preserving hierarchical parent IDs.
std::vector<GdtfModeBrowserNodePresentation>
BuildGdtfModeBrowserPresentation(const gdtf::GdtfDmxModeNode *mode) {
  std::vector<GdtfModeBrowserNodePresentation> rows;
  if (!mode)
    return rows;
  for (const auto &channel : mode->channels) {
    GdtfModeBrowserNodePresentation ch;
    ch.id = channel.id;
    ch.item = channel.virtualChannel ? "Virtual DMX Channel" : "DMX Channel " + FormatOffsets(channel.offsets);
    ch.address = FormatGroupedChannelFunctions(channel);
    Detail(ch, "raw DMXBreak", RawOrNotSpecified(channel.rawDmxBreak));
    Detail(ch, "raw Offset", RawOrNotSpecified(channel.rawOffset));
    Detail(ch, "parsed offsets", FormatOffsets(channel.offsets));
    Detail(ch, "resolution", std::to_string(channel.resolution));
    Detail(ch, "virtual", YesNo(channel.virtualChannel));
    Detail(ch, "InitialFunction", RawOrNotSpecified(channel.initialFunction));
    Detail(ch, "Highlight", RawOrNotSpecified(channel.highlight));
    Detail(ch, "Geometry", RawOrNotSpecified(channel.geometry));
    Detail(ch, "Channel functions", ch.address);
    rows.push_back(ch);
    for (const auto &logical : channel.logicalChannels) {
      GdtfModeBrowserNodePresentation lc;
      lc.id = logical.id;
      lc.parentId = channel.id;
      lc.item = DisplayName(logical.attribute);
      lc.address = NormalizeChannelFunctionLabel(logical.attribute);
      if (lc.address.empty()) {
        std::vector<std::string> logicalNames;
        for (const auto &function : logical.channelFunctions)
          logicalNames.push_back(FirstFunctionLabel(
              {function.attribute, function.originalAttribute, function.name}));
        lc.address = JoinLabels(logicalNames);
      }
      lc.unit = logical.attributeInfo.physicalUnit;
      Detail(lc, "Attribute", RawOrNotSpecified(logical.attribute));
      Detail(lc, "Pretty", RawOrNotSpecified(logical.attributeInfo.pretty));
      Detail(lc, "Snap", RawOrNotSpecified(logical.snap));
      Detail(lc, "Master", RawOrNotSpecified(logical.master));
      Detail(lc, "MibFade", RawOrNotSpecified(logical.mibFade));
      Detail(lc, "DMXChangeTimeLimit", RawOrNotSpecified(logical.dmxChangeTimeLimit));
      Detail(lc, "PhysicalUnit", RawOrNotSpecified(logical.attributeInfo.physicalUnit));
      Detail(lc, "Feature", RawOrNotSpecified(logical.attributeInfo.feature));
      Detail(lc, "ActivationGroup", RawOrNotSpecified(logical.attributeInfo.activationGroup));
      Detail(lc, "MainAttribute", RawOrNotSpecified(logical.attributeInfo.mainAttribute));
      rows.push_back(lc);
      for (const auto &fn : logical.channelFunctions) {
        GdtfModeBrowserNodePresentation f;
        f.id = fn.id;
        f.parentId = logical.id;
        f.item = DisplayName(fn.name.empty() ? fn.attribute : fn.name);
        f.address = FirstFunctionLabel({fn.attribute, fn.originalAttribute, fn.name});
        f.dmxRange = FormatDmxRange(fn.effectiveDmxRange);
        f.physicalRange = FormatPhysicalRange(fn.effectivePhysicalRange);
        f.unit = fn.physicalUnit.empty() ? logical.attributeInfo.physicalUnit : fn.physicalUnit;
        Detail(f, "Name", RawOrNotSpecified(fn.name));
        Detail(f, "Attribute", RawOrNotSpecified(fn.attribute));
        Detail(f, "OriginalAttribute", RawOrNotSpecified(fn.originalAttribute));
        Detail(f, "raw DMXFrom", RawOrNotSpecified(fn.rawDmxFrom));
        Detail(f, "effective DMX range", f.dmxRange.empty() ? "Unavailable" : f.dmxRange);
        Detail(f, "Default", RawOrNotSpecified(fn.rawDefault));
        Detail(f, "raw PhysicalFrom", RawOrNotSpecified(fn.rawPhysicalFrom));
        Detail(f, "raw PhysicalTo", RawOrNotSpecified(fn.rawPhysicalTo));
        Detail(f, "effective physical range", f.physicalRange.empty() ? "Unavailable" : f.physicalRange);
        Detail(f, "Unit", RawOrNotSpecified(f.unit));
        Detail(f, "Wheel", RawOrNotSpecified(fn.wheel));
        Detail(f, "Emitter", RawOrNotSpecified(fn.emitter));
        Detail(f, "Filter", RawOrNotSpecified(fn.filter));
        Detail(f, "ModeMaster", RawOrNotSpecified(fn.modeMaster));
        Detail(f, "ModeFrom/ModeTo", RawOrNotSpecified(fn.modeFrom) + " / " + RawOrNotSpecified(fn.modeTo));
        Detail(f, "DMXProfile", RawOrNotSpecified(fn.dmxProfile));
        Detail(f, "CustomName", RawOrNotSpecified(fn.customName));
        rows.push_back(f);
        for (const auto &set : fn.channelSets) {
          GdtfModeBrowserNodePresentation s;
          s.id = set.id;
          s.parentId = fn.id;
          s.item = DisplayName(set.name);
          s.address = f.address;
          s.dmxRange = FormatDmxRange(set.effectiveDmxRange);
          s.physicalRange = FormatPhysicalRange(set.effectivePhysicalRange);
          s.unit = f.unit;
          Detail(s, "Name", RawOrNotSpecified(set.name));
          Detail(s, "raw DMXFrom", RawOrNotSpecified(set.rawDmxFrom));
          Detail(s, "effective DMX range", s.dmxRange.empty() ? "Unavailable" : s.dmxRange);
          Detail(s, "raw PhysicalFrom", RawOrNotSpecified(set.rawPhysicalFrom));
          Detail(s, "raw PhysicalTo", RawOrNotSpecified(set.rawPhysicalTo));
          Detail(s, "effective physical range", s.physicalRange.empty() ? "Unavailable" : s.physicalRange);
          Detail(s, "origin of inherited/default values", set.effectivePhysicalRange.available ? "Explicit or inherited" : "Unavailable");
          Detail(s, "WheelSlotIndex", RawOrNotSpecified(set.wheelSlotIndex));
          rows.push_back(s);
          for (const auto &sub : set.subChannelSets) {
            GdtfModeBrowserNodePresentation ss;
            ss.id = sub.id;
            ss.parentId = set.id;
            ss.item = DisplayName(sub.name);
            ss.address = f.address;
            ss.physicalRange = RawOrNotSpecified(sub.physicalFrom) + " -> " + RawOrNotSpecified(sub.physicalTo);
            ss.unit = sub.subPhysicalUnit;
            Detail(ss, "Name", RawOrNotSpecified(sub.name));
            Detail(ss, "PhysicalFrom/To", ss.physicalRange);
            Detail(ss, "SubPhysicalUnit", RawOrNotSpecified(sub.subPhysicalUnit));
            Detail(ss, "DMXProfile", RawOrNotSpecified(sub.dmxProfile));
            rows.push_back(ss);
          }
        }
      }
    }
  }
  return rows;
}

// Builds the quick legacy-style channel summary presentation for a mode.
std::vector<GdtfModeChannelPresentation>
BuildGdtfModeChannelSummaryPresentation(const gdtf::GdtfDmxModeNode *mode) {
  std::vector<GdtfModeChannelPresentation> rows;
  if (!mode)
    return rows;
  for (const auto &channel : mode->channels) {
    const std::vector<std::string> names = BuildPerByteChannelFunctionNames(channel);
    if (channel.virtualChannel || channel.offsets.empty()) {
      rows.push_back({"V", FormatGroupedChannelFunctions(channel)});
      continue;
    }
    for (size_t i = 0; i < channel.offsets.size(); ++i) {
      rows.push_back({FormatOffsetLabel(channel.offsets[i]),
                      FormatSummaryFunctionAt(names, i)});
    }
  }
  return rows;
}
