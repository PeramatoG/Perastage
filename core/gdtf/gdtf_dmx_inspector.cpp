#include "gdtf/gdtf_dmx_inspector.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gdtf {
namespace {
// Adds an inspector diagnostic.
void AddDiagnostic(std::vector<GdtfModeDiagnostic> &diagnostics,
                   GdtfDiagnosticSeverity severity, std::string message,
                   std::string context = {}, std::string rawValue = {},
                   std::string nodeId = {}) {
  diagnostics.push_back({severity, std::move(message), std::move(context),
                         std::move(rawValue), std::move(nodeId)});
}

// Returns the maximum DMX value represented by a channel resolution.
std::uint64_t MaxForResolution(int bytes) {
  if (bytes <= 0)
    return 255;
  if (bytes >= 8)
    return UINT64_MAX;
  return (std::uint64_t{1} << (8 * bytes)) - 1;
}

// Finds a channel by stable id.
const GdtfDmxChannelNode *FindChannel(const GdtfDmxModeNode &mode,
                                      const std::string &channelId) {
  for (const auto &channel : mode.channels) {
    if (channel.id == channelId)
      return &channel;
  }
  return nullptr;
}

// Reports whether a value is inside an effective range.
bool Contains(const std::optional<GdtfDmxRange> &range, std::uint64_t value) {
  return range && value >= range->start && value <= range->end;
}

// Parses a positive 1-based slot index.
std::optional<int> ParseSlotIndex(const std::string &text) {
  if (text.empty())
    return std::nullopt;
  int value = 0;
  for (unsigned char c : text) {
    if (c < '0' || c > '9')
      return std::nullopt;
    value = value * 10 + (c - '0');
  }
  if (value <= 0)
    return std::nullopt;
  return value;
}

// Parses a floating point presentation value.
bool ParseDouble(const std::string &text, double &value) {
  std::stringstream input(text);
  return (input >> value) && input.eof();
}

// Formats an interpolated physical value for read-only display.
std::string InterpolatePhysical(const GdtfDmxRange &dmx,
                                const GdtfPhysicalRange &physical,
                                std::uint64_t value) {
  double from = 0.0;
  double to = 0.0;
  if (!physical.available || !ParseDouble(physical.from, from) || !ParseDouble(physical.to, to))
    return {};
  const double span = dmx.end > dmx.start ? static_cast<double>(dmx.end - dmx.start) : 0.0;
  const double t = span > 0.0 ? static_cast<double>(value - dmx.start) / span : 0.0;
  std::ostringstream out;
  out << (from + (to - from) * t);
  return out.str();
}

// Decomposes a normalized value into coarse-to-fine DMX bytes.
std::vector<unsigned int> DecomposeBytes(std::uint64_t value, int resolution) {
  std::vector<unsigned int> bytes;
  resolution = std::clamp(resolution, 1, 8);
  for (int i = resolution - 1; i >= 0; --i)
    bytes.push_back(static_cast<unsigned int>((value >> (i * 8)) & 0xffu));
  return bytes;
}
} // namespace

