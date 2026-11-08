#include "gdtf/gdtf_channel_footprint.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <functional>
#include <set>
#include <sstream>
#include <tinyxml2.h>

namespace gdtf {
namespace {
using Element = tinyxml2::XMLElement;
std::string Attr(const Element* node, const char* key) {
  const char* value = node ? node->Attribute(key) : nullptr;
  return value ? value : "";
}
std::string Trim(const std::string& text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  return begin == std::string::npos ? "" :
      text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}
bool Positive(const std::string& raw, int& value) {
  const auto text = Trim(raw);
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value > 0;
}
struct Channel {
  const Element* xml;
  int sourceBreak = 1;
  bool overwrite = false;
  bool valid = true;
  std::vector<int> offsets;
};
struct Instance {
  const Element* geometry;
  const Element* reference = nullptr;
  int index = 0;
};
}
int GdtfModeFootprint::PrimaryFootprint() const {
  const auto found = footprintsByBreak.find(1);
  return found == footprintsByBreak.end() ? 0 : found->second;
}
GdtfModeFootprint ResolveGdtfModeFootprint(const Element* geometries, const Element* mode) {
  GdtfModeFootprint result;
  auto diagnose = [&](const std::string& message) {
    result.diagnostics.push_back("DMXMode '" + Attr(mode, "Name") + "': " + message);
  };
  std::vector<Channel> channels;
  const auto* container = mode ? mode->FirstChildElement("DMXChannels") : nullptr;
  for (auto* xml = container ? container->FirstChildElement("DMXChannel") : nullptr;
       xml; xml = xml->NextSiblingElement("DMXChannel")) {
    Channel channel{xml};
    GdtfChannelAddressResolution resolved;
    const auto raw = Trim(Attr(xml, "Offset"));
    resolved.virtualChannel = raw.empty() || raw == "None";
    if (!resolved.virtualChannel) {
      std::stringstream input(raw);
      std::string token;
      while (std::getline(input, token, ',')) {
        int offset = 0;
        if (!Positive(token, offset)) channel.valid = false;
        channel.offsets.push_back(offset);
      }
      if (raw.back() == ',' || channel.offsets.size() > 4) channel.valid = false;
      if (!channel.valid) diagnose("Invalid physical Offset '" + raw + "'.");
    }
    const auto rawBreak = Attr(xml, "DMXBreak");
    channel.overwrite = rawBreak == "Overwrite";
    if (!channel.overwrite && !rawBreak.empty() && !Positive(rawBreak, channel.sourceBreak)) {
      channel.valid = false;
      diagnose("Invalid DMXBreak '" + rawBreak + "'.");
    }
    channels.push_back(std::move(channel));
    result.channels.push_back(std::move(resolved));
  }
  std::map<std::string, std::vector<const Element*>> definitions;
  for (auto* node = geometries ? geometries->FirstChildElement() : nullptr;
       node; node = node->NextSiblingElement())
    definitions[Attr(node, "Name")].push_back(node);
  auto definition = [&](const std::string& name) -> const Element* {
    const auto found = definitions.find(name);
    if (name.empty() || found == definitions.end() || found->second.size() != 1) {
      diagnose("Missing or ambiguous top-level geometry '" + name + "'.");
      return nullptr;
    }
    return found->second.front();
  };
  std::vector<Instance> instances;
  int referenceIndex = 0;
  std::function<void(const Element*, const Element*, int, std::set<const Element*>)> walk;
  walk = [&](const Element* node, const Element* reference, int index,
             std::set<const Element*> ancestors) {
    if (!node) return;
    if (!ancestors.insert(node).second) {
      diagnose("Cyclic GeometryReference at '" + Attr(node, "Name") + "'.");
      return;
    }
    if (std::string(node->Name()) == "GeometryReference") {
      const auto* target = definition(Attr(node, "Geometry"));
      if (!target) return;
      if (ancestors.count(target)) {
        diagnose("Cyclic GeometryReference at '" + Attr(node, "Name") + "'.");
        return;
      }
      if (reference) {
        diagnose("Nested GeometryReference is unsupported at '" + Attr(node, "Name") + "'.");
        return;
      }
      walk(target, node, ++referenceIndex, std::move(ancestors));
      return;
    }
    instances.push_back({node, reference, index});
    for (auto* child = node->FirstChildElement(); child; child = child->NextSiblingElement())
      if (!Attr(child, "Name").empty()) walk(child, reference, index, ancestors);
  };
  const auto rootName = Attr(mode, "Geometry");
  if (!rootName.empty()) walk(definition(rootName), nullptr, 0, {});
  // Compatibility: older files without a mode geometry retain direct addresses.
  const bool unscoped = rootName.empty();
  if (unscoped && !channels.empty()) diagnose("Missing mode Geometry; using direct channel addresses.");
  for (size_t c = 0; c < channels.size(); ++c) {
    const auto& channel = channels[c];
    if (!channel.valid || result.channels[c].virtualChannel) continue;
    std::vector<Instance> matches;
    const auto geometry = Attr(channel.xml, "Geometry");
    if (unscoped || geometry.empty()) {
      if (unscoped || !instances.empty()) matches.push_back({nullptr});
    } else {
      for (const auto& instance : instances)
        if (Attr(instance.geometry, "Name") == geometry) matches.push_back(instance);
    }
    // Repeated instances are valid; repeated definitions within one instance are not.
    std::set<int> seen;
    bool ambiguous = false;
    for (const auto& match : matches) {
      if (!seen.insert(match.index).second || match.geometry != matches.front().geometry)
        ambiguous = true;
    }
    if (matches.empty() || ambiguous) {
      diagnose("Unresolved or ambiguous channel Geometry '" + geometry + "'.");
      continue;
    }
    for (const auto& match : matches) {
      int effectiveBreak = channel.sourceBreak;
      int shift = 0;
      if (match.reference) {
        // Break children correspond to sorted distinct source breaks (GDTF 1.2,
        // Geometry Type Reference); Overwrite uses the remaining break entry.
        std::set<int> sourceBreaks;
        bool hasOverwrite = false;
        for (const auto& other : channels) {
          const auto rawOffset = Trim(Attr(other.xml, "Offset"));
          if (!other.valid || rawOffset.empty() || rawOffset == "None") continue;
          const auto otherGeometry = Attr(other.xml, "Geometry");
          const bool belongs = std::any_of(instances.begin(), instances.end(), [&](const Instance& i) {
            return i.reference == match.reference && Attr(i.geometry, "Name") == otherGeometry;
          });
          if (!belongs) continue;
          if (other.overwrite) hasOverwrite = true;
          else sourceBreaks.insert(other.sourceBreak);
        }
        std::vector<const Element*> breaks;
        for (auto* br = match.reference->FirstChildElement("Break"); br;
             br = br->NextSiblingElement("Break")) breaks.push_back(br);
        const size_t slot = channel.overwrite ? sourceBreaks.size() :
            static_cast<size_t>(std::distance(sourceBreaks.begin(), sourceBreaks.find(channel.sourceBreak)));
        if (breaks.size() != sourceBreaks.size() + (hasOverwrite ? 1 : 0) || slot >= breaks.size()) {
          diagnose("Missing or ambiguous GeometryReference Break at '" + Attr(match.reference, "Name") + "'.");
          continue;
        }
        const auto rawBreak = Attr(breaks[slot], "DMXBreak");
        const auto rawOffset = Attr(breaks[slot], "DMXOffset");
        int base = 1;
        effectiveBreak = 1;
        if ((!rawBreak.empty() && (!Positive(rawBreak, effectiveBreak) || effectiveBreak > 255)) ||
            (!rawOffset.empty() && !Positive(rawOffset, base))) {
          diagnose("Invalid GeometryReference Break at '" + Attr(match.reference, "Name") + "'.");
          continue;
        }
        shift = base - 1;
      } else if (channel.overwrite) {
        diagnose("Overwrite requires a GeometryReference Break for '" + geometry + "'.");
        continue;
      }
      GdtfPhysicalChannelInstance expanded{effectiveBreak, match.index, {}};
      bool overflow = false;
      for (int offset : channel.offsets) {
        if (offset > INT_MAX - shift) { overflow = true; break; }
        expanded.offsets.push_back(offset + shift);
      }
      if (overflow) { diagnose("Physical DMX offset overflow."); continue; }
      for (int offset : expanded.offsets)
        result.footprintsByBreak[effectiveBreak] = std::max(result.footprintsByBreak[effectiveBreak], offset);
      result.channels[c].instances.push_back(std::move(expanded));
    }
  }
  if (result.footprintsByBreak.size() > 1 ||
      (!result.footprintsByBreak.empty() && !result.footprintsByBreak.count(1)))
    diagnose("Multiple/separate DMX breaks retained; the legacy Ch Count represents break 1 only.");
  return result;
}
} // namespace gdtf
