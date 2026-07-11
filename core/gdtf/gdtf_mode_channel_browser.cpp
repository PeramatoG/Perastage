#include "gdtf/gdtf_mode_channel_browser.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <sstream>

#include <tinyxml2.h>

namespace gdtf {
namespace {
// Trims ASCII whitespace while preserving the original raw value elsewhere.
std::string Trim(std::string value) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
  return value;
}

// Reads an XML attribute without fabricating omitted source text.
std::string Attr(const tinyxml2::XMLElement *element, const char *name) {
  if (!element)
    return {};
  if (const char *value = element->Attribute(name))
    return value;
  return {};
}

// Adds a structured parser diagnostic.
void AddDiagnostic(GdtfModeChannelDocument &doc, GdtfDiagnosticSeverity severity,
                   std::string message, std::string context = {},
                   std::string rawValue = {}, std::string nodeId = {}) {
  doc.diagnostics.push_back({severity, std::move(message), std::move(context),
                             std::move(rawValue), std::move(nodeId)});
}

// Builds a deterministic identity from typed source indexes.
std::string ChildId(const std::string &parent, const char *type, int index) {
  return parent + "/" + type + "[" + std::to_string(index) + "]";
}

// Parses a positive integer with overflow detection.
bool ParseUInt(const std::string &text, std::uint64_t &out) {
  if (text.empty())
    return false;
  out = 0;
  for (unsigned char c : text) {
    if (!std::isdigit(c))
      return false;
    const std::uint64_t digit = c - '0';
    if (out > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
      return false;
    out = out * 10 + digit;
  }
  return true;
}

// Parses a signed integer attribute without throwing.
bool ParseIntText(const std::string &text, int &out) {
  if (text.empty())
    return false;
  size_t pos = 0;
  bool negative = false;
  if (text[pos] == '-' || text[pos] == '+') {
    negative = text[pos] == '-';
    ++pos;
  }
  if (pos >= text.size())
    return false;
  std::uint64_t magnitude = 0;
  for (; pos < text.size(); ++pos) {
    const unsigned char c = static_cast<unsigned char>(text[pos]);
    if (!std::isdigit(c))
      return false;
    const std::uint64_t digit = c - '0';
    if (magnitude > (static_cast<std::uint64_t>(std::numeric_limits<int>::max()) + (negative ? 1u : 0u) - digit) / 10)
      return false;
    magnitude = magnitude * 10 + digit;
  }
  if (negative) {
    if (magnitude == static_cast<std::uint64_t>(std::numeric_limits<int>::max()) + 1u)
      out = std::numeric_limits<int>::min();
    else
      out = -static_cast<int>(magnitude);
  } else {
    out = static_cast<int>(magnitude);
  }
  return true;
}

// Returns the maximum raw DMX value for a resolution byte count.
std::uint64_t MaxForBytes(int bytes) {
  if (bytes <= 0)
    bytes = 1;
  if (bytes >= 8)
    return std::numeric_limits<std::uint64_t>::max();
  return (std::uint64_t{1} << (8 * bytes)) - 1;
}

// Returns the effective physical range, inheriting only presentation values.
GdtfPhysicalRange PhysicalRange(const std::string &from, const std::string &to,
                                const GdtfPhysicalRange *parent = nullptr) {
  GdtfPhysicalRange range;
  if (!from.empty()) {
    range.from = from;
    range.fromOrigin = GdtfValueOrigin::Explicit;
  } else if (parent && parent->available) {
    range.from = parent->from;
    range.fromOrigin = GdtfValueOrigin::Inherited;
  }
  if (!to.empty()) {
    range.to = to;
    range.toOrigin = GdtfValueOrigin::Explicit;
  } else if (parent && parent->available) {
    range.to = parent->to;
    range.toOrigin = GdtfValueOrigin::Inherited;
  }
  range.available = !range.from.empty() || !range.to.empty();
  return range;
}

// Reads GDTF Attribute definitions for unit/detail resolution.
std::map<std::string, GdtfAttributeInfo> ReadAttributes(const tinyxml2::XMLElement *fixtureType) {
  std::map<std::string, GdtfAttributeInfo> result;
  const auto *defs = fixtureType ? fixtureType->FirstChildElement("AttributeDefinitions") : nullptr;
  const auto *attrs = defs ? defs->FirstChildElement("Attributes") : nullptr;
  for (const auto *a = attrs ? attrs->FirstChildElement("Attribute") : nullptr; a;
       a = a->NextSiblingElement("Attribute")) {
    GdtfAttributeInfo info;
    info.name = Attr(a, "Name");
    info.pretty = Attr(a, "Pretty");
    info.physicalUnit = Attr(a, "PhysicalUnit");
    info.feature = Attr(a, "Feature");
    info.activationGroup = Attr(a, "ActivationGroup");
    info.mainAttribute = Attr(a, "MainAttribute");
    info.color = Attr(a, "Color");
    info.resolved = !info.name.empty();
    if (!info.name.empty())
      result[info.name] = info;
  }
  return result;
}

// Parses comma-separated offsets while preserving source order.
std::vector<GdtfDmxValue> ParseOffsets(const std::string &raw, GdtfModeChannelDocument &doc,
                                       const std::string &nodeId) {
  std::vector<GdtfDmxValue> values;
  std::stringstream input(raw);
  std::string token;
  while (std::getline(input, token, ',')) {
    auto parsed = ParseGdtfDmxValue(token);
    if (!parsed.valid)
      AddDiagnostic(doc, GdtfDiagnosticSeverity::Warning, "Malformed Offset value.",
                    "DMXChannel", token, nodeId);
    values.push_back(std::move(parsed));
  }
  return values;
}

// Finds a geometry element by exact Name below a geometry subtree.
const tinyxml2::XMLElement *FindGeometryByName(const tinyxml2::XMLElement *node,
                                               const std::string &name) {
  if (!node || name.empty())
    return nullptr;
  if (Attr(node, "Name") == name)
    return node;
  for (const auto *child = node->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    if (const auto *found = FindGeometryByName(child, name))
      return found;
  }
  return nullptr;
}

// Collects DMX offset shifts introduced by GeometryReference nodes.
void CollectGeometryReferenceOffsets(const tinyxml2::XMLElement *node,
                                     std::map<std::string, std::vector<int>> &offsets,
                                     GdtfModeChannelDocument &doc,
                                     const std::string &modeId) {
  if (!node)
    return;
  if (std::string(node->Name()) == "GeometryReference") {
    const std::string referencedGeometry = Attr(node, "Geometry");
    if (!referencedGeometry.empty()) {
      bool hadBreak = false;
      for (const auto *br = node->FirstChildElement("Break"); br;
           br = br->NextSiblingElement("Break")) {
        hadBreak = true;
        int dmxOffset = 1;
        const std::string rawOffset = Attr(br, "DMXOffset");
        if (!rawOffset.empty() && !ParseIntText(Trim(rawOffset), dmxOffset))
          AddDiagnostic(doc, GdtfDiagnosticSeverity::Warning,
                        "Malformed GeometryReference DMXOffset.",
                        "GeometryReference", rawOffset, modeId);
        offsets[referencedGeometry].push_back(std::max(0, dmxOffset - 1));
      }
      if (!hadBreak)
        offsets[referencedGeometry].push_back(0);
    }
  }
  for (const auto *child = node->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    CollectGeometryReferenceOffsets(child, offsets, doc, modeId);
  }
}

// Builds per-geometry DMX offset shifts for the selected mode root geometry.
std::map<std::string, std::vector<int>> BuildGeometryReferenceOffsets(
    const tinyxml2::XMLElement *geometries, const std::string &rootGeometryName,
    GdtfModeChannelDocument &doc, const std::string &modeId) {
  std::map<std::string, std::vector<int>> offsets;
  if (!geometries || rootGeometryName.empty())
    return offsets;
  const tinyxml2::XMLElement *rootGeometry = nullptr;
  for (const auto *geometry = geometries->FirstChildElement(); geometry && !rootGeometry;
       geometry = geometry->NextSiblingElement()) {
    rootGeometry = FindGeometryByName(geometry, rootGeometryName);
  }
  CollectGeometryReferenceOffsets(rootGeometry, offsets, doc, modeId);
  return offsets;
}

// Returns offsets shifted by a GeometryReference DMXOffset value.
std::vector<GdtfDmxValue> ShiftOffsets(const std::vector<GdtfDmxValue> &offsets,
                                       int shift) {
  std::vector<GdtfDmxValue> shifted = offsets;
  for (auto &offset : shifted) {
    if (!offset.valid || !offset.normalized)
      continue;
    offset.normalized = *offset.normalized + static_cast<std::uint64_t>(std::max(0, shift));
  }
  return shifted;
}

// Rebuilds stable child identities after a GeometryReference expansion clone.
void RebaseChannelIds(GdtfDmxChannelNode &channel, const std::string &channelId) {
  channel.id = channelId;
  for (size_t l = 0; l < channel.logicalChannels.size(); ++l) {
    auto &logical = channel.logicalChannels[l];
    logical.id = ChildId(channel.id, "logical", static_cast<int>(l));
    for (size_t f = 0; f < logical.channelFunctions.size(); ++f) {
      auto &fn = logical.channelFunctions[f];
      fn.id = ChildId(logical.id, "function", static_cast<int>(f));
      for (size_t s = 0; s < fn.channelSets.size(); ++s) {
        auto &set = fn.channelSets[s];
        set.id = ChildId(fn.id, "set", static_cast<int>(s));
        for (size_t sub = 0; sub < set.subChannelSets.size(); ++sub)
          set.subChannelSets[sub].id = ChildId(set.id, "subset", static_cast<int>(sub));
      }
    }
  }
}

// Calculates ordered ranges for channel functions and channel sets.
void CalculateRanges(GdtfDmxChannelNode &channel, GdtfModeChannelDocument &doc) {
  const std::uint64_t parentMax = MaxForBytes(std::max(1, channel.resolution));
  for (auto &logical : channel.logicalChannels) {
    for (size_t i = 0; i < logical.channelFunctions.size(); ++i) {
      auto &fn = logical.channelFunctions[i];
      if (!fn.parsedDmxFrom.valid) {
        AddDiagnostic(doc, GdtfDiagnosticSeverity::Warning, "Invalid ChannelFunction DMXFrom.",
                      "ChannelFunction", fn.rawDmxFrom, fn.id);
        continue;
      }
      const std::uint64_t start = *fn.parsedDmxFrom.normalized;
      std::optional<std::uint64_t> end;
      for (size_t j = i + 1; j < logical.channelFunctions.size(); ++j) {
        const auto &next = logical.channelFunctions[j].parsedDmxFrom;
        if (!next.valid)
          continue;
        const std::uint64_t nextStart = *next.normalized;
        if (nextStart <= start) {
          AddDiagnostic(doc, GdtfDiagnosticSeverity::Warning,
                        "Duplicate or descending ChannelFunction DMX range start.",
                        "ChannelFunction", logical.channelFunctions[j].rawDmxFrom,
                        logical.channelFunctions[j].id);
          continue;
        }
        end = nextStart - 1;
        break;
      }
      if (!end)
        end = parentMax;
      fn.effectiveDmxRange = GdtfDmxRange{start, *end, GdtfValueOrigin::Explicit,
                                          GdtfValueOrigin::Calculated};
      fn.effectivePhysicalRange = PhysicalRange(fn.rawPhysicalFrom, fn.rawPhysicalTo);

      for (size_t s = 0; s < fn.channelSets.size(); ++s) {
        auto &set = fn.channelSets[s];
        if (!set.parsedDmxFrom.valid) {
          AddDiagnostic(doc, GdtfDiagnosticSeverity::Warning, "Invalid ChannelSet DMXFrom.",
                        "ChannelSet", set.rawDmxFrom, set.id);
          continue;
        }
        const std::uint64_t setStart = *set.parsedDmxFrom.normalized;
        std::uint64_t setEnd = fn.effectiveDmxRange->end;
        for (size_t n = s + 1; n < fn.channelSets.size(); ++n) {
          const auto &next = fn.channelSets[n].parsedDmxFrom;
          if (next.valid && *next.normalized > setStart) {
            setEnd = *next.normalized - 1;
            break;
          }
        }
        if (setStart < fn.effectiveDmxRange->start || setStart > fn.effectiveDmxRange->end) {
          AddDiagnostic(doc, GdtfDiagnosticSeverity::Warning,
                        "ChannelSet DMX range starts outside the parent ChannelFunction range.",
                        "ChannelSet", set.rawDmxFrom, set.id);
        }
        set.effectiveDmxRange = GdtfDmxRange{setStart, setEnd, GdtfValueOrigin::Explicit,
                                            GdtfValueOrigin::Calculated};
        set.effectivePhysicalRange = PhysicalRange(set.rawPhysicalFrom, set.rawPhysicalTo,
                                                   &fn.effectivePhysicalRange);
      }
    }
  }
}
} // namespace

// Parses a GDTF DMXValue, including value/n and value/ns forms.
GdtfDmxValue ParseGdtfDmxValue(const std::string &raw, int parentResolutionBytes) {
  GdtfDmxValue result;
  result.raw = raw;
  const std::string text = Trim(raw);
  std::string valueText = text;
  std::string suffix;
  const size_t slash = text.find('/');
  if (slash != std::string::npos) {
    valueText = Trim(text.substr(0, slash));
    suffix = Trim(text.substr(slash + 1));
    if (!suffix.empty() && suffix.back() == 's') {
      result.mode = GdtfDmxValueMode::Shift;
      suffix.pop_back();
    } else {
      result.mode = GdtfDmxValueMode::Mirror;
    }
    std::uint64_t bytes = 0;
    if (!ParseUInt(suffix, bytes) || bytes == 0 || bytes > 8) {
      result.error = "Invalid DMXValue byte count";
      return result;
    }
    result.byteCount = static_cast<int>(bytes);
  } else {
    result.byteCount = parentResolutionBytes > 0 ? parentResolutionBytes : 1;
  }
  std::uint64_t value = 0;
  if (!ParseUInt(valueText, value)) {
    result.error = "Invalid DMXValue integer";
    return result;
  }
  if (value > MaxForBytes(result.byteCount)) {
    result.error = "DMXValue overflow";
    return result;
  }
  result.value = value;
  result.normalized = result.mode == GdtfDmxValueMode::Shift && parentResolutionBytes > result.byteCount
                          ? value << (8 * (parentResolutionBytes - result.byteCount))
                          : value;
  result.valid = true;
  return result;
}

// Finds a parsed DMX mode by exact name.
const GdtfDmxModeNode *GdtfModeChannelDocument::FindMode(const std::string &modeName) const {
  for (const auto &mode : modes) {
    if (mode.name == modeName)
      return &mode;
  }
  return nullptr;
}

// Reads hierarchical DMX mode/channel data from GDTF description.xml text.
GdtfModeChannelDocument ReadGdtfModeChannelDocument(const std::string &descriptionXml) {
  GdtfModeChannelDocument result;
  tinyxml2::XMLDocument xml;
  if (xml.Parse(descriptionXml.c_str(), descriptionXml.size()) != tinyxml2::XML_SUCCESS) {
    AddDiagnostic(result, GdtfDiagnosticSeverity::Error, "Malformed GDTF description XML.");
    return result;
  }
  const auto *root = xml.FirstChildElement("GDTF");
  const auto *fixtureType = root ? root->FirstChildElement("FixtureType") : nullptr;
  const auto *dmxModes = fixtureType ? fixtureType->FirstChildElement("DMXModes") : nullptr;
  const auto *geometries = fixtureType ? fixtureType->FirstChildElement("Geometries") : nullptr;
  if (!dmxModes) {
    AddDiagnostic(result, GdtfDiagnosticSeverity::Error, "Missing DMXModes element.");
    return result;
  }
  const auto attributes = ReadAttributes(fixtureType);
  int modeIndex = 0;
  for (const auto *modeXml = dmxModes->FirstChildElement("DMXMode"); modeXml;
       modeXml = modeXml->NextSiblingElement("DMXMode"), ++modeIndex) {
    GdtfDmxModeNode mode;
    mode.sourceIndex = modeIndex;
    mode.name = Attr(modeXml, "Name");
    mode.description = Attr(modeXml, "Description");
    mode.geometry = Attr(modeXml, "Geometry");
    mode.id = "mode[" + std::to_string(modeIndex) + "]:" + mode.name;
    const auto geometryReferenceOffsets =
        BuildGeometryReferenceOffsets(geometries, mode.geometry, result, mode.id);
    const auto *channelsXml = modeXml->FirstChildElement("DMXChannels");
    int channelIndex = 0;
    for (const auto *channelXml = channelsXml ? channelsXml->FirstChildElement("DMXChannel") : nullptr;
         channelXml; channelXml = channelXml->NextSiblingElement("DMXChannel"), ++channelIndex) {
      GdtfDmxChannelNode channel;
      channel.sourceIndex = channelIndex;
      channel.id = ChildId(mode.id, "channel", channelIndex);
      channel.rawDmxBreak = Attr(channelXml, "DMXBreak");
      channel.rawOffset = Attr(channelXml, "Offset");
      channel.initialFunction = Attr(channelXml, "InitialFunction");
      channel.highlight = Attr(channelXml, "Highlight");
      channel.geometry = Attr(channelXml, "Geometry");
      channel.virtualChannel = channel.rawOffset.empty() || channel.rawOffset == "None";
      if (!channel.virtualChannel) {
        channel.offsets = ParseOffsets(channel.rawOffset, result, channel.id);
        channel.resolution = static_cast<int>(channel.offsets.size());
        for (const auto &offset : channel.offsets) {
          if (offset.valid && offset.normalized)
            mode.calculatedFootprint = std::max(mode.calculatedFootprint,
                                                static_cast<int>(*offset.normalized));
        }
      }
      int logicalIndex = 0;
      for (const auto *logicalXml = channelXml->FirstChildElement("LogicalChannel"); logicalXml;
           logicalXml = logicalXml->NextSiblingElement("LogicalChannel"), ++logicalIndex) {
        GdtfLogicalChannelNode logical;
        logical.sourceIndex = logicalIndex;
        logical.id = ChildId(channel.id, "logical", logicalIndex);
        logical.attribute = Attr(logicalXml, "Attribute");
        logical.snap = Attr(logicalXml, "Snap");
        logical.master = Attr(logicalXml, "Master");
        logical.mibFade = Attr(logicalXml, "MibFade");
        logical.dmxChangeTimeLimit = Attr(logicalXml, "DMXChangeTimeLimit");
        auto attrIt = attributes.find(logical.attribute);
        if (attrIt != attributes.end())
          logical.attributeInfo = attrIt->second;
        else if (!logical.attribute.empty())
          AddDiagnostic(result, GdtfDiagnosticSeverity::Warning, "Unresolved Attribute reference.",
                        "LogicalChannel", logical.attribute, logical.id);
        int functionIndex = 0;
        for (const auto *fnXml = logicalXml->FirstChildElement("ChannelFunction"); fnXml;
             fnXml = fnXml->NextSiblingElement("ChannelFunction"), ++functionIndex) {
          GdtfChannelFunctionNode fn;
          fn.sourceIndex = functionIndex;
          fn.id = ChildId(logical.id, "function", functionIndex);
          fn.name = Attr(fnXml, "Name");
          fn.attribute = Attr(fnXml, "Attribute");
          fn.originalAttribute = Attr(fnXml, "OriginalAttribute");
          fn.rawDmxFrom = Attr(fnXml, "DMXFrom");
          fn.rawDefault = Attr(fnXml, "Default");
          fn.rawPhysicalFrom = Attr(fnXml, "PhysicalFrom");
          fn.rawPhysicalTo = Attr(fnXml, "PhysicalTo");
          fn.physicalUnit = Attr(fnXml, "PhysicalUnit");
          fn.realFade = Attr(fnXml, "RealFade");
          fn.realAcceleration = Attr(fnXml, "RealAcceleration");
          fn.wheel = Attr(fnXml, "Wheel");
          fn.emitter = Attr(fnXml, "Emitter");
          fn.filter = Attr(fnXml, "Filter");
          fn.colorSpace = Attr(fnXml, "ColorSpace");
          fn.gamut = Attr(fnXml, "Gamut");
          fn.modeMaster = Attr(fnXml, "ModeMaster");
          fn.modeFrom = Attr(fnXml, "ModeFrom");
          fn.modeTo = Attr(fnXml, "ModeTo");
          fn.dmxProfile = Attr(fnXml, "DMXProfile");
          fn.min = Attr(fnXml, "Min");
          fn.max = Attr(fnXml, "Max");
          fn.customName = Attr(fnXml, "CustomName");
          if (fn.physicalUnit.empty()) {
            auto fnAttr = attributes.find(fn.attribute.empty() ? logical.attribute : fn.attribute);
            if (fnAttr != attributes.end())
              fn.physicalUnit = fnAttr->second.physicalUnit;
          }
          fn.parsedDmxFrom = ParseGdtfDmxValue(fn.rawDmxFrom.empty() ? "0" : fn.rawDmxFrom,
                                               std::max(1, channel.resolution));
          int setIndex = 0;
          for (const auto *setXml = fnXml->FirstChildElement("ChannelSet"); setXml;
               setXml = setXml->NextSiblingElement("ChannelSet"), ++setIndex) {
            GdtfChannelSetNode set;
            set.sourceIndex = setIndex;
            set.id = ChildId(fn.id, "set", setIndex);
            set.name = Attr(setXml, "Name");
            set.rawDmxFrom = Attr(setXml, "DMXFrom");
            set.rawPhysicalFrom = Attr(setXml, "PhysicalFrom");
            set.rawPhysicalTo = Attr(setXml, "PhysicalTo");
            set.wheelSlotIndex = Attr(setXml, "WheelSlotIndex");
            set.parsedDmxFrom = ParseGdtfDmxValue(set.rawDmxFrom.empty() ? "0" : set.rawDmxFrom,
                                                  std::max(1, channel.resolution));
            int subIndex = 0;
            for (const auto *subXml = setXml->FirstChildElement("SubChannelSet"); subXml;
                 subXml = subXml->NextSiblingElement("SubChannelSet"), ++subIndex) {
              GdtfSubChannelSetNode sub;
              sub.sourceIndex = subIndex;
              sub.id = ChildId(set.id, "subset", subIndex);
              sub.name = Attr(subXml, "Name");
              sub.physicalFrom = Attr(subXml, "PhysicalFrom");
              sub.physicalTo = Attr(subXml, "PhysicalTo");
              sub.subPhysicalUnit = Attr(subXml, "SubPhysicalUnit");
              sub.dmxProfile = Attr(subXml, "DMXProfile");
              set.subChannelSets.push_back(std::move(sub));
            }
            fn.channelSets.push_back(std::move(set));
          }
          logical.channelFunctions.push_back(std::move(fn));
        }
        channel.logicalChannels.push_back(std::move(logical));
      }
      CalculateRanges(channel, result);
      auto geometryShifts = geometryReferenceOffsets.find(channel.geometry);
      if (!channel.virtualChannel && geometryShifts != geometryReferenceOffsets.end() &&
          !geometryShifts->second.empty()) {
        for (size_t shiftIndex = 0; shiftIndex < geometryShifts->second.size(); ++shiftIndex) {
          GdtfDmxChannelNode expanded = channel;
          expanded.geometryReferenceIndex = static_cast<int>(shiftIndex + 1);
          expanded.offsets = ShiftOffsets(channel.offsets, geometryShifts->second[shiftIndex]);
          RebaseChannelIds(expanded, channel.id + "/geometryReference[" +
                                         std::to_string(shiftIndex) + "]");
          for (const auto &offset : expanded.offsets) {
            if (offset.valid && offset.normalized)
              mode.calculatedFootprint = std::max(mode.calculatedFootprint,
                                                  static_cast<int>(*offset.normalized));
          }
          mode.channels.push_back(std::move(expanded));
        }
      } else {
        mode.channels.push_back(std::move(channel));
      }
    }
    result.modes.push_back(std::move(mode));
  }
  return result;
}

} // namespace gdtf