// Resolves the active functions, sets, wheel slots, resources, and display values.
GdtfDmxInspectionResult InspectGdtfDmxValue(const GdtfDmxModeNode &mode,
                                            const std::string &channelId,
                                            std::uint64_t normalizedValue,
                                            const GdtfWheelCatalog &catalog) {
  GdtfDmxInspectionResult result;
  result.modeId = mode.id;
  result.channelId = channelId;
  const auto *channel = FindChannel(mode, channelId);
  if (!channel) {
    AddDiagnostic(result.diagnostics, GdtfDiagnosticSeverity::Error,
                  "Selected DMX channel was not found.", "DMXChannel", {}, channelId);
    return result;
  }
  result.virtualChannel = channel->virtualChannel;
  result.maxValue = MaxForResolution(std::max(1, channel->resolution));
  result.normalizedValue = std::min(normalizedValue, result.maxValue);
  result.bytes = DecomposeBytes(result.normalizedValue, std::max(1, channel->resolution));

  for (const auto &logical : channel->logicalChannels) {
    const GdtfChannelFunctionNode *activeFunction = nullptr;
    for (const auto &function : logical.channelFunctions) {
      if (Contains(function.effectiveDmxRange, result.normalizedValue)) {
        activeFunction = &function;
        break;
      }
    }
    if (!activeFunction) {
      GdtfDmxInspectorMapping mapping;
      mapping.logicalChannelId = logical.id;
      mapping.logicalAttribute = logical.attribute;
      AddDiagnostic(mapping.diagnostics, GdtfDiagnosticSeverity::Warning,
                    "No ChannelFunction contains the inspected DMX value.",
                    "LogicalChannel", {}, logical.id);
      result.mappings.push_back(std::move(mapping));
      continue;
    }
    GdtfDmxInspectorMapping mapping;
    mapping.logicalChannelId = logical.id;
    mapping.logicalAttribute = logical.attribute;
    mapping.channelFunctionId = activeFunction->id;
    mapping.channelFunctionName = activeFunction->name.empty() ? activeFunction->attribute : activeFunction->name;
    mapping.channelFunctionDmxRange = activeFunction->effectiveDmxRange;
    mapping.physicalUnit = activeFunction->physicalUnit.empty() ? logical.attributeInfo.physicalUnit : activeFunction->physicalUnit;
    mapping.modeMaster = activeFunction->modeMaster;
    mapping.modeMasterConditional = !activeFunction->modeMaster.empty();
    if (!activeFunction->dmxProfile.empty()) {
      mapping.physicalApproximate = true;
      AddDiagnostic(mapping.diagnostics, GdtfDiagnosticSeverity::Info,
                    "Physical value is approximate because a DMXProfile is referenced.",
                    "ChannelFunction", activeFunction->dmxProfile, activeFunction->id);
    }
    if (activeFunction->effectiveDmxRange)
      mapping.physicalValue = InterpolatePhysical(*activeFunction->effectiveDmxRange,
                                                  activeFunction->effectivePhysicalRange,
                                                  result.normalizedValue);
    const GdtfChannelSetNode *activeSet = nullptr;
    for (const auto &set : activeFunction->channelSets) {
      if (Contains(set.effectiveDmxRange, result.normalizedValue)) {
        activeSet = &set;
        break;
      }
    }
    if (activeSet) {
      mapping.channelSetId = activeSet->id;
      mapping.channelSetName = activeSet->name;
      mapping.channelSetDmxRange = activeSet->effectiveDmxRange;
      if (activeSet->effectiveDmxRange) {
        const std::string setPhysical = InterpolatePhysical(*activeSet->effectiveDmxRange,
                                                            activeSet->effectivePhysicalRange,
                                                            result.normalizedValue);
        if (!setPhysical.empty())
          mapping.physicalValue = setPhysical;
      }
    } else if (!activeFunction->channelSets.empty()) {
      AddDiagnostic(mapping.diagnostics, GdtfDiagnosticSeverity::Warning,
                    "No ChannelSet contains the inspected DMX value.",
                    "ChannelFunction", {}, activeFunction->id);
    }
    if (!activeFunction->wheel.empty()) {
      mapping.wheel = catalog.FindWheel(activeFunction->wheel);
      if (!mapping.wheel) {
        AddDiagnostic(mapping.diagnostics, GdtfDiagnosticSeverity::Warning,
                      "ChannelFunction wheel reference could not be resolved exactly.",
                      "ChannelFunction", activeFunction->wheel, activeFunction->id);
      } else if (activeSet) {
        const auto slotIndex = ParseSlotIndex(activeSet->wheelSlotIndex);
        if (!slotIndex) {
          AddDiagnostic(mapping.diagnostics, GdtfDiagnosticSeverity::Warning,
                        "WheelSlotIndex is missing or invalid; no slot was guessed.",
                        "ChannelSet", activeSet->wheelSlotIndex, activeSet->id);
        } else if (*slotIndex > static_cast<int>(mapping.wheel->slots.size())) {
          AddDiagnostic(mapping.diagnostics, GdtfDiagnosticSeverity::Warning,
                        "WheelSlotIndex is outside the resolved wheel slot range.",
                        "ChannelSet", activeSet->wheelSlotIndex, activeSet->id);
        } else {
          mapping.slot = &mapping.wheel->slots[static_cast<size_t>(*slotIndex - 1)];
          mapping.mediaResource = mapping.slot->mediaFileName;
          mapping.graphicWheelResource = mapping.slot->graphicWheelResource;
          if (!mapping.slot->rawFilter.empty())
            mapping.filter = catalog.FindFilter(mapping.slot->rawFilter);
        }
      }
    }
    result.mappings.push_back(std::move(mapping));
  }
  return result;
}

} // namespace gdtf
