/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "Viewer2DPrintSettings.h"

#include "configmanager.h"

namespace {
constexpr double kMmToPoints = 72.0 / 25.4;
} // namespace

namespace print {

Viewer2DPrintSettings Viewer2DPrintSettings::LoadFromConfig(
    ConfigManager &cfg) {
  Viewer2DPrintSettings settings;
  settings.pageSize = cfg.GetFloat("print_viewer2d_page_size") >= 0.5f
                          ? PageSize::A4
                          : PageSize::A3;
  settings.landscape = cfg.GetFloat("print_viewer2d_landscape") != 0.0f;
  settings.includeGrid = cfg.GetFloat("print_include_grid") != 0.0f;
  settings.detailedFootprints =
      cfg.GetFloat("print_use_simplified_footprints") == 0.0f;
  return settings;
}

void Viewer2DPrintSettings::SaveToConfig(ConfigManager &cfg) const {
  cfg.SetFloat("print_viewer2d_page_size",
               pageSize == PageSize::A4 ? 1.0f : 0.0f);
  cfg.SetFloat("print_viewer2d_landscape", landscape ? 1.0f : 0.0f);
  cfg.SetFloat("print_include_grid", includeGrid ? 1.0f : 0.0f);
  cfg.SetFloat("print_use_simplified_footprints",
               detailedFootprints ? 0.0f : 1.0f);
}

std::pair<double, double> Viewer2DPrintSettings::BasePageSizeMm() const {
  switch (pageSize) {
  case PageSize::A4:
    return {210.0, 297.0};
  case PageSize::A3:
  default:
    return {297.0, 420.0};
  }
}

double Viewer2DPrintSettings::PageWidthPt() const {
  auto [portraitW, portraitH] = BasePageSizeMm();
  const double mm = landscape ? portraitH : portraitW;
  return mm * kMmToPoints;
}

double Viewer2DPrintSettings::PageHeightPt() const {
  auto [portraitW, portraitH] = BasePageSizeMm();
  const double mm = landscape ? portraitW : portraitH;
  return mm * kMmToPoints;
}

} // namespace print
