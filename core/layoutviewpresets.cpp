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
#include "layoutviewpresets.h"

#include <algorithm>

namespace {
const std::vector<LayoutViewPreset> kLayoutViewPresets = {
    {
        "3d_layout_view",
        {
            "FileToolbar",
            "LayoutViewsToolbar",
        },
        {
            "LayoutPanel",
            "LayoutViewer",
            "LayoutToolbar",
        },
    },
    {
        "2d_layout_view",
        {
            "FileToolbar",
            "LayoutViewsToolbar",
        },
        {
            "LayoutPanel",
            "LayoutViewer",
            "LayoutToolbar",
        },
    },
    {
        "layout_mode_view",
        {
            "LayoutPanel",
            "LayoutViewer",
            "FileToolbar",
            "LayoutToolbar",
            "LayoutViewsToolbar",
        },
        {
            "3DViewport",
            "2DViewport",
            "2DRenderOptions",
            "DataNotebook",
            "Console",
            "LayerPanel",
            "SummaryPanel",
            "RiggingPanel",
        },
    },
};
} // namespace

const LayoutViewPreset *LayoutViewPresetRegistry::GetPreset(
    const std::string &name) {
  auto it = std::find_if(
      kLayoutViewPresets.begin(), kLayoutViewPresets.end(),
      [&name](const LayoutViewPreset &preset) { return preset.name == name; });
  if (it == kLayoutViewPresets.end())
    return nullptr;
  return &(*it);
}

const std::vector<LayoutViewPreset> &LayoutViewPresetRegistry::GetPresets() {
  return kLayoutViewPresets;
}
