#include "gdtf/gdtf_mode_browser_presenter.h"

#include <sstream>

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
    ch.address = channel.virtualChannel ? "Virtual" : ("Break " + RawOrNotSpecified(channel.rawDmxBreak) + ", Offset " + RawOrNotSpecified(channel.rawOffset));
    Detail(ch, "raw DMXBreak", RawOrNotSpecified(channel.rawDmxBreak));
    Detail(ch, "raw Offset", RawOrNotSpecified(channel.rawOffset));
    Detail(ch, "parsed offsets", FormatOffsets(channel.offsets));
    Detail(ch, "resolution", std::to_string(channel.resolution));
    Detail(ch, "virtual", YesNo(channel.virtualChannel));
    Detail(ch, "InitialFunction", RawOrNotSpecified(channel.initialFunction));
    Detail(ch, "Highlight", RawOrNotSpecified(channel.highlight));
    Detail(ch, "Geometry", RawOrNotSpecified(channel.geometry));
    rows.push_back(ch);
    for (const auto &logical : channel.logicalChannels) {
      GdtfModeBrowserNodePresentation lc;
      lc.id = logical.id;
      lc.parentId = channel.id;
      lc.item = DisplayName(logical.attribute);
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
