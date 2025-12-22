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
#pragma once

#include <array>
#include <string>
#include <vector>

#include "../print/PageSetup.h"

namespace layouts {

struct Layout2DViewState {
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  float zoom = 1.0f;
  int viewportWidth = 0;
  int viewportHeight = 0;
  int view = 0;

  int renderMode = 2;
  bool darkMode = true;
  bool showGrid = true;
  int gridStyle = 0;
  float gridColorR = 0.35f;
  float gridColorG = 0.35f;
  float gridColorB = 0.35f;
  bool gridDrawAbove = false;

  std::array<bool, 3> showLabelName = {true, true, true};
  std::array<bool, 3> showLabelId = {true, true, true};
  std::array<bool, 3> showLabelDmx = {true, true, true};
  float labelFontSizeName = 3.0f;
  float labelFontSizeId = 2.0f;
  float labelFontSizeDmx = 4.0f;
  std::array<float, 3> labelOffsetDistance = {0.5f, 0.5f, 0.5f};
  std::array<float, 3> labelOffsetAngle = {180.0f, 180.0f, 180.0f};

  std::vector<std::string> hiddenLayers;

  int frameWidth = 0;
  int frameHeight = 0;
};

struct LayoutDefinition {
  std::string name;
  print::PageSetup pageSetup;
  Layout2DViewState view2dState;
  bool hasView2dState = false;
};

class LayoutCollection {
public:
  LayoutCollection();

  const std::vector<LayoutDefinition> &Items() const;
  std::size_t Count() const;

  bool AddLayout(const LayoutDefinition &layout);
  bool RenameLayout(const std::string &currentName,
                    const std::string &newName);
  bool RemoveLayout(const std::string &name);
  bool SetLayoutOrientation(const std::string &name, bool landscape);
  bool UpdateLayout2DViewState(const std::string &name,
                               const Layout2DViewState &state);

  void ReplaceAll(std::vector<LayoutDefinition> layouts);

private:
  static LayoutDefinition DefaultLayout();
  bool NameExists(const std::string &name,
                  const std::string &ignoreName = {}) const;

  std::vector<LayoutDefinition> layouts;
};

} // namespace layouts
