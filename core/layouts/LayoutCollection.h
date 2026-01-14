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
#include <array>
#include <string>
#include <vector>

#include "../print/PageSetup.h"

namespace layouts {

struct Layout2DViewFrame {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct Layout2DViewCameraState {
  float offsetPixelsX = 0.0f;
  float offsetPixelsY = 0.0f;
  float zoom = 1.0f;
  int viewportWidth = 0;
  int viewportHeight = 0;
  int view = 0;
};

struct Layout2DViewRenderOptions {
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
};

struct Layout2DViewLayers {
  std::vector<std::string> hiddenLayers;
};

struct Layout2DViewDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  Layout2DViewCameraState camera;
  Layout2DViewRenderOptions renderOptions;
  Layout2DViewLayers layers;
};

struct LayoutLegendDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
};

struct LayoutEventTableDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  std::array<std::string, 7> fields;
};

struct LayoutTextDefinition {
  int id = 0;
  int zIndex = 0;
  Layout2DViewFrame frame;
  std::string text;
  std::string richText;
  bool solidBackground = true;
  bool drawFrame = true;
};

struct LayoutDefinition {
  std::string name;
  print::PageSetup pageSetup;
  std::vector<Layout2DViewDefinition> view2dViews;
  std::vector<LayoutLegendDefinition> legendViews;
  std::vector<LayoutEventTableDefinition> eventTables;
  std::vector<LayoutTextDefinition> textViews;
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
  bool UpdateLayout2DView(const std::string &name,
                          const Layout2DViewDefinition &view);
  bool RemoveLayout2DView(const std::string &name, int viewId);
  bool MoveLayout2DView(const std::string &name, int viewId, bool toFront);
  bool UpdateLayoutLegend(const std::string &name,
                          const LayoutLegendDefinition &legend);
  bool RemoveLayoutLegend(const std::string &name, int legendId);
  bool MoveLayoutLegend(const std::string &name, int legendId, bool toFront);
  bool UpdateLayoutEventTable(const std::string &name,
                              const LayoutEventTableDefinition &table);
  bool RemoveLayoutEventTable(const std::string &name, int tableId);
  bool MoveLayoutEventTable(const std::string &name, int tableId,
                            bool toFront);
  bool UpdateLayoutText(const std::string &name,
                        const LayoutTextDefinition &text);
  bool RemoveLayoutText(const std::string &name, int textId);
  bool MoveLayoutText(const std::string &name, int textId, bool toFront);

  void ReplaceAll(std::vector<LayoutDefinition> layouts);

private:
  static LayoutDefinition DefaultLayout();
  bool NameExists(const std::string &name,
                  const std::string &ignoreName = {}) const;

  std::vector<LayoutDefinition> layouts;
};

} // namespace layouts
