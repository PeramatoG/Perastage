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

#include "gdtf/gdtf_color_cie.h"

#include <optional>
#include <string>
#include <vector>

namespace gdtf {

struct GdtfWheelSlotInfo {
  std::string id;
  int index = 0;
  std::string name;
  std::string rawColor;
  GdtfColorCie color;
  std::string rawFilter;
  std::string mediaFileName;
  std::string resolvedResourcePath;
  std::vector<std::string> prismFacets;
  std::vector<std::string> animationSystems;
  std::string graphicWheelReference;
  std::string graphicWheelResource;
  std::vector<GdtfModeDiagnostic> diagnostics;
};

struct GdtfWheelInfo {
  std::string id;
  std::string name;
  int sourceIndex = 0;
  std::string type;
  bool graphicWheel = false;
  std::vector<GdtfWheelSlotInfo> slots;
  std::vector<GdtfModeDiagnostic> diagnostics;
};

struct GdtfFilterInfo {
  std::string id;
  std::string name;
  std::string rawColor;
  GdtfColorCie color;
  int sourceIndex = 0;
};

struct GdtfWheelCatalog {
  std::vector<GdtfWheelInfo> wheels;
  std::vector<GdtfFilterInfo> filters;
  std::vector<GdtfModeDiagnostic> diagnostics;
  const GdtfWheelInfo *FindWheel(const std::string &name) const;
  const GdtfFilterInfo *FindFilter(const std::string &name) const;
};

GdtfWheelCatalog ReadGdtfWheelCatalog(const std::string &descriptionXml);

} // namespace gdtf
