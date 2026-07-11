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

#include <string>
#include <vector>

namespace gdtf {

struct GdtfColorCie {
  std::string raw;
  double x = 0.0;
  double y = 0.0;
  double Y = 0.0;
  bool valid = false;
  GdtfValueOrigin origin = GdtfValueOrigin::Unavailable;
  std::vector<GdtfModeDiagnostic> diagnostics;
};

struct GdtfSrgbPreview {
  double red = 0.0;
  double green = 0.0;
  double blue = 0.0;
  bool valid = false;
  bool clipped = false;
  std::vector<GdtfModeDiagnostic> diagnostics;
};

GdtfColorCie ParseGdtfColorCie(const std::string &raw, GdtfValueOrigin origin);
GdtfSrgbPreview ConvertCieXyyToSrgb(const GdtfColorCie &color);

} // namespace gdtf
