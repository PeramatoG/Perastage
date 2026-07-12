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

#include "gdtf/gdtf_mode_channel_browser.h"
#include "gdtf/gdtf_wheel_catalog.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gdtf {

struct GdtfDmxInspectorMapping {
  std::string logicalChannelId;
  std::string logicalAttribute;
  std::string channelFunctionId;
  std::string channelFunctionName;
  std::string channelSetId;
  std::string channelSetName;
  std::string physicalValue;
  std::string physicalUnit;
  bool physicalApproximate = false;
  bool modeMasterConditional = false;
  std::string modeMaster;
  const GdtfCatalogWheelInfo *wheel = nullptr;
  const GdtfCatalogWheelSlotInfo *slot = nullptr;
  const GdtfCatalogFilterInfo *filter = nullptr;
  std::string mediaResource;
  std::string graphicWheelResource;
  std::vector<GdtfModeDiagnostic> diagnostics;
};

struct GdtfDmxInspectionResult {
  std::string modeId;
  std::string channelId;
  std::uint64_t normalizedValue = 0;
  std::uint64_t maxValue = 255;
  std::vector<unsigned int> bytes;
  bool virtualChannel = false;
  std::vector<GdtfDmxInspectorMapping> mappings;
  std::vector<GdtfModeDiagnostic> diagnostics;
};

GdtfDmxInspectionResult InspectGdtfDmxValue(const GdtfDmxModeNode &mode,
                                            const std::string &channelId,
                                            std::uint64_t normalizedValue,
                                            const GdtfWheelCatalog &catalog);

} // namespace gdtf
