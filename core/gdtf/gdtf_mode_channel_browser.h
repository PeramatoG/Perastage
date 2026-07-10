/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gdtf {

enum class GdtfValueOrigin { Explicit, Inherited, SpecificationDefault, Calculated, Unavailable };
enum class GdtfDiagnosticSeverity { Info, Warning, Error };
enum class GdtfDmxValueMode { Plain, Mirror, Shift };

struct GdtfModeDiagnostic {
  GdtfDiagnosticSeverity severity = GdtfDiagnosticSeverity::Info;
  std::string message;
  std::string context;
  std::string rawValue;
  std::string nodeId;
};

struct GdtfDmxValue {
  std::string raw;
  std::optional<std::uint64_t> value;
  int byteCount = 0;
  GdtfDmxValueMode mode = GdtfDmxValueMode::Plain;
  std::optional<std::uint64_t> normalized;
  bool valid = false;
  std::string error;
};

struct GdtfDmxRange {
  std::uint64_t start = 0;
  std::uint64_t end = 0;
  GdtfValueOrigin startOrigin = GdtfValueOrigin::Explicit;
  GdtfValueOrigin endOrigin = GdtfValueOrigin::Calculated;
};

struct GdtfPhysicalRange {
  std::string from;
  std::string to;
  GdtfValueOrigin fromOrigin = GdtfValueOrigin::Unavailable;
  GdtfValueOrigin toOrigin = GdtfValueOrigin::Unavailable;
  bool available = false;
};

struct GdtfAttributeInfo {
  std::string name;
  std::string pretty;
  std::string physicalUnit;
  std::string feature;
  std::string activationGroup;
  std::string mainAttribute;
  std::string color;
  bool resolved = false;
};

struct GdtfSubChannelSetNode {
  std::string id;
  std::string name;
  std::string physicalFrom;
  std::string physicalTo;
  std::string subPhysicalUnit;
  std::string dmxProfile;
  int sourceIndex = 0;
};

struct GdtfChannelSetNode {
  std::string id;
  std::string name;
  std::string rawDmxFrom;
  GdtfDmxValue parsedDmxFrom;
  std::optional<GdtfDmxRange> effectiveDmxRange;
  std::string rawPhysicalFrom;
  std::string rawPhysicalTo;
  GdtfPhysicalRange effectivePhysicalRange;
  std::string wheelSlotIndex;
  int sourceIndex = 0;
  std::vector<GdtfSubChannelSetNode> subChannelSets;
};

struct GdtfChannelFunctionNode {
  std::string id;
  std::string name;
  std::string attribute;
  std::string originalAttribute;
  std::string rawDmxFrom;
  GdtfDmxValue parsedDmxFrom;
  std::optional<GdtfDmxRange> effectiveDmxRange;
  std::string rawDefault;
  std::string rawPhysicalFrom;
  std::string rawPhysicalTo;
  GdtfPhysicalRange effectivePhysicalRange;
  std::string physicalUnit;
  std::string realFade;
  std::string realAcceleration;
  std::string wheel;
  std::string emitter;
  std::string filter;
  std::string colorSpace;
  std::string gamut;
  std::string modeMaster;
  std::string modeFrom;
  std::string modeTo;
  std::string dmxProfile;
  std::string min;
  std::string max;
  std::string customName;
  int sourceIndex = 0;
  std::vector<GdtfChannelSetNode> channelSets;
};

struct GdtfLogicalChannelNode {
  std::string id;
  std::string attribute;
  std::string snap;
  std::string master;
  std::string mibFade;
  std::string dmxChangeTimeLimit;
  GdtfAttributeInfo attributeInfo;
  int sourceIndex = 0;
  std::vector<GdtfChannelFunctionNode> channelFunctions;
};

struct GdtfDmxChannelNode {
  std::string id;
  std::string rawDmxBreak;
  std::string rawOffset;
  std::vector<GdtfDmxValue> offsets;
  bool virtualChannel = false;
  int resolution = 0;
  std::string initialFunction;
  std::string highlight;
  std::string geometry;
  int sourceIndex = 0;
  std::vector<GdtfLogicalChannelNode> logicalChannels;
};

struct GdtfDmxModeNode {
  std::string id;
  std::string name;
  std::string description;
  std::string geometry;
  int sourceIndex = 0;
  int calculatedFootprint = 0;
  std::vector<GdtfDmxChannelNode> channels;
};

struct GdtfModeChannelDocument {
  std::vector<GdtfDmxModeNode> modes;
  std::vector<GdtfModeDiagnostic> diagnostics;
  const GdtfDmxModeNode *FindMode(const std::string &modeName) const;
};

GdtfDmxValue ParseGdtfDmxValue(const std::string &raw, int parentResolutionBytes = 0);
GdtfModeChannelDocument ReadGdtfModeChannelDocument(const std::string &descriptionXml);

} // namespace gdtf
